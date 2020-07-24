package pmath.util;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

/** Represents a pMath expression tree.
 */
public final class Expr {
    private final Object object;
    private final int convert_options;

    private static final int CONVERT_DEFAULT = 0;
    private static final int CONVERT_AS_JAVAOBJECT = 1;
    private static final int CONVERT_AS_SYMBOL = 2;
    private static final int CONVERT_AS_PARSED = 3;
    private static final int CONVERT_AS_EXPRESSION = 4;

    /** The pMath {@code System`List} symbol.
     */
    public static final Expr LIST_SYMBOL = symbol("System`List");

    /** Create an expression from an abitrary object using default conversion rules.
     * 
     * @param value An object or {@code null} to be converted to a suitable pMath expression upon evaluation.
     * 
     * @see #isDefaultConverted()
     * @see #javaObject(Object)
     */
    public Expr(Object value) {
        object = value;
        convert_options = CONVERT_DEFAULT;
    }

    private Expr(Object value, int opts) {
        object = value;
        convert_options = opts;
    }

    /** Create a pMath symbol reference.
     * 
     * @param name A valid pMath symbol name with namespace, e.g. {@code "System`Pi"}.
     * @return A new expression that will be converted to a pMath symbol upon evaluation.
     * 
     * @see #isSymbol()
     */
    public static Expr symbol(String name) {
        if(name == null)
            throw new IllegalArgumentException("Null not allowed for `name`.");
        return new Expr(name, CONVERT_AS_SYMBOL);
    }

    /** Create an expression from an arbitrary piece of valid pMath code.
     * 
     * @param code Valid pMath code
     * @return A new expression that will be converted to a pMath symbol upon evaluation.
     * 
     * @see #isUnparsedCode()
     */
    public static Expr parseDelayed(String code) {
        if(code == null)
            throw new IllegalArgumentException("Null not allowed for `code`.");
        return new Expr(code, CONVERT_AS_PARSED);
    }

    /** Create an expression from an arbitary object without special conversions.
     * 
     * @param object An arbitrary object or {@code null}.
     * @return An expression that will be converted to a {@code Java`JavaObject(...)} (or {@code /\/})
     *         pMath expression upon evaluation.
     * 
     * @see #isWrappedJavaObject()
     */
    public static Expr javaObject(Object object) {
        return new Expr(object, CONVERT_AS_JAVAOBJECT);
    }

    /** Create a new composite expression with this expression as head.
     * 
     * @param arguments The new composite expression's arguments.
     * @return A new pMath composite expression <i>h</i>(<i>a</i><sub>1</sub>, <i>a</i><sub>2</sub>, ...).
     * 
     * @see #isComposite()
     */
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

    /** Create a new composite expression.
     * 
     * @param headAndArgs The new composite expression's head and arguments. Must be at least 1 argument (for the head).
     * @return A new expression pMath <span class="math"><i>a</i><sub>0</sub>(<i>a</i><sub>1</sub>, <i>a</i><sub>2</sub>, ...)</span>.
     * 
     * @see #isComposite()
     */
    public static Expr callTo(Object... headAndArgs) {
        if (headAndArgs.length == 0)
            throw new IllegalArgumentException("expression head (element [0]) is missing");

        return new Expr(headAndArgs, CONVERT_AS_EXPRESSION);
    }

    /** Get the underlying object data.
     * 
     * @return The underlying object. Its meaning depends on the kind of expression.
     */
    public Object getObject() {
        return object;
    }

    /** Test whether this expression represents a composite expression.
     * 
     * @return whether this expression represents a composite expression 
     *         <span class="math"><i>a</i><sub>0</sub>(<i>a</i><sub>1</sub>, <i>a</i><sub>2</sub>, ...)</span>.
     * @see #length()
     * @see #head()
     * @see #part(int)
     */
    public boolean isComposite() {
        switch (convert_options) {
            case CONVERT_DEFAULT:
                return object instanceof Object[];

            case CONVERT_AS_EXPRESSION:
                return true;
        }

        return false;
    }

    /** Test whether this expression represents an explicit Java reference.
     * 
     * The referenced object is {@link #getObject()}.
     * 
     * @return whether this expression represents a {@code Java`JavaObject(...)} reference for pMath code.
     */
    public boolean isWrappedJavaObject() {
        return convert_options == CONVERT_AS_JAVAOBJECT;
    }

    /** Test whether this expression represents a value with default conversion rules.
     * 
     * If {@link #getObject()} is an instance of one of the special classes mentioned in 
     * {@link pmath.ParserArguments#getExpectedType()}, it will be converted to the corresponding pMath type.
     * Otherwise, this expression will be converted a {@code Java`JavaObject(...)} (or {@code /\/}).
     * 
     * @return whether this expression uses default conversion rules for {@link #getObject()}.
     */
    public boolean isDefaultConverted() {
        return convert_options == CONVERT_DEFAULT;
    }

    /** Test whether this expression represents a pMath symbol.
     * 
     * A symbol expression's {@link #getObject()} gives the symbol's full name.
     * 
     * @return whether this expression represents a pMath symbol {@code full`namespace`path`name}.
     */
    public boolean isSymbol() {
        return convert_options == CONVERT_AS_SYMBOL;
    }

    /** Test whether this expression represents an arbitrary pMath object given by its InputForm string.
     * 
     * @return whether this expression will by parsing its {@code (String){@link #getObject()}}.
     */
    public boolean isUnparsedCode() {
        return convert_options == CONVERT_AS_PARSED;
    }

    /** Test whether this expression represents a pMath string.
     * 
     * @return Whether {@link #isDefaultConverted()} and {@link #getObject()} is a string.
     */
    public boolean isString() {
        return isDefaultConverted() && object instanceof String;
    }

    /** Test whether this expression represents a (small) pMath integer.
     * 
     * @return Whether {@link #isDefaultConverted()} is true and {@link #getObject()} is 
     *         an {@link Integer}, {@link Short}, or {@link Byte}.
     * @see #getInt() 
     */
    public boolean isInt() {
        if (!isDefaultConverted())
            return false;
        return object instanceof Integer || object instanceof Short || object instanceof Byte;
    }

    /** Test whether this expression represents an arbitrary precision pMath integer.
     * 
     * @return Whether {@link #isDefaultConverted()} is true and {@link #getObject()} is 
     *         a {@link java.math.BigInteger}, {@link Long}, {@link Integer}, {@link Short}, or {@link Byte}.
     * @see #getInteger() 
     */
    public boolean isInteger() {
        if (!isDefaultConverted())
            return false;
        return object instanceof BigInteger || object instanceof Long || object instanceof Integer
                || object instanceof Short || object instanceof Byte;
    }

    /** Get the int value of an integer exprssion.
     * 
     * @return the integer value if {@link #isInt()} is true.
     * @throws UnsupportedOperationException if {@link #isInt()} is false.
     */
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

    /** Get the integer value of an integer exprssion.
     * 
     * @return the integer value if {@link #isInteger()} is true.
     * @throws UnsupportedOperationException if {@link #isInteger()} is false.
     */
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

    /** Get the length of this composite expression.
     * 
     * @return The expression's number of arguments or 0 if {@link #isComposite()} is false.
     * @see #part(int)
     */
    public int length() {
        switch (convert_options) {
            case CONVERT_AS_EXPRESSION:
                return ((Object[]) object).length - 1;

            case CONVERT_DEFAULT:
                if (object instanceof Object[])
                    return ((Object[]) object).length;
                return 0;
        }

        return 0;
    }

    /** Get this composite expression's head.
     * 
     * If {@link #isDefaultConverted()} is true and {@link #getObject()} is an array,
     * the array will contain elements for a pMath list, and this function will give
     * {@link #LIST_SYMBOL}.
     * 
     * @return The expression's head if {@link #isComposite()} is true, and a {@code new Expr(null)}
     *         otherwise. 
     */
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

    /** Get a part of this composite expression.
     * 
     * @param index The 1-based part index.
     * @return If index is 0, returns {@link #head()}. 
     *         If index is between 1 and {@link #length()} (inclusive), returns the requested 
     *         argument <span class="math"><i>a</i><sub><i>index</i></sub></span> of this composite expression
     *         <span class="math"><i>h</i>(<i>a</i><sub>1</sub>, <i>a</i><sub>2</sub>, ...)</span>.
     *         Otherwise, returns a {@code new Expr(null)}.
     */
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

    /** Literally compare this expression to another expression.
     * 
     * <p>
     * Note that wrapped java objects ({@link #isWrappedJavaObject()}) are compared for reference equality.
     */
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
                Charset charset = StandardCharsets.UTF_16LE;

                try (PrintStream ps = new PrintStream(byteArrayStream, true, charset)) {
                    writeTo(ps);
                }

                return byteArrayStream.toString(charset);
            }
        } catch (IOException ex) {
            return "" + object;
        }
    }

    /** Print this expression to a stream.
     * 
     * <p>
     * This function is an optimized version of {@code stream.append(this.toString())}.
     * 
     * @param stream A stream to print to.
     */
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