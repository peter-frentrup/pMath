package pmath;

import java.io.*;

public class Core {
	public static native Object execute(String code, Object... args);
	
	private static String getPathInUserDir(String path){
		if(path == null)
			return path;
		
		if(new File(path).isAbsolute())
			return path;
		
		return System.getProperty("user.dir") + System.getProperty("file.separator") + path;
	}
	
	static {
		String coredll        = getPathInUserDir(System.getProperty("pmath.core.dll"));
		String base_directory = getPathInUserDir(System.getProperty("pmath.core.base_directory"));
		String bindingdll     = getPathInUserDir(System.getProperty("pmath.binding.dll"));
		
		if(base_directory == null) {
			try {
				File file = new File(bindingdll);
				if(new File(file.getParentFile(), "maininit.pmath").exists())
					base_directory = file.getParent();
			} catch(Exception ex){
			}
		}
		
		if(base_directory == null && coredll != null) {
			try {
				File file = new File(coredll);
				if(new File(file.getParentFile(), "maininit.pmath").exists())
					base_directory = file.getParent();
			} catch(Exception ex) {
			}
		}
		
		try{ System.setProperty("pmath.core.dll",            coredll);        } catch(Exception ex) { }
		try{ System.setProperty("pmath.core.base_directory", base_directory); } catch(Exception ex) { }
		try{ System.setProperty("pmath.binding.dll",         bindingdll);     } catch(Exception ex) { }
		
		if(coredll != null)
			System.load(coredll);
		
		// reads pmath.core.base_directory
		System.load(bindingdll);
	}
}