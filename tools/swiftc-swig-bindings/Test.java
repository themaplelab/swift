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

        instance.performSema();
        if (instance.getASTContext().hadError())
            System.out.println("Parse error");
        ModuleDecl module = instance.getMainModule();
        SWIGTYPE_p_SILModule silModulePtr = swiftc.constructSILModule(new SWIGTYPE_p_ModuleDecl(ModuleDecl.getCPtr(module), false), new SWIGTYPE_p_SILOptions(SILOptions.getCPtr(new SILOptions()), false));
        swiftc.setSILModule(new SWIGTYPE_p_CompilerInstance(CompilerInstance.getCPtr(instance), false), silModulePtr);
        SILModule silModule = new SILModule(SWIGTYPE_p_SILModule.getCPtr(silModulePtr), false);
        System.out.println("Main source file: " + instance.getMainModule().getMainSourceFile(SourceFileKind.Main));
        System.out.println("Have SIL module? " + silModule);
        silModule.dump(true);
        ASTWalker walker = new ASTWalker() {
            @Override
            public boolean walkToDeclPre(Decl s) {
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
