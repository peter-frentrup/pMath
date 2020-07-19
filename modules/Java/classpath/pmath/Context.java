package pmath;

import java.util.Arrays;

/** Defines an environment to execute pMath code in.
 * 
 * @author Peter Frentrup
 */
public class Context {
    private String namespace;
    private String[] namespacePath;

    /** Gets the pMath namespace.
     * 
     * <p>This may initially be {@code null} inherit a namespace upon first evaluation.
     * 
     * @return {@code null} or the pMath namespace such as {@code Global`} to use for new symbols 
     *         when executing pMath code with this context.
     */
    public String getNamespace() { return namespace; }

    /** Sets the pMath namespace this context.
     * 
     * <p>Note that after each evaluation, this property gets updated to denote the current pMath namespace.
     * 
     * @param ns The pMath namespace (e.g. {@code Global`}) to use for new symbols when executing 
     *           pMath code with this context, or {@code null}.
     */
    public void setNamespace(String ns) { namespace = ns; }

    /** Gets the pMath namespace path.
     * 
     * <p>This should contain {@code System`}.
     * 
     * @return {@code null} or the list of namespaces to search (besides {@link #getNamespace()}) 
     *         for pMath symbols.
     */
    public String[] getNamespacePath() { return namespacePath; }

    /** Sets the pMath namespace path for this context.
     * 
     * <p>Note that after each evaluation with this context, this property gets updated to denote
     * the current pMath namespace.
     * 
     * @param nsPath The list of namespaces to search (besides {@link #getNamespace()}) for pMath symbols, 
     *               or {@code null}.
     */
    public void setNamespacePath(String[] nsPath) { namespacePath = nsPath; }

    /** Creates a new context with namespace {@code Global`} and namespace path {@code {"System`"}}.
     */
    public Context() {
        this("Global`");
    }

    /** Creates a new context with given namespace.
     * 
     * <p>If {@code namespace} is {@code null}, the namespace path will be {@code null}, otherwise, it defaults
     *      to {@code {"System`", "Java`"}}.
     * 
     * @param namespace The initial pMath namespace, e.g. {@code "Global`"}.
     */
    public Context(String namespace) {
        this(namespace, namespace == null ? null : new String[]{ "System`", "Java`" });
    }

    /** Creates a new context with given namespace and namespace path.
     * 
     * @param namespace     The initial pMath namespace, e.g. {@code "Global`"}.
     * @param namespacePath The initial pMath namespace path, e.g. {@code {"System`", "Java`"}}.
     */
    public Context(String namespace, String[] namespacePath) {
        this.namespace = namespace;
        this.namespacePath = namespacePath;
    }

    /** Creates a new context by copying another one.
     * 
     * @param original The Context object to make a deep copy of. Must not be {@code null}.
     */
    public Context(Context original) {
        namespace = original.namespace;
        if(original.namespacePath != null)
            namespacePath = Arrays.copyOf(original.namespacePath, original.namespacePath.length);
    }

    /** Gets a copy of this context with new namespace.
     * 
     * @param ns A pMath namespace.
     * @return The new context.
     * 
     * @see #setNamespace(String)
     */
    public Context withNamespace(String ns) {
        Context result = new Context(this);
        result.setNamespace(ns);
        return result;
    }

    /** Gets a copy of this context with new namespace path.
     * 
     * @param nsPath An array pMath namespaces.
     * @return The new context.
     * 
     * @see #setNamespacePath(String[])
     */
    public Context withNamespacePath(String[] nsPath) {
        Context result = new Context(this);
        result.setNamespacePath(nsPath);
        return result;
    }

    /** Prepares evaluating pMath code.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return A {@link ParserArguments} structure for evaluation with this context.
     */
    public ParserArguments parse(String code, Object... args) {
        return new ParserArguments(this, code, args);
    }

    /** Prepares evaluating pMath code with expected return type.
     * 
     * @param expectedType The expected Java return type.
     * @param code         Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args         The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return A {@link ParserArguments} structure for evaluation with this context.
     */
    public ParserArguments parse(Class<?> expectedType, String code, Object... args) {
        return new ParserArguments(this, code, args, expectedType); 
    }

    /** Runs a piece of pMath code.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     */
    public void run(String code, Object... args) {
        evaluate(Void.class, code, args);
    }

    /** Evaluates a piece of pMath code.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The Java representation of the evaluation result, or {@code null} if conversion is not possible.
     */
    public Object evaluate(String code, Object... args) {
        return parse(code, args).execute();
    }

    /** Evaluates a piece of pMath code to an expected Java return type.
     * 
     * @param <R>          The return type.
     * @param expectedType The expected return type. For recognized types, see {@link ParserArguments#getExpectedType()}.
     * @param code         Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args         The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The Java representation of the evaluation result, cast to {@code R}.
     */
    public <R> R evaluate(Class<R> expectedType, String code, Object... args) {
        @SuppressWarnings("unchecked")
        R result = (R)parse(expectedType, code, args).execute();
        return result;
    }

    /** Evaluates a piece of pMath code that returns a list.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return An array of objects containing the pMath list's elements. 
     */
    public Object[] evaluateArray(String code, Object... args) {
        return evaluate(Object[].class, code, args);
    }

    /** Evaluates a piece of pMath code and convert the result to a string.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The pMath output form.
     */
    public String evaluateToString(String code, Object... args) {
        return "" + parse(code, args).withStringConversion().execute();
    }

    /** Evaluates a piece of pMath code that returns a symbols True or False.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return {@code true} for the symbol True and {@code false} for the symbol False.
     */
    public boolean evaluateBoolean(String code, Object... args) {
        return evaluate(Boolean.class, code, args);
    }

    /** Evaluates a piece of pMath code that returns a small integer value.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The 32 bit integer value.
     */
    public int evaluateInt(String code, Object... args) {
        return evaluate(Integer.class, code, args);
    }

    /** Evaluates a piece of pMath code and approximate it.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The approximated result.
     */
    public double evaluateToDouble(String code, Object... args) {
        return (Double)parse(code, args).withDoubleConversion().execute();
    }
}