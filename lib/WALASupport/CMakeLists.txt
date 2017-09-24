if (SWIFT_FORCE_OPTIMIZED_TYPECHECKER)
  set(EXTRA_TYPECHECKER_FLAGS "FORCE_BUILD_OPTIMIZED")
endif()

add_swift_library(swiftWALASupport STATIC
  WALAWalker.cpp
  LINK_LIBRARIES
    swiftParse
    swiftSema
    swiftAST
  C_COMPILE_FLAGS
    -I$ENV{WALA_HOME}/com.ibm.wala.cast/source/c/include
    -I$ENV{JAVA_HOME}/include/
    -I$ENV{JAVA_HOME}/include/darwin
  ${EXTRA_TYPECHECKER_FLAGS}
)