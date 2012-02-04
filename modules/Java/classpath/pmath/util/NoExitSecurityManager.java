package pmath.util;

import java.security.Permission;

public class NoExitSecurityManager extends SecurityManager {
	public void checkExit(int status){
		throw new SecurityException("exit not allowed");
	}
	
	public void checkPermission(Permission perm){
		/* allow everything else */
	}
}
