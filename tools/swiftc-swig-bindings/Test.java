public class Test {
    public static void main(String args[]) {
        System.loadLibrary("swiftc");
        swiftc.initialize_llvm(args.length, args);
        CompilerInvocation invocation = new CompilerInvocation();
        CompilerInstance instance = new CompilerInstance();
        
        // setup invocation
        //invocation.parseArgs(swiftc.make_string_arrayref(new String[]{"foo"}, 1), instance.getDiags());
        invocation.setModuleName(swiftc.make_stringref("Test"));
        invocation.setMainExecutablePath(swiftc.make_stringref("test"));
        invocation.addInputFilename(swiftc.make_stringref("test.swift"));
        invocation.setSDKPath("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk");
        invocation.setTargetTriple(swiftc.make_stringref("x86_64-apple-macosx10.12.4"));
        //invocation.setParseStdlib();

        // This is necessary to get error messages
        instance.addDiagnosticConsumer(swiftc.make_BasicDiagnosticConsumer());

        if (instance.setup(invocation))
            System.out.println("Setup error");

        // parse + typecheck
        instance.performSema();
        if (instance.getASTContext().hadError())
            System.out.println("Parse error");
        System.out.println(instance.getPrimarySourceFile());
    }
}
