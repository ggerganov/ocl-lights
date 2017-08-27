add_library(${CG_OPENCL_LIB}
    oclBaseManager.cpp
    oclCommon.cpp
    oclProfiler.cpp
    renderer.cpp
    )

target_link_libraries(${CG_OPENCL_LIB}
    ${OPENCL_LIBRARIES}
    )
