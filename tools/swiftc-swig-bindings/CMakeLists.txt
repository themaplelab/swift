find_package(SWIG REQUIRED)
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)
include(UseSWIG)
include_directories(${JNI_INCLUDE_DIRS})

set( CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR} )
set_property(SOURCE swiftc.i PROPERTY CPLUSPLUS ON)

# Build the C++ code into a dynamic library: example.dll (on Windows) or libexample.so (on Linux)
swig_add_library(
    swiftc 
    LANGUAGE java 
    TYPE MODULE
    SOURCES swiftc.i swiftc_hacks.cpp
)

# For convenience we copy the dynamic library to the current build folder
add_custom_command(
    TARGET swiftc 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:swiftc> ${CMAKE_CURRENT_BINARY_DIR}
)

