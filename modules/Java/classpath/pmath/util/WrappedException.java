package pmath.util;

/** Wraps a native pMath exception.
 */
public class WrappedException extends RuntimeException {
	private Expr expr;

	/** Get the wrapped pMath exception expression.
	 * 
	 * @return An arbitrary expression.
	 */
	public Expr getExpr() {
		return expr;
	}

	/** Create a new wrapped pMath exception
	 * 
	 * @param expr The pMath representation of the expression.
	 */
	public WrappedException(Expr expr) {
		super("wrapped pMath exception: " + expr);
		this.expr = expr;
	}
	
	@java.io.Serial
	private static final long serialVersionUID = 3637246794112472621L;
}
