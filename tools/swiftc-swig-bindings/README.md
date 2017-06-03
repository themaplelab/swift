# swiftc swig bindings

swiftc.i contains the SWIG interface

swiftc_hacks.cpp is where I intend to put cheap workarounds for swig not being able to easily wrap some types

Test.java is a simple java program to test the wrapper

to build, build swift as specified in the top level readme. You can run the java test application with run_cmd.

to do an incremental build of everything in this directory, use build_lib. This is preferred over using utils/build-script as that will rebuild the entire compiler on any change.
