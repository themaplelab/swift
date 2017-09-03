add_swift_library(swiftFrontendTool STATIC
  FrontendTool.cpp
  ImportedModules.cpp
  ReferenceDependencies.cpp
  TBD.cpp
  DEPENDS
    swift-syntax-generated-headers SwiftOptions
  LINK_LIBRARIES
    swiftIndex
    swiftIDE
    swiftTBDGen swiftIRGen swiftSIL swiftSILGen swiftSILOptimizer
    swiftDemangling
    swiftImmediate
    swiftSerialization
    swiftPrintAsObjC
    swiftFrontend
    swiftClangImporter
    swiftOption
    swiftWALASupport
    clangAPINotes
    clangBasic
  C_COMPILE_FLAGS
    -I$ENV{WALA_HOME}/com.ibm.wala.cast/source/c/include
    -I$ENV{JAVA_HOME}/include/
    -I$ENV{JAVA_HOME}/include/darwin 
  LINK_FLAGS
    -L$ENV{WALA_HOME}/com.ibm.wala.cast/target/classes -lcast
)
