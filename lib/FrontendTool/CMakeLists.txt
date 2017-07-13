add_swift_library(swiftFrontendTool STATIC
  FrontendTool.cpp
  ImportedModules.cpp
  ReferenceDependencies.cpp
  TBD.cpp
  DEPENDS SwiftOptions
  LINK_LIBRARIES
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
  LINK_FLAGS
    -L$ENV{WALA_HOME}/com.ibm.wala.cast/target/classes -lcast
)
