package pmath.util;

import java.security.Permission;

/** A security manager that gives every permission but disallows exit.
 */
public class NoExitSecurityManager extends SecurityManager {
	@Override 
	public void checkExit(int status) {
		throw new SecurityException("exit not allowed");
	}
	
	@Override
	public void checkPermission(Permission perm) {
		/* allow everything else */
	}
}
