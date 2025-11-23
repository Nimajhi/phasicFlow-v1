#include "forceChain.hpp"
#include "systemControl.hpp"
#include "dictionary.hpp"
#include "types.hpp"

namespace pFlow
{

forceChain::forceChain(
    systemControl& control, 
    dynamicPointStructure& dynPointStruct
)
 : forceChainActive_(false),
    forceChainFCn_(nullptr),
    forceChainDist_(nullptr), 
    forceChainPairs_(nullptr),
    pairCounter_(nullptr)

{

}

forceChain::~forceChain()
{
    // Clean up allocated fields
    
}

void forceChain::zeroFCn()
{
    if (!forceChainActive_ || !forceChainFCn_) return;
    forceChainFCn_->fill(zero3);  // Changed . to ->
}

void forceChain::zeroDist()
{
    if (!forceChainActive_ || !forceChainDist_) return;
    forceChainDist_->fill(zero3);  // Changed . to ->
}

void forceChain::zeroPairs()
{
    if (!forceChainActive_ || !forceChainPairs_) return;
    forceChainPairs_->fill(zero3);  // Changed . to ->
}

void forceChain::zeroAll()
{
    if (!forceChainActive_) return;
    zeroFCn();
    zeroDist();
    zeroPairs();
    if (pairCounter_) pairCounter_->fill(0);  // Changed . to ->
}

void forceChain::resetPairCounter()
{
    if (!forceChainActive_ || !pairCounter_) return;
    Kokkos::deep_copy(pairCounter_->deviceViewAll(), 0u);  // Changed . to ->
}

bool forceChain::initializeFromDict(const dictionary& modelDict)
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


void forceChain::activateForceChain(systemControl& control, dynamicPointStructure& dynPointStruct)
{
     if (!forceChainActive_)
        return;
    {

     if (!forceChainFCn_) // Create the fields only when activating
        forceChainFCn_ = makeUnique<realx3PointField_D>(
            objectFile(
                "forceChainFCn",
                "",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        forceChainDist_ = makeUnique<realx3PointField_D>(
            objectFile(
                "distance",
                "",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        forceChainPairs_ = makeUnique<realx3PointField_D>(
            objectFile(
                "pairs",
                "",
                objectFile::READ_IF_PRESENT,
                objectFile::WRITE_ALWAYS
            ),
            dynPointStruct,
            zero3
        );
        
        pairCounter_ = makeUnique<uint32PointField_D>(
            objectFile(
                "pairCount",
                "",
                objectFile::READ_NEVER,
                objectFile::WRITE_NEVER
            ),
            dynPointStruct,
            0
        );
    }
}

bool forceChain::isActive() const 
{
    return forceChainActive_;
}
    
    



} // namespace pFlow
