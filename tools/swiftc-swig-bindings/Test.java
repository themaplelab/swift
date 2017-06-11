public class Test {
    public static void main(String args[]) {
        System.loadLibrary("swiftc");
        swiftc.initialize_llvm(args.length, args);
        CompilerInvocation invocation = new CompilerInvocation();
        CompilerInstance instance = new CompilerInstance();
        
        // setup invocation
        //invocation.parseArgs(swiftc.make_string_arrayref(new String[]{"foo"}, 1), instance.getDiags());
        invocation.addInputFilename(swiftc.make_stringref("test.swift"));
        invocation.setModuleName(swiftc.make_stringref("main"));
        invocation.setMainExecutablePath(swiftc.make_stringref("test"));
        invocation.setSDKPath("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk");
        invocation.setTargetTriple(swiftc.make_stringref("x86_64-apple-macosx10.12.4"));
        invocation.setParseStdlib();

        // This is necessary to get error messages
        instance.addDiagnosticConsumer(swiftc.make_BasicDiagnosticConsumer());

        if (instance.setup(invocation))
            System.out.println("Setup error");

        instance.performParseOnly();
        if (instance.getASTContext().hadError())
            System.out.println("Parse error");
        System.out.println("Main source file: " + instance.getMainModule().getMainSourceFile(SourceFileKind.Main));
        System.out.println("Have SIL module? " + instance.getSILModule());
        ASTWalker walker = new ASTWalker() {
            @Override
            public boolean walkToDeclPre(SWIGTYPE_p_swift__Decl s) {
                System.out.println("Visiting decl: " + s.toString());
                return true;
            }
            @Override
            public SWIGTYPE_p_swift__Stmt walkToStmtPost(SWIGTYPE_p_swift__Stmt s) {
                System.out.println("Visiting stmt: " + s.toString());
                return s;
            }
            @Override
            public Expr walkToExprPost(Expr e) {
                System.out.print("Visiting expr: ");
                e.print(swiftc.dbgs());
                System.out.println();
                return e;
            }
        };
        // walk returns true on failure
        System.out.println("Walk result: " + !instance.getMainModule().walk(walker));
    }
}
