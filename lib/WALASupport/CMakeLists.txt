if (SWIFT_FORCE_OPTIMIZED_TYPECHECKER)
  set(EXTRA_TYPECHECKER_FLAGS "FORCE_BUILD_OPTIMIZED")
endif()

# Check for the underlying platform
set(JNI_INCLUDE_PATH "")
if(APPLE) # for MacOS X
  set(JNI_INCLUDE_PATH "$ENV{JAVA_HOME}/include/darwin")
else() # for Ubuntu
  set(JNI_INCLUDE_PATH "$ENV{JAVA_HOME}/include/linux")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_swift_library(swiftWALASupport STATIC
  WALAWalker.cpp
  BasicBlockLabeller.cpp
  SymbolTable.cpp
  SILWalaInstructionVisitor.cpp
  LINK_LIBRARIES
    swiftParse
    swiftSema
    swiftAST
  C_COMPILE_FLAGS
    -I$ENV{WALA_HOME}/com.ibm.wala.cast/source/c/include
    -I$ENV{JAVA_HOME}/include/
    -I${JNI_INCLUDE_PATH}
  ${EXTRA_TYPECHECKER_FLAGS}
)
