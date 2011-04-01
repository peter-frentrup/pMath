package pmath;

public class Expr {
    public static native Object execute(String code, Object... args);
    
    static {
        System.loadLibrary("pmath-java");
    }
}