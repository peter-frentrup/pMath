package pmath;

public class ParserArguments {
    private final Context context;
    private final String code;
    private final Object[] arguments;
    private final String simplePostProcessor;
    private final Class<?> expectedType;

    public Context getContext() { return context; }
    public Class<?> getExpectedType() { return expectedType; }
    public String getCode() { return code; }
    public Object[] getArguments() { return arguments; }
    public String getSimplePostProcessor() { return simplePostProcessor; }

    public ParserArguments(Context context, String code) {
        this(context, code, null, Object.class, "");
    }

    public ParserArguments(Context context, String code, Object[] arguments) {
        this(context, code, arguments, Object.class, "");
    }

    public ParserArguments(Context context, String code, Object[] arguments, Class<?> expectedType) {
        this(context, code, arguments, expectedType, "");
    }

    public ParserArguments(Context context, String code, Object[] arguments, Class<?> expectedType, String simplePostProcessor) {
        this.context = context;
        this.code = code;
        this.arguments = arguments;
        this.expectedType = expectedType;
        this.simplePostProcessor = simplePostProcessor;
    }
    
    protected ParserArguments(ParserArguments original) {
        this(original.context, original.code, original.arguments, original.expectedType, original.simplePostProcessor);
    }

    public ParserArguments withPostProcessor(String function, Class<?> newExpectedType) {
        if(simplePostProcessor != null && !simplePostProcessor.isEmpty())
            throw new IllegalStateException("Multiple post-processors are not currently allowed");
        
        return new ParserArguments(context, code, arguments, newExpectedType, function);
    }

    public ParserArguments withPostProcessor(String function) {
        return withPostProcessor(function, expectedType);
    }

    public ParserArguments withStringConversion() {
        return withPostProcessor("System`ToString", String.class);
    }

    public ParserArguments withDoubleConversion() {
        return withDoubleConversion(Double.class);
    }

    public ParserArguments withDoubleConversion(Class<?> newExpectedType) {
        return withPostProcessor("System`N", newExpectedType);
    }

    public native Object execute();

    static {
        Core.load();
    }
}