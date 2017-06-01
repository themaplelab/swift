# swiftc swig bindings

swiftc.i contains the SWIG interface

swiftc_hacks.cpp is where I intend to put cheap workarounds for swig not being able to easily wrap some types

Test.java is a simple java program to test the wrapper

to build, build swift as specified in the top level readme, then follow up with running link_cmd and run_cmd (those two scripts are temporary hacks and need to be integrated into CMakeLists.txt)

to force an incremental build, I use utils/build-script --preset=buildbot_incremental_base
