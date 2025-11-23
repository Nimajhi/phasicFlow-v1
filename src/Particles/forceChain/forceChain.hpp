#ifndef __forceChain_hpp__
#define __forceChain_hpp__

#include "dynamicPointStructure.hpp"
#include "objectFile.hpp"
#include "systemControl.hpp"
#include "dictionary.hpp"

namespace pFlow
{

class forceChain
{
private:

     bool        forceChainActive_ = false;
    
    /// forceChain force field
    uniquePtr<realx3PointField_D> forceChainFCn_;

        
    /// Distance Colliding Particles
    uniquePtr<realx3PointField_D> forceChainDist_;
    
    /// Pairs of Colliding Particles  
    uniquePtr<realx3PointField_D> forceChainPairs_;
    
    /// pair Counter
    uniquePtr<uint32PointField_D> pairCounter_;

public:

    forceChain(
        systemControl& control, 
        dynamicPointStructure& dynPointStruct
    );
    
    // Destructor
    ~forceChain();

    
    // Member functions
    void zeroFCn();
    void zeroDist();
    void zeroPairs();
    void zeroAll();
    
    // Initialize force chain from dictionary
    bool initializeFromDict(const dictionary& modelDict);
    
    // Reset pair counter (used before sphere-sphere interaction)
    void resetPairCounter();

    // Activate/deactivate force chain
    void activateForceChain(systemControl& control, dynamicPointStructure& dynPointStruct);
  
    // Getters
    inline auto& forceChainFCn()
    {
        return *forceChainFCn_;
    }

    inline const auto& forceChainFCn() const
    {
        return *forceChainFCn_;
    }
        
    inline auto& forceChainDist()
    {
        return *forceChainDist_;
    }

    inline const auto& forceChainDist() const
    {
        return *forceChainDist_;
    }
	
    inline auto& forceChainPairs()
    {
        return *forceChainPairs_;
    }

    inline const auto& forceChainPairs() const
    {
        return *forceChainPairs_;
    }
	
    inline auto& pairCounter()
    {
        return *pairCounter_;
    }

    inline const auto& pairCounter() const
    {
        return *pairCounter_;
    }
    
    bool forceChainActive() const 
    {
        return forceChainActive_; 
    }

   inline bool hasForceChainFCn() const { return forceChainActive_ && forceChainFCn_; }
    inline bool hasForceChainDist() const { return forceChainActive_ && forceChainDist_; }
    inline bool hasForceChainPairs() const { return forceChainActive_ && forceChainPairs_; }
    inline bool hasPairCounter() const { return forceChainActive_ && pairCounter_; }
    
  bool isActive() const;
    ////
  

}; // forceChain

} // namespace pFlow

#endif //__forceChain_hpp__
