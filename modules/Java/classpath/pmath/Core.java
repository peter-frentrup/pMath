package pmath;

import java.io.*;

/** Loading the pMath system libraries.
 * 
 * <p>This class uses the following system properties:
 * <ul>
 *  <li>{@code "pmath.core.dll"} –
 *      the path of the main pMath DLL.
 *  <li>{@code "pmath.core.base_directory"} –
 *      (optional) the directory that contains the {@code maininit.pmath} file
 *      (overrides the environment variable {@code PMATH_BASEDIRECTORY}).
 *  <li>{@code "pmath.binding.dll"} –
 *      the location of the JNI bridge library for this package.
 * </ul>
 * 
 * @see pmath.Context
 * @author Peter Frentrup
 */
public class Core {
    /** Parses and evaluates a string of pMath code.
     * 
     * <p>Note that this forwards any exception thrown from pMath code.
     * 
     * @param code Valid pMath code with possible argument specifications {@code `1`}, {@code `2`}, ...
     * @param args The elements to insert in place of {@code `1`}, {@code `2`}, etc. after parsing.
     * @return The evaluation result, cast to {@link java.lang.Object} if possible, or {@code null}.
     * 
     * @deprecated To have more control over the evaluation environment and returned result types, use 
     *             {@link pmath.Context#evaluate(String, Object...)} and related functions.
     */
    public static native Object execute(String code, Object... args);
    
    /** Forces the {@link pmath.Core} class to load.
     */
    public static void load() {
    }

    private Core() {}

    private static String getPathInUserDir(String path){
        if(path == null)
            return path;
        
        if(new File(path).isAbsolute())
            return path;
        
        return System.getProperty("user.dir") + System.getProperty("file.separator") + path;
    }
    
    static {
        String coredll        = getPathInUserDir(System.getProperty("pmath.core.dll"));
        String base_directory = getPathInUserDir(System.getProperty("pmath.core.base_directory"));
        String bindingdll     = getPathInUserDir(System.getProperty("pmath.binding.dll"));
        
        if(base_directory == null) {
            try {
                File file = new File(bindingdll);
                if(new File(file.getParentFile(), "maininit.pmath").exists())
                    base_directory = file.getParent();
            } catch(Exception ex){
            }
        }
        
        if(base_directory == null && coredll != null) {
            try {
                File file = new File(coredll);
                if(new File(file.getParentFile(), "maininit.pmath").exists())
                    base_directory = file.getParent();
            } catch(Exception ex) {
            }
        }
        
        try{ System.setProperty("pmath.core.dll",            coredll);        } catch(Exception ex) { }
        try{ System.setProperty("pmath.core.base_directory", base_directory); } catch(Exception ex) { }
        try{ System.setProperty("pmath.binding.dll",         bindingdll);     } catch(Exception ex) { }
        
        if(coredll != null)
            System.load(coredll);
        
        // reads pmath.core.base_directory
        System.load(bindingdll);
    }
}