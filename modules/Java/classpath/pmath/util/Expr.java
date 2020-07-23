package pmath.util;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

public final class Expr {
    private final Object object;
    private final int convert_options;

    private static final int CONVERT_DEFAULT = 0;
    private static final int CONVERT_AS_JAVAOBJECT = 1;
    private static final int CONVERT_AS_SYMBOL = 2;
    private static final int CONVERT_AS_PARSED = 3;
    private static final int CONVERT_AS_EXPRESSION = 4;

    public static final Expr LIST_SYMBOL = symbol("System`List");

    public Expr(Object value) {
        object = value;
        convert_options = CONVERT_DEFAULT;
    }

    private Expr(Object value, int opts) {
        object = value;
        convert_options = opts;
    }

    public static Expr symbol(String name) {
        return new Expr(name, CONVERT_AS_SYMBOL);
    }

    public static Expr parseDelayed(String code) {
        return new Expr(code, CONVERT_AS_PARSED);
    }

    public static Expr javaObject(Object object) {
        return new Expr(object, CONVERT_AS_JAVAOBJECT);
    }

    public Expr call(Object... arguments) {
        Object[] headAndArgs = new Object[arguments.length + 1];
        headAndArgs[0] = this;
        for (int i = 0; i < arguments.length; ++i)
            headAndArgs[i + 1] = arguments[i];
        return new Expr(headAndArgs, CONVERT_AS_EXPRESSION);
    }

    @SuppressWarnings("unused")
    private static Expr callTo() {
        throw new IllegalArgumentException();
    }

    public static Expr callTo(Object... headAndArgs) {
        if (headAndArgs.length == 0)
            throw new IllegalArgumentException("expression head (element [0]) is missing");

        return new Expr(headAndArgs, CONVERT_AS_EXPRESSION);
    }

    public Object getObject() {
        return object;
    }

    public boolean isComposite() {
        switch (convert_options) {
            case CONVERT_DEFAULT:
                return object instanceof Object[];

            case CONVERT_AS_EXPRESSION:
                return true;
        }

        return false;
    }

    public boolean isWrappedJavaObject() {
        return convert_options == CONVERT_AS_JAVAOBJECT;
    }

    public boolean isDefaultConverted() {
        return convert_options == CONVERT_DEFAULT;
    }

    public boolean isSymbol() {
        return convert_options == CONVERT_AS_SYMBOL;
    }

    public boolean isUnparsedCode() {
        return convert_options == CONVERT_AS_PARSED;
    }

    public boolean isString() {
        return (isWrappedJavaObject() || isDefaultConverted()) && object instanceof String;
    }

    public boolean isInt() {
        if (!isDefaultConverted())
            return false;
        return object instanceof Integer || object instanceof Short || object instanceof Byte;
    }

    public boolean isInteger() {
        if (!isDefaultConverted())
            return false;
        return object instanceof BigInteger || object instanceof Long || object instanceof Integer
                || object instanceof Short || object instanceof Byte;
    }

    public int getInt() {
        if (!isDefaultConverted())
            throw new UnsupportedOperationException("not an int");

        if (object instanceof Integer)
            return (Integer) object;
        if (object instanceof Short)
            return (Short) object;
        if (object instanceof Byte)
            return (Byte) object;

        throw new UnsupportedOperationException("not an int");
    }

    public BigInteger getInteger() {
        if (!isDefaultConverted())
            throw new UnsupportedOperationException("not an integer");

        if (object instanceof BigInteger)
            return (BigInteger) object;
        if (object instanceof Long)
            return BigInteger.valueOf((Long) object);
        if (object instanceof Integer)
            return BigInteger.valueOf((Integer) object);
        if (object instanceof Short)
            return BigInteger.valueOf((Short) object);
        if (object instanceof Byte)
            return BigInteger.valueOf((Byte) object);

        throw new UnsupportedOperationException("not an integer");
    }

    private static Expr asExpr(Object object) {
        if (object instanceof Expr)
            return (Expr) object;
        else
            return new Expr(object);
    }

    public int length() {
        switch (convert_options) {
            case CONVERT_AS_EXPRESSION:
                return ((Object[]) object).length - 1;

            case CONVERT_DEFAULT:
                if (object instanceof Object[])
                    return ((Object[]) object).length;
        }

        return 0;
    }

    public Expr head() {
        switch (convert_options) {
            case CONVERT_AS_EXPRESSION:
                return asExpr(((Object[]) object)[0]);

            case CONVERT_DEFAULT:
                if (object instanceof Object[])
                    return LIST_SYMBOL;
                break;
        }

        return new Expr(null);
    }

    public Expr part(int index) {
        if (index < 0)
            return new Expr(null);

        if (index == 0)
            return head();

        switch (convert_options) {
            case CONVERT_AS_EXPRESSION: {
                Object[] headAndArgs = (Object[]) object;
                if (index < headAndArgs.length)
                    return asExpr(headAndArgs[index]);
            }
                break;

            case CONVERT_DEFAULT: {
                if (object instanceof Object[]) {
                    Object[] listItems = (Object[]) object;
                    if (index <= listItems.length)
                        return asExpr(listItems[index - 1]);
                }
            }
                break;
        }
        return new Expr(null);
    }

    @Override
    public int hashCode() {
        if (object == null)
            return 0;

        if (isComposite()) {
            int result = 17;
            int len = length();
            for (int i = 0; i <= len; ++i)
                result = 31 * result + part(i).hashCode();
            return result;
        }

        return object.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof Expr))
            return false;

        Expr other = (Expr) obj;
        if (object == other.object)
            return true;
        if (object == null)
            return false;

        if (isComposite()) {
            if (!other.isComposite())
                return false;

            int len = length();
            if (len != other.length())
                return false;

            for (int i = 0; i <= len; ++i) {
                if (!part(i).equals(other.part(i)))
                    return false;
            }
            return true;
        }
        if (other.isComposite())
            return false;

        if (convert_options != other.convert_options)
            return false;

        if (convert_options == CONVERT_AS_JAVAOBJECT)
            return false; // we already checked for reference equality above

        // TODO: double and integers should be treated as not equal.
        return object.equals(other.object);
    }

    @Override
    public String toString() {
        try {
            try (ByteArrayOutputStream byteArrayStream = new ByteArrayOutputStream()) {
                Charset charset = StandardCharsets.UTF_8;

                try (PrintStream ps = new PrintStream(byteArrayStream, true, charset)) {
                    writeTo(ps);
                }

                return byteArrayStream.toString(charset);
            }
        } catch (IOException ex) {
            return "" + object;
        }
    }

    public void writeTo(PrintStream stream) {
        switch (convert_options) {
            case CONVERT_AS_EXPRESSION:
                writeCallForm(stream, (Object[]) object);
                return;

            case CONVERT_DEFAULT:
                if (object instanceof Object[]) {
                    writeListForm(stream, (Object[]) object);
                    return;
                }
                break;

            case CONVERT_AS_JAVAOBJECT:
                stream.append("JavaObject( ");
                stream.append("" + object);
                stream.append(" )");
                return;
        }

        stream.append("" + object);
    }

    private static void writeCallForm(PrintStream stream, Object[] headAndArgs) {
        if (headAndArgs.length == 0) {
            stream.append(headAndArgs.toString());
            return;
        }

        Expr head = asExpr(headAndArgs[0]);
        if (head.isSymbol()) {
            head.writeTo(stream);
        } else {
            stream.append('(');
            head.writeTo(stream);
            stream.append(')');
        }
        stream.append('(');
        for (int i = 1; i < headAndArgs.length; ++i) {
            if (i > 1) {
                stream.append(", ");
            }

            asExpr(headAndArgs[i]).writeTo(stream);
        }
        stream.append(')');
    }

    private static void writeListForm(PrintStream stream, Object[] elements) {
        // Class<?> clazz = elements.getClass();
        // if(!clazz.getName().endsWith(";")) { // not enging in "Lname;": arbitrarily
        // nested array of primitive type
        // stream.append(elements.toString());
        // return;
        // }
        // stream.append(clazz.getSimpleName());
        stream.append("{ ");
        boolean first = true;
        for (Object item : elements) {
            if (first) {
                first = false;
            } else {
                stream.append(", ");
            }

            asExpr(item).writeTo(stream);
        }
        stream.append(" }");
    }
}