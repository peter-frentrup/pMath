package pmath;

import java.math.BigInteger;
import java.util.Arrays;

public class Context {
    private String namespace;
    private String[] namespacePath;

    public String getNamespace() { return namespace; }
    public void setNamespace(String ns) { namespace = ns; }

    public String[] getNamespacePath() { return namespacePath; }
    public void setNamespacePath(String[] nsPath) { namespacePath = nsPath; }

    public Context() {
        this("Global`");
    }

    public Context(String namespace) {
        this(namespace, namespace == null ? null : new String[]{ "System`" });
    }

    public Context(String namespace, String[] namespacePath) {
        this.namespace = namespace;
        this.namespacePath = namespacePath;
    }

    public Context(Context original) {
        namespace = original.namespace;
        namespacePath = Arrays.copyOf(original.namespacePath, original.namespacePath.length);
    }

    public Context withNamespace(String ns) {
        Context result = new Context(this);
        result.setNamespace(ns);
        return result;
    }
    public Context withNamespacePath(String[] nsPath) {
        Context result = new Context(this);
        result.setNamespacePath(nsPath);
        return result;
    }

    public ParserArguments parse(String code, Object... args) {
        return new ParserArguments(this, code, args);
    }
    public ParserArguments parse(Class<?> expectedType, String code, Object... args) {
        return new ParserArguments(this, code, args, expectedType); 
    }

    public void run(String code, Object... args) {
        parse(Void.class, code, args).execute();
    }

    public Object evaluate(String code, Object... args) {
        return parse(code, args).execute();
    }

    public Object[] evaluateArray(String code, Object... args) {
        return (Object[])parse(Object[].class, code, args).execute();
    }

    public String evaluateToString(String code, Object... args) {
        return "" + parse(code, args).withStringConversion().execute();
    }

    public boolean evaluateBoolean(String code, Object... args) {
        return (Boolean)parse(Boolean.class, code, args).execute();
    }

    public int evaluateInt(String code, Object... args) {
        return (Integer)parse(Integer.class, code, args).execute();
    }

    public BigInteger evaluateBigInteger(String code, Object... args) {
        return (BigInteger)parse(BigInteger.class, code, args).execute();
    }

    public double evaluateToDouble(String code, Object... args) {
        return (Double)parse(code, args).withDoubleConversion().execute();
    }
}