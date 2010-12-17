package pmath;

public class NoExitSecurityManager extends SecurityManager {
	public void checkExit(int status){
		throw new SecurityException("exit not allowed");
	}
}
