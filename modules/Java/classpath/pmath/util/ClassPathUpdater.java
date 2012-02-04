package pmath.util;

import java.io.File;
import java.io.IOException;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.net.URL;
import java.net.URLClassLoader;

/**
 * Allows programs to modify the classpath during runtime.
 * Source: http://stackoverflow.com/questions/271506/why-system-setproperty-cannot-change-the-classpath-at-run-time
 */
public class ClassPathUpdater {
	/** Used to find the method signature. */
	private static final Class[] PARAMETERS = new Class[]{ URL.class };

	/** Class containing the private addURL method. */
	private static final Class<?> CLASS_LOADER = URLClassLoader.class;

	/** Adds a direcotry or jar/zip file to the class path. */
	public static void add( String s )
		throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException 
	{
		add( new File( s ) );
	}

	/** Adds a direcotry or jar/zip file to the class path. */
	public static void add( File f )
		throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException 
	{
		add( f.toURI().toURL() );
	}

	/** Adds a direcotry or jar/zip file to the class path. */
	public static void add( URL url )
		throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException 
	{
		Method method = CLASS_LOADER.getDeclaredMethod( "addURL", PARAMETERS );
		method.setAccessible( true );
		method.invoke( getClassLoader(), new Object[]{ url } );
	}

	private static URLClassLoader getClassLoader() {
		return (URLClassLoader)ClassLoader.getSystemClassLoader();
	}
}