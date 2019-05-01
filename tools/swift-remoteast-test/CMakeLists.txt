add_swift_host_tool(swift-remoteast-test
  swift-remoteast-test.cpp
  SWIFT_COMPONENT tools
)
target_link_libraries(swift-remoteast-test
                      PRIVATE
                        swiftFrontendTool
                        swiftRemoteAST)
set_target_properties(swift-remoteast-test PROPERTIES ENABLE_EXPORTS 1)
if(HAVE_UNICODE_LIBEDIT)
  target_link_libraries(swift-remoteast-test PRIVATE edit)
endif()

# If building as part of clang, make sure the headers are installed.
if(NOT SWIFT_BUILT_STANDALONE)
  add_dependencies(swift clang-headers)
endif()

# Add libraries that WALA requires: jvm and cast
set(LIBJVM_PATH "")
set(LIBCAST_PATH "")

# Check for the underlying platform
if(APPLE) # for MacOS X
  set(LIBJVM_PATH "$ENV{JAVA_HOME}/jre/lib/server/libjvm.dylib")
  set(LIBCAST_PATH "$ENV{WALA_HOME}/com.ibm.wala.cast/target/classes/libcast.jnilib")
else() # for Ubuntu
  set(LIBJVM_PATH "$ENV{JAVA_HOME}/jre/lib/amd64/server/libjvm.so")
  set(LIBCAST_PATH "$ENV{WALA_HOME}/com.ibm.wala.cast/target/classes/libcast.so")
endif()

# Link those libraries
add_library(jvm SHARED IMPORTED)
set_property(TARGET jvm PROPERTY IMPORTED_LOCATION ${LIBJVM_PATH})
target_link_libraries(swift-remoteast-test jvm)

add_library(cast SHARED IMPORTED)
set_property(TARGET cast PROPERTY IMPORTED_LOCATION ${LIBCAST_PATH})
target_link_libraries(swift-remoteast-test cast)
