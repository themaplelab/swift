public class Test {
    public static void main(String args[]) {
        System.loadLibrary("swiftc");
        CompilerInvocation invocation = new CompilerInvocation();
        invocation.setModuleName(swiftc.make_stringref("test"));
        invocation.addInputFilename(swiftc.make_stringref("test.swift"));

        CompilerInstance instance = new CompilerInstance();
        //PrintingDiagnosticConsumer printDiags = new PrintingDiagnosticConsumer();
        //instance.addDiagnosticConsumer(printDiags);
        instance.setup(invocation);
        instance.performSema(); // Perform parsing
        if (instance.getASTContext().hadError())
            System.out.println("Error");
    }
}
