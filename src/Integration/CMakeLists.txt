
list(APPEND SourceFiles 
integration/integration.cpp
boundaries/boundaryIntegration.cpp
boundaries/boundaryIntegrationList.cpp 
AdamsBashforth2/AdamsBashforth2.cpp
AdamsBashforth3/AdamsBashforth3.cpp
AdamsBashforth4/AdamsBashforth4.cpp
AdamsBashforth5/AdamsBashforth5.cpp

#AdamsMoulton3/AdamsMoulton3.cpp
#AdamsMoulton4/AdamsMoulton4.cpp
#AdamsMoulton5/AdamsMoulton5.cpp
)

if(pFlow_Build_MPI)
 list(APPEND SourceFiles 
 AdamsBashforth2/processorAB2BoundaryIntegration.cpp)
endif()

set(link_libs Kokkos::kokkos phasicFlow)

pFlow_add_library_install(Integration SourceFiles link_libs)



