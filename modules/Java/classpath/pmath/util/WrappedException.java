package pmath.util;

public class WrappedException extends RuntimeException {
	private String code;
	
	public String getCode(){
		return code;
	}
	
	public WrappedException(String ex){
		super("wrapped pMath exception: " + ex);
		this.code = ex;
	}
}
