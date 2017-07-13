add_swift_host_tool(swift-remoteast-test
  swift-remoteast-test.cpp
  LINK_LIBRARIES
    swiftFrontendTool
    swiftRemoteAST
  SWIFT_COMPONENT tools
)

set_target_properties(swift-remoteast-test PROPERTIES ENABLE_EXPORTS 1)
if(HAVE_UNICODE_LIBEDIT)
  target_link_libraries(swift-remoteast-test edit)
endif()

# If building as part of clang, make sure the headers are installed.
if(NOT SWIFT_BUILT_STANDALONE)
  add_dependencies(swift clang-headers)
endif()

add_library(jvm SHARED IMPORTED) 
set_property(TARGET jvm PROPERTY IMPORTED_LOCATION /Library/Java/JavaVirtualMachines/jdk1.8.0_131.jdk/Contents/Home/jre/lib/server/libjvm.dylib) 
target_link_libraries(swift-remoteast-test jvm)

add_library(cast SHARED IMPORTED) 
set_property(TARGET cast PROPERTY IMPORTED_LOCATION $ENV{WALA_HOME}/com.ibm.wala.cast/target/classes/libcast.jnilib) 
target_link_libraries(swift-remoteast-test cast)
