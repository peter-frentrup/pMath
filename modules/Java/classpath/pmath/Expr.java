package pmath;

public class Expr {
	public static native Object execute(String code, Object... args);
	
	static {
		String coredll = System.getProperty("pmath.core.dll");
		if(coredll != null)
			System.load(coredll);
		
		// reads pmath.core.base_directory
		System.load(System.getProperty("pmath.binding.dll"));
	}
}