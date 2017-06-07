public class Test {
    public static void main(String args[]) {
        System.loadLibrary("swiftc");
        swiftc.initialize_llvm(args.length, args);
        CompilerInvocation invocation = new CompilerInvocation();
        CompilerInstance instance = new CompilerInstance();
        //invocation.parseArgs(swiftc.make_string_arrayref(new String[]{"foo"}, 1), instance.getDiags());
        invocation.setModuleName(swiftc.make_stringref("test"));
        invocation.addInputFilename(swiftc.make_stringref("test.swift"));
        //invocation.setSerializedDiagnosticsPath(swiftc.make_stringref("/tmp/diag"));

        instance.addDiagnosticConsumer(swiftc.make_BasicDiagnosticConsumer());
        instance.setup(invocation);
        instance.performSema(); // Perform parsing
        if (instance.getASTContext().hadError())
            System.out.println("Error");
    }
}
