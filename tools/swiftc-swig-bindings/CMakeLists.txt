find_package(SWIG REQUIRED)
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)
include(UseSWIG)
include_directories(${JNI_INCLUDE_DIRS})

set(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR})
set_property(SOURCE swiftc.i PROPERTY CPLUSPLUS ON)

# Build the C++ code into a dynamic library: libswiftc.jnilib 
swig_add_library(
    swiftc 
    LANGUAGE java 
    TYPE MODULE
    SOURCES swiftc.i
)

set_source_files_properties( ${swig_generated_file_fullname} PROPERTIES COMPILE_FLAGS "-DSWIG_COMPILE" )

set_target_properties(${SWIG_MODULE_swiftc_REAL_NAME} PROPERTIES LINK_FLAGS "-L${CMAKE_CURRENT_BINARY_DIR} -L./lib -lswiftFrontend `../llvm-macosx-x86_64/bin/llvm-config --libs --system-libs` -lclangAST -lclangFrontend -lclangCodeGen -lclangTooling -lclangASTMatchers -lLLVMBitReader -lLLVMSystemZDesc -lLLVMBitWriter -lLLVMSystemZDisassembler -lLLVMCodeGen -lLLVMSystemZInfo -lLLVMCore -lLLVMTableGen -lLLVMCoroutines -lLLVMTarget -lLLVMCoverage -lLLVMTransformUtils -lLLVMDebugInfoCodeView -lLLVMVectorize -lLLVMDebugInfoDWARF -lLLVMX86AsmParser -lLLVMDebugInfoMSF -lLLVMX86AsmPrinter -lLLVMDebugInfoPDB -lLLVMX86CodeGen -lLLVMDemangle -lLLVMX86Desc -lLLVMExecutionEngine -lLLVMX86Disassembler -lLLVMGlobalISel -lLLVMX86Info -lLLVMIRReader -lLLVMX86Utils -lLLVMInstCombine -lLLVMXRay -lLLVMInstrumentation -lLLVMipo -lLLVMInterpreter -lLLVMLTO -lLLVMLibDriver -lclangAPINotes -lLLVMLineEditor -lclangARCMigrate -lLLVMLinker -lclangAST -lLLVMMC -lclangASTMatchers -lLLVMMCDisassembler -lclangAnalysis -lLLVMMCJIT -lclangBasic -lLLVMMCParser -lclangCodeGen -lLLVMMIRParser -lclangDriver -lLLVMObjCARCOpts -lclangDynamicASTMatchers -lLLVMObject -lclangEdit -lLLVMObjectYAML -lclangFormat -lLLVMOption -lclangFrontend -lLLVMOrcJIT -lclangFrontendTool -lLLVMAArch64AsmParser -lLLVMPasses -lclangIndex -lLLVMAArch64AsmPrinter -lLLVMPowerPCAsmParser -lclangLex -lLLVMAArch64CodeGen -lLLVMPowerPCAsmPrinter -lclangParse -lLLVMAArch64Desc -lLLVMPowerPCCodeGen -lclangRewrite -lLLVMAArch64Disassembler -lLLVMPowerPCDesc -lclangRewriteFrontend -lLLVMAArch64Info -lLLVMPowerPCDisassembler -lclangSema -lLLVMAArch64Utils -lLLVMPowerPCInfo -lclangSerialization -lLLVMARMAsmParser -lLLVMProfileData -lclangStaticAnalyzerCheckers -lLLVMARMAsmPrinter -lLLVMRuntimeDyld -lclangStaticAnalyzerCore -lLLVMARMCodeGen -lLLVMScalarOpts -lclangStaticAnalyzerFrontend -lLLVMARMDesc -lLLVMSelectionDAG -lclangTooling -lLLVMARMDisassembler -lLLVMSupport -lclangToolingCore -lLLVMARMInfo -lLLVMSymbolize -lgtest -lLLVMAnalysis -lLLVMSystemZAsmParser -lgtest_main -lLLVMAsmParser -lLLVMSystemZAsmPrinter -lLLVMAsmPrinter -lLLVMSystemZCodeGen -lswiftMigrator -lswiftOption -lswiftAST -lswiftParse -lswiftASTSectionImporter -lswiftPrintAsObjC -lswiftBasic -lswiftRemoteAST -lswiftClangImporter -lswiftSIL -lswiftSILGen -lswiftDemangling -lswiftSILOptimizer -lswiftDriver -lswiftSema -lswiftFrontend -lswiftSerialization -lswiftFrontendTool -lswiftSyntax -lswiftIDE -lswiftTBDGen -lswiftIRGen -lswiftImmediate -lswiftIndex -lswiftLLVMPasses -lswiftMarkup")
