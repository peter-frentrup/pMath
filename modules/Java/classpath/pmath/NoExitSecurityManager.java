package pmath;

public class NoExitSecurityManager extends SecurityManager {
	public void checkExit(int status){
		throw new SecurityException("exit not allowed");
	}
	
	public void checkLink(String lib){
		//System.out.println("checkLink: " + lib);
		//if(!lib.equals("pmath-java"))
		//	super.checkLink(lib);
	}
	
	public void checkPropertyAccess(String key){
	}
}
