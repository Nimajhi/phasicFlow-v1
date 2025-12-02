// makeForceChainCSV.cpp - Minimal version
// Compile: g++ -std=c++17 -O2 -o makeForceChainCSV makeForceChainCSV.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct Vec3 { double x, y, z; };
struct ForcePair { int i, j; double force; };

bool isNumeric(const std::string& s) {
    try { std::stod(s); return true; } catch (...) { return false; }
}

fs::path findFile(const fs::path& folder, const std::string& pattern) {
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
        std::string p = pattern;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        if (fname.find(p) != std::string::npos) return entry.path();
    }
    return "";
}

std::vector<ForcePair> parsePairs(const fs::path& path) {
    std::vector<ForcePair> pairs;
    std::ifstream file(path);
    if (!file.is_open()) return pairs;
    
    std::string line;
    bool inData = false;
    while (getline(file, line)) {
        if (line.find("internalField") != std::string::npos || 
            line.find("ineternalField") != std::string::npos) {
            inData = true;
            continue;
        }
        if (!inData) continue;
        
        if (line.find(')') != std::string::npos && line.find('(') == std::string::npos) break;
        
        std::string clean;
        for (char c : line) {
            if (c == '(' || c == ')' || c == ';') clean += ' ';
            else clean += c;
        }
        
        std::stringstream ss(clean);
        int i, j; double f;
        if (ss >> i >> j >> f) pairs.push_back({i, j, f});
    }
    return pairs;
}

std::vector<Vec3> parsePositions(const fs::path& path) {
    std::vector<Vec3> positions;
    std::ifstream file(path);
    if (!file.is_open()) return positions;
    
    std::string line;
    bool inData = false;
    int count = 0;
    while (getline(file, line)) {
        if (line.find("internalField") != std::string::npos || 
            line.find("ineternalField") != std::string::npos) {
            inData = true;
            continue;
        }
        if (!inData) continue;
        
        if (line.find(')') != std::string::npos && line.find('(') == std::string::npos) break;
        
        std::string clean;
        for (char c : line) {
            if (c == '(' || c == ')' || c == ';') clean += ' ';
            else clean += c;
        }
        
        std::stringstream ss(clean);
        double x, y, z;
        if (ss >> x >> y >> z) {
            positions.push_back({x, y, z});
            count++;
        }
    }
    return positions;
}

void processTimestep(const fs::path& folder, const fs::path& outDir) {
    std::cout << "\nProcessing: " << folder.filename().string() << std::endl;
    
    fs::path forceDir = folder / "forceChain";
    if (!fs::exists(forceDir)) return;
    
    fs::path pairsFile = findFile(forceDir, "pairs");
    fs::path posIFile = findFile(forceDir, "posI");
    fs::path posJFile = findFile(forceDir, "posJ");
    
    if (pairsFile.empty() || posIFile.empty() || posJFile.empty()) return;
    
    auto pairs = parsePairs(pairsFile);
    auto posI = parsePositions(posIFile);
    auto posJ = parsePositions(posJFile);
    
    if (pairs.empty() || posI.empty() || posJ.empty()) return;
    
    fs::path csvFile = outDir / ("forceChain_" + folder.filename().string() + ".csv");
    std::ofstream out(csvFile);
    out << "i,j,xi,yi,zi,xj,yj,zj,forceMag\n";
    
    int valid = 0;
    for (const auto& p : pairs) {
        if (p.i < 0 || p.i >= posI.size() || p.j < 0 || p.j >= posJ.size()) continue;
        
        const Vec3& pi = posI[p.i];
        const Vec3& pj = posJ[p.j];
        if ((pi.x == 0 && pi.y == 0 && pi.z == 0) || (pj.x == 0 && pj.y == 0 && pj.z == 0)) continue;
        
        out << p.i << "," << p.j << ","
            << pi.x << "," << pi.y << "," << pi.z << ","
            << pj.x << "," << pj.y << "," << pj.z << ","
            << p.force << "\n";
        valid++;
    }
    
    std::cout << "  Pairs: " << pairs.size() << ", Valid: " << valid 
              << ", File: " << csvFile.filename().string() << std::endl;
}

int main() {
    fs::path outDir = fs::current_path() / "forceChainCSV";
    if (!fs::exists(outDir)) fs::create_directory(outDir);
    
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (!entry.is_directory()) continue;
        if (isNumeric(entry.path().filename().string())) {
            processTimestep(entry.path(), outDir);
        }
    }
    
    std::cout << "\nDone! Files in: " << outDir.string() << std::endl;
    return 0;
}