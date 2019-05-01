# Check for the underlying platform
set(JNI_INCLUDE_PATH "")
if(APPLE) # for MacOS X
  set(JNI_INCLUDE_PATH "$ENV{JAVA_HOME}/include/darwin")
else() # for Ubuntu
  set(JNI_INCLUDE_PATH "$ENV{JAVA_HOME}/include/linux")
endif()

add_swift_host_library(swiftFrontendTool STATIC
  FrontendTool.cpp
  ImportedModules.cpp
  ReferenceDependencies.cpp
  TBD.cpp)
add_dependencies(swiftFrontendTool
  swift-syntax-generated-headers
  SwiftOptions)
target_link_libraries(swiftFrontendTool INTERFACE
  clangAPINotes
  clangBasic)
target_link_libraries(swiftFrontendTool PRIVATE
    swiftClangImporter
    swiftDemangling
    swiftDWARFImporter
    swiftFrontend
    swiftIDE
    swiftImmediate
    swiftIndex
    swiftIRGen
    swiftOption
    swiftWALASupport
    clangAPINotes
    clangBasic
    swiftPrintAsObjC
    swiftSerialization
    swiftSIL
    swiftSILGen
    swiftSILOptimizer
    swiftTBDGen
  C_COMPILE_FLAGS
    -I$ENV{WALA_HOME}/com.ibm.wala.cast/source/c/include
    -I$ENV{JAVA_HOME}/include/
    -I${JNI_INCLUDE_PATH}
  LINK_FLAGS
    -L$ENV{WALA_HOME}/com.ibm.wala.cast/target/classes -lcast
)
