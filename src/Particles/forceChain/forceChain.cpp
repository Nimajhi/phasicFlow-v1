#include "forceChain.hpp"


pFlow::forceChain::forceChain(
    systemControl& control,
    dynamicPointStructure& dynPointStruct
)
:



  forceChainActive_(false)
   
{

}
pFlow::forceChain::~forceChain()
{
    forceChainFCn_.reset();
    forceChainDist_.reset();
    forceChainPairs_.reset();
    pairCounter_.reset();
    
}

void pFlow::forceChain::zeroFCn()
{
    if (!forceChainActive_ || !forceChainFCn_) return;
    forceChainFCn_->fill(zero3);  
}

void pFlow::forceChain::zeroDist()
{
    if (!forceChainActive_ || !forceChainDist_) return;
    forceChainDist_->fill(zero3);  
}

void pFlow::forceChain::zeroPairs()
{
    if (!forceChainActive_ || !forceChainPairs_) return;
    forceChainPairs_->fill(zero3);  
}

void pFlow::forceChain::zeroAll()
{
    if (!forceChainActive_) return;
    zeroFCn();
    zeroDist();
    zeroPairs();
    if (pairCounter_) pairCounter_->fill(0);  
}

void pFlow::forceChain::resetPairCounter()
{
    if (!forceChainActive_ || !pairCounter_) return;
    Kokkos::deep_copy(pairCounter_->deviceViewAll(), 0u);  
}

bool pFlow::forceChain::initializeFromDict(const dictionary& modelDict)
{
    word forceChainVal = "No";
    forceChainVal = modelDict.getValOrSet<word>("forceChain", forceChainVal);

    if (forceChainVal == "Yes" || forceChainVal == "yes" || 
        forceChainVal == "True"|| forceChainVal == "true" )
    {
        forceChainActive_ = true;
        REPORT(1) << "ForceChain is " << Yellow_Text("active")
                  << " in this simulation." << END_REPORT;
    }
    else
    {
        forceChainActive_ = false;
        REPORT(1) << "ForceChain is " << Yellow_Text("inactive")
                  << " in this simulation." << END_REPORT;
    }
    
    return true;
}


void pFlow::forceChain::activateForceChain(systemControl& control, dynamicPointStructure& dynPointStruct)
{
     if (!forceChainActive_)
        return;
    {

     if (!forceChainFCn_) // Create the fields only when activating
        forceChainFCn_ = makeUnique<realx3PointField_D>(
            objectFile(
                "forceChainFCn",
                "forceChain",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        forceChainDist_ = makeUnique<realx3PointField_D>(
            objectFile(
                "pos",
                "forceChain",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        forceChainPairs_ = makeUnique<realx3PointField_D>(
            objectFile(
                "pairs",
                "forceChain",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        pairCounter_ = makeUnique<uint32PointField_D>(
            objectFile(
                "pairCount",
                "forceChain",
                objectFile::READ_NEVER,
                objectFile::WRITE_NEVER
            ),
            dynPointStruct,
            0
        );
    }
}

void pFlow::forceChain::addInteraction(uint32 i, uint32 j, const realx3& FCn, const realx3& xi, const realx3& xj)
{
    if (!forceChainActive_ || !forceChainFCn_ || !forceChainDist_ || !forceChainPairs_ || !pairCounter_) 
        return;
    
    // Get device views
    auto& fcnView = forceChainFCn_->deviceViewAll();
    auto& distView = forceChainDist_->deviceViewAll();
    auto& pairsView = forceChainPairs_->deviceViewAll();
    auto& counterView = pairCounter_->deviceViewAll();
    
    // 1. Add forces (atomic operations)
    Kokkos::atomic_add(&fcnView(i).x_, FCn.x_);
    Kokkos::atomic_add(&fcnView(i).y_, FCn.y_);
    Kokkos::atomic_add(&fcnView(i).z_, FCn.z_);

    Kokkos::atomic_add(&fcnView(j).x_, -FCn.x_);
    Kokkos::atomic_add(&fcnView(j).y_, -FCn.y_);
    Kokkos::atomic_add(&fcnView(j).z_, -FCn.z_);
    
    // 2. Add distances (atomic operations)
    Kokkos::atomic_add(&distView(i), xi);
    Kokkos::atomic_add(&distView(j), xj);
    
    // 3. Add pair (only once per unique pair, i < j)
    if (i < j)
    {
        real fcn_mag = length(FCn);
        
        // Get a unique index using atomic counter
        uint32_t pairIndex = Kokkos::atomic_fetch_add(&counterView(0), uint32_t(1));
        
        // Bounds check before writing
        const uint32_t maxPairs = static_cast<uint32_t>(pairsView.extent(0));
        if (pairIndex < maxPairs)
        {
            // Single write of the tuple (i,j,fcn_mag)
            pairsView(pairIndex) = realx3(real(i), real(j), fcn_mag);
        }
    }
}
bool pFlow::forceChain::isActive() const 
{
    return forceChainActive_;
}
    
    



 
