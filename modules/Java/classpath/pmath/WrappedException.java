package pmath;

public class WrappedException extends RuntimeException {
	public String pmathException;
	public WrappedException(String ex){
		super("pmath.WrappedException: " + ex);
		this.pmathException = ex;
	}
}
