package pmath;

public class NoExitSecurityManager extends SecurityManager {
	public void checkExit(int status){
		throw new SecurityException("exit not allowed");
	}
	
	public void checkLink(String lib){
		if(!lib.equals("pmath-java"))
			super.checkLink(lib);
	}
	
	public void checkPropertyAccess(String key){
		//if(!key.equals("user.dir"))
		//	super.checkPropertyAccess(key);
	}
}
