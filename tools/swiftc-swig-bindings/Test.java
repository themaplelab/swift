public class Test {
    public static void main(String args[]) {
        System.loadLibrary("swiftc");
        CompilerInvocation ci = new CompilerInvocation();
        //ci.addInputFilename(swiftc.make_stringref("fake.swift"));
        System.out.println(ci.getParseStdlib());
    }
}
