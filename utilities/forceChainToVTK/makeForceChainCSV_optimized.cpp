// makeForceChainCSV_optimized.cpp
// Compile: g++ -std=c++17 -O3 -march=native -flto -funroll-loops -fopenmp -o makeForceChainCSV makeForceChainCSV.cpp
// Run: ./makeForceChainCSV [--threads N]

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <charconv>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <mutex>
#include <iomanip>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace fs = std::filesystem;

// High-performance timer for profiling
class Timer {
    std::chrono::high_resolution_clock::time_point start;
public:
    Timer() : start(std::chrono::high_resolution_clock::now()) {}
    double elapsed() const {
        return std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - start).count();
    }
};

// Optimized Vec3 structure (packed, no padding)
#pragma pack(push, 1)
struct Vec3 {
    double x, y, z;
    
    Vec3() noexcept : x(0), y(0), z(0) {}
    Vec3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}
    
    bool isZero() const noexcept {
        return x == 0.0 && y == 0.0 && z == 0.0;
    }
};
#pragma pack(pop)

// ForcePair for efficient cache usage
struct ForcePair {
    int32_t i, j;
    double force;
    
    ForcePair(int32_t i_ = 0, int32_t j_ = 0, double f_ = 0.0) noexcept 
        : i(i_), j(j_), force(f_) {}
};

// Memory-mapped file reader for maximum I/O performance
class MemoryMappedFile {
    std::ifstream file;
    std::string content;
    
public:
    MemoryMappedFile(const fs::path& filepath) {
        file.open(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return;
        
        size_t size = file.tellg();
        file.seekg(0);
        content.resize(size);
        file.read(&content[0], size);
        file.close();
    }
    
    const char* data() const noexcept { return content.data(); }
    size_t size() const noexcept { return content.size(); }
    bool valid() const noexcept { return !content.empty(); }
};

// Fast parser for phasicFlow files
class FastPhasicFlowParser {
    
public:
    // Parse file directly from memory with maximum speed
    std::vector<ForcePair> parsePairs(const fs::path& filepath) {
        MemoryMappedFile mmap(filepath);
        if (!mmap.valid()) return {};
        
        const char* p = mmap.data();
        const char* end = p + mmap.size();
        
        // Skip to internalField
        while (p < end && !(*p == 'i' && strncmp(p, "internalField", 13) == 0)) {
            ++p;
        }
        
        if (p >= end) return {};
        
        // Skip "internalField"
        while (p < end && *p != '\n') ++p;
        if (p >= end) return {};
        ++p; // Skip newline
        
        // Skip whitespace
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
        
        // Parse total particles
        int total_particles = 0;
        const char* num_start = p;
        while (p < end && *p >= '0' && *p <= '9') {
            total_particles = total_particles * 10 + (*p - '0');
            ++p;
        }
        
        // Skip to data start
        while (p < end && *p != '(') ++p;
        
        // Pre-allocate for typical sizes (adjust based on your data)
        std::vector<ForcePair> pairs;
        pairs.reserve(total_particles > 0 ? total_particles : 100000);
        
        // Parse pairs with minimal allocations
        while (p < end) {
            // Skip to next '('
            while (p < end && *p != '(') {
                if (*p == ';' && p + 1 < end && *(p + 1) == ')') {
                    p = end; // End of data
                    break;
                }
                ++p;
            }
            if (p >= end) break;
            
            ++p; // Skip '('
            
            // Parse i
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            int i = 0;
            while (p < end && *p >= '0' && *p <= '9') {
                i = i * 10 + (*p - '0');
                ++p;
            }
            
            // Parse j
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            int j = 0;
            while (p < end && *p >= '0' && *p <= '9') {
                j = j * 10 + (*p - '0');
                ++p;
            }
            
            // Parse force
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            
            // Fast double parsing
            const char* force_start = p;
            while (p < end && *p != ')' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                ++p;
            }
            
            double force = 0.0;
            auto [ptr, ec] = std::from_chars(force_start, p, force);
            if (ec != std::errc()) {
                // Fallback parsing
                std::string force_str(force_start, p - force_start);
                try {
                    force = std::stod(force_str);
                } catch (...) {
                    force = 0.0;
                }
            }
            
            pairs.emplace_back(i, j, force);
            
            // Skip to next line or end
            while (p < end && *p != '\n' && *p != '(') ++p;
        }
        
        return pairs;
    }
    
    // Parse positions file
    std::vector<Vec3> parsePositions(const fs::path& filepath) {
        MemoryMappedFile mmap(filepath);
        if (!mmap.valid()) return {};
        
        const char* p = mmap.data();
        const char* end = p + mmap.size();
        
        // Skip to internalField
        while (p < end && !(*p == 'i' && strncmp(p, "internalField", 13) == 0)) {
            ++p;
        }
        
        if (p >= end) return {};
        
        // Skip "internalField"
        while (p < end && *p != '\n') ++p;
        if (p >= end) return {};
        ++p;
        
        // Parse total particles
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
        int total_particles = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            total_particles = total_particles * 10 + (*p - '0');
            ++p;
        }
        
        // Skip to data start
        while (p < end && *p != '(') ++p;
        
        std::vector<Vec3> positions;
        positions.reserve(total_particles);
        
        // Parse positions
        int parsed = 0;
        while (p < end && parsed < total_particles) {
            // Skip to '('
            while (p < end && *p != '(') {
                if (*p == ';' && p + 1 < end && *(p + 1) == ')') {
                    break;
                }
                ++p;
            }
            if (p >= end || (*p == ';' && p + 1 < end && *(p + 1) == ')')) break;
            
            ++p; // Skip '('
            
            // Parse x, y, z
            double x = 0, y = 0, z = 0;
            
            // Parse x
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            const char* x_start = p;
            while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ')') ++p;
            std::from_chars(x_start, p, x);
            
            // Parse y
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            const char* y_start = p;
            while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ')') ++p;
            std::from_chars(y_start, p, y);
            
            // Parse z
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            const char* z_start = p;
            while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ')') ++p;
            std::from_chars(z_start, p, z);
            
            positions.emplace_back(x, y, z);
            parsed++;
            
            // Skip to next
            while (p < end && *p != '\n' && *p != '(') ++p;
        }
        
        // Ensure we have exactly total_particles entries
        if (positions.size() < static_cast<size_t>(total_particles)) {
            positions.resize(total_particles, Vec3(0, 0, 0));
        }
        
        return positions;
    }
};

// Parallel CSV writer with buffering
class ParallelCSVWriter {
    std::ofstream file;
    std::mutex write_mutex;
    
public:
    ParallelCSVWriter(const fs::path& filepath) {
        file.open(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open output file");
        }
        file << "i,j,xi,yi,zi,xj,yj,zj,forceMag\n";
    }
    
    void writeRowFast(int32_t i, int32_t j, 
                      double xi, double yi, double zi,
                      double xj, double yj, double zj,
                      double force) {
        // Fast writing with sprintf
        thread_local char buffer[256];
        int len = snprintf(buffer, sizeof(buffer),
                          "%d,%d,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
                          i, j, xi, yi, zi, xj, yj, zj, force);
        
        std::lock_guard<std::mutex> lock(write_mutex);
        file.write(buffer, len);
    }
};

// Optimized timestep processor
void processTimestepOptimized(const fs::path& time_folder, const fs::path& output_dir, 
                              FastPhasicFlowParser& parser) {
    std::string time_name = time_folder.filename().string();
    
    // Find forceChain folder
    fs::path forceChainFolder = time_folder / "forceChain";
    if (!fs::exists(forceChainFolder)) return;
    
    // Find files (fast, single pass)
    fs::path pairsFile, posIFile, posJFile;
    for (const auto& entry : fs::directory_iterator(forceChainFolder)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        if (name.find("pairs") != std::string::npos) pairsFile = entry.path();
        else if (name.find("posi") != std::string::npos && name.find("posj") == std::string::npos) 
            posIFile = entry.path();
        else if (name.find("posj") != std::string::npos) 
            posJFile = entry.path();
    }
    
    if (pairsFile.empty() || posIFile.empty() || posJFile.empty()) return;
    
    // Parse files
    auto pairs = parser.parsePairs(pairsFile);
    auto positions_i = parser.parsePositions(posIFile);
    auto positions_j = parser.parsePositions(posJFile);
    
    if (pairs.empty() || positions_i.empty() || positions_j.empty()) return;
    
    // Create CSV writer
    fs::path csv_file = output_dir / ("forceChain_" + time_name + ".csv");
    ParallelCSVWriter writer(csv_file);
    
    size_t valid_count = 0;
    
    // Process pairs
    for (size_t idx = 0; idx < pairs.size(); ++idx) {
        const auto& pair = pairs[idx];
        
        // Check bounds
        if (pair.i < 0 || static_cast<size_t>(pair.i) >= positions_i.size() ||
            pair.j < 0 || static_cast<size_t>(pair.j) >= positions_j.size()) {
            continue;
        }
        
        const Vec3& pos_i = positions_i[pair.i];
        const Vec3& pos_j = positions_j[pair.j];
        
        // Skip zero positions
        if (pos_i.isZero() || pos_j.isZero()) {
            continue;
        }
        
        writer.writeRowFast(pair.i, pair.j, 
                           pos_i.x, pos_i.y, pos_i.z,
                           pos_j.x, pos_j.y, pos_j.z,
                           pair.force);
        valid_count++;
    }
    
    std::cout << time_name << ": " << valid_count << "/" << pairs.size() << " pairs" << std::endl;
}

// Main function with progress reporting
int main(int argc, char* argv[]) {
    Timer total_timer;
    
    // Parse command line arguments
    int num_threads = std::thread::hardware_concurrency();
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--threads" && i + 1 < argc) {
            num_threads = std::atoi(argv[i + 1]);
            ++i;
        }
    }
    
    std::cout << "Using " << num_threads << " threads" << std::endl;
    
    fs::path root = fs::current_path();
    std::cout << "Processing directory: " << root.string() << std::endl;
    
    // Collect timestep folders
    std::vector<fs::path> time_folders;
    
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) continue;
        
        const std::string& name = entry.path().filename().string();
        // Check if name is numeric
        bool is_numeric = !name.empty();
        for (char c : name) {
            if (!std::isdigit(c) && c != '.' && c != '-') {
                is_numeric = false;
                break;
            }
        }
        if (is_numeric) {
            time_folders.push_back(entry.path());
        }
    }
    
    if (time_folders.empty()) {
        std::cerr << "No timestep folders found!" << std::endl;
        return 1;
    }
    
    // Sort
    std::sort(time_folders.begin(), time_folders.end(),
        [](const fs::path& a, const fs::path& b) {
            try {
                return std::stod(a.filename().string()) < std::stod(b.filename().string());
            } catch (...) {
                return a.filename().string() < b.filename().string();
            }
        });
    
    std::cout << "Found " << time_folders.size() << " timestep folders" << std::endl;
    
    // Create output directory
    fs::path output_dir = root / "forceChainCSV";
    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }
    
    // Process folders in parallel
    std::atomic<size_t> processed_count{0};
    size_t total_folders = time_folders.size();
    
    // Process each folder - OpenMP parallel loop
    #pragma omp parallel for schedule(dynamic) num_threads(num_threads)
    for (size_t i = 0; i < time_folders.size(); ++i) {
        try {
            // Each thread gets its own parser to avoid contention
            FastPhasicFlowParser parser;
            processTimestepOptimized(time_folders[i], output_dir, parser);
            
            // Update progress counter
            ++processed_count;
            
            // Show progress every 10 folders (thread-safe with atomic)
            if (processed_count % 10 == 0) {
                // Only one thread prints progress
                #pragma omp critical(progress)
                {
                    std::cout << "Progress: " << processed_count << "/" 
                              << total_folders << std::endl;
                }
            }
        } catch (const std::exception& e) {
            #pragma omp critical(error)
            {
                std::cerr << "Error processing " << time_folders[i].filename().string()
                          << ": " << e.what() << std::endl;
            }
        }
    }
    
    double total_time = total_timer.elapsed();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "PROCESSING COMPLETE" << std::endl;
    std::cout << "Total time: " << total_time << " seconds" << std::endl;
    std::cout << "Throughput: " << (time_folders.size() / total_time) << " folders/second" << std::endl;
    std::cout << "Output directory: " << output_dir.string() << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}