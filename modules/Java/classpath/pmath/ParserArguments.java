package pmath;

/** Represents a piece of pMath code to be parsed, together with settings that control parsing and evaluation result.
 */
public class ParserArguments {
    private final Context context;
    private final String code;
    private final Object[] arguments;
    private final String simplePostProcessor;
    private final Class<?> expectedType;

    /** Gets the runtime context to use for parsing and evaluation.
     * @return The runetime context to use and update for evaluations. 
     */
    public Context getContext() { return context; }

    /** Gets the type to convert evaluation results to. 
     * 
     * <p>Recognized types are 
     * <ul>
     *  <li>the eight primitive types ({@code boolean}, {@code byte}, {@code char}, {@code double}, 
     *      {@code float}, {@code int}, {@code long}, {@code short}) represented by their boxing classes
     *      ({@code Boolean}, {@code Byte}, etc.),
     *  <li>the {@link java.math.BigInteger} class,
     *  <li>the {@link java.lang.Number} class to allow any of the recognized number types above,
     *  <li>the {@link String} class for strings,
     *  <li>the {@link pmath.util.Expr} class for arbitrary expressions,
     *  <li>the {@link Object} class to allow any recognized type,
     *  <li>any array of recognized types (including primitive types),
     *  <li>the {@code void} type (represented by {@link Void}), to ignore any pMath result and return
     *      {@code null}.
     * </ul>
     * 
     * When no particular number type is expected, pMath will give integers as {@code int} if they fit –
     * otherwise as {@code long} or {@code java.math.BigInteger}. Real values are by default returned as
     * {@code double}.
     * 
     * @return {@code null} or a recognized Java class.
     */
    public Class<?> getExpectedType() { return expectedType; }

    /** Gets the pMath code to be parsed.
     * 
     * @return A string possibly containing slots {@code `1`}, {@code `2`} etc. to be filled with 
     *         {@link #getArguments()} elements after parsing.
     */
    public String getCode() { return code; }

    /** Gets the replacement arguments to insert into the expression tree after parsing, but before evaluation.
     * 
     * @return {@code null} or an array {@code args} of arguments to replace {@code `1`}, {@code `2`}, ... 
     *         slots in the pMath code after parsing. Note that slot {@code `1`} gets replaced by element 
     *         {@code args[0]}, {@code `2`} gets replaced by {@code args[1]}, etc.
     */
    public Object[] getArguments() { return arguments; }

    /** Gets an (optional) pMath function to post-process the evaluated result.
     * 
     * @return {@code null} or the pMath code (without replacement slots) of a function to applay to the main evaluation result.
     */
    public String getSimplePostProcessor() { return simplePostProcessor; }

    /** Creates a parsing environment for specified code in a given context.
     * 
     * @param context The context to use for parsing and evaluation.
     * @param code    The pMath code.
     */
    public ParserArguments(Context context, String code) {
        this(context, code, null, Object.class, "");
    }

    /** Creates a parsing environment for parametrized code in a given context.
     * 
     * @param context   The context to use for parsing and evaluation.
     * @param code      The pMath code, possibly containing slots {@code `1`}, {@code `2`}, etc.
     * @param arguments The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     */
    public ParserArguments(Context context, String code, Object[] arguments) {
        this(context, code, arguments, Object.class, "");
    }

    /** Creates a parsing environment for parametrized code in a given context with expected evaluatin result type.
     * 
     * @param context      The context to use for parsing and evaluation.
     * @param code         The pMath code, possibly containing slots {@code `1`}, {@code `2`}, etc.
     * @param arguments    The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @param expectedType The type to convert the evaluation result to, see {@link #getExpectedType()}.
     */
    public ParserArguments(Context context, String code, Object[] arguments, Class<?> expectedType) {
        this(context, code, arguments, expectedType, "");
    }

    /** Creates a parsing environment for parametrized code in a given context with post-processing and expected evaluatin result type.
     * 
     * @param context             The context to use for parsing and evaluation.
     * @param code                The pMath code, possibly containing slots {@code `1`}, {@code `2`}, etc.
     * @param arguments           The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @param expectedType        The type to convert the evaluation result, to after post-processing it.
     *                            See {@link #getExpectedType()}.
     * @param simplePostProcessor {@code null} or a pMath function such as "System`ToString" to post-process the evaluation result.
     */
    public ParserArguments(Context context, String code, Object[] arguments, Class<?> expectedType, String simplePostProcessor) {
        this.context = context;
        this.code = code;
        this.arguments = arguments;
        this.expectedType = expectedType;
        this.simplePostProcessor = simplePostProcessor;
    }
    
    /** Copies a parsing environment.
     * 
     * @param original The object to be copied.
     */
    protected ParserArguments(ParserArguments original) {
        this(original.context, original.code, original.arguments, original.expectedType, original.simplePostProcessor);
    }

    /** Gets a copy of this parsing environment with given post-processor.
     * 
     * @param function        The post-processor (pMath code) to apply to the evaluation result.
     * @param newExpectedType The new expected return type, see {@link #getExpectedType()}
     * @return                A copy of this parsing environment with modified post-processor and return type.
     */
    public ParserArguments withPostProcessor(String function, Class<?> newExpectedType) {
        if(simplePostProcessor != null && !simplePostProcessor.isEmpty())
            throw new IllegalStateException("Multiple post-processors are not currently allowed");
        
        return new ParserArguments(context, code, arguments, newExpectedType, function);
    }

    /** Gets a copy of this parsing environment with {@code System`ToString} post-processor.
     * 
     * @return A new parsing environment based on this, with {@code String} expected return type.
     */
    public ParserArguments withStringConversion() {
        return withPostProcessor("System`ToString", String.class);
    }

    /** Gets a copy of this parsing environment with {@code System`Numericalize} post-processor.
     * 
     * @return A new parsing environment based on this, with {@code Double} expected return type.
     */
    public ParserArguments withDoubleConversion() {
        return withDoubleConversion(Double.class);
    }

    /** Gets a copy of this parsing environment with {@code System`Numericalize} post-processor.
     * 
     * @param newExpectedType The new expected return type, e.g. {@code Double} or {@code Double[]}.
     * @return A new parsing environment based on this.
     */
    public ParserArguments withDoubleConversion(Class<?> newExpectedType) {
        return withPostProcessor("System`Numericalize", newExpectedType);
    }

    /** Parse and evaluate the pMath code.
     * 
     * @return The evaluated result (after possible post-processing), cast to {@code #getExpectedType()}
     */
    public native Object execute();

    static {
        Core.load();
    }
}