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
	private static final Class[] PARAMETERS_URL = new Class[]{ URL.class };
	private static final Class[] PARAMETERS_STRING = new Class[]{ String.class };
	
	/** Adds a directory or jar/zip file to the class path. */
	public static void add(String path)
		throws IOException, NoSuchMethodException, IllegalAccessException, InvocationTargetException 
	{
		ClassLoader sysLoader = ClassLoader.getSystemClassLoader();
		Class<?> classLoaderClass = sysLoader.getClass();
		try {
			Method method = classLoaderClass.getDeclaredMethod("appendToClassPathForInstrumentation", PARAMETERS_STRING);
			method.setAccessible(true);
			method.invoke(sysLoader, new Object[]{ path });
		}
		catch(NoSuchMethodException ex) {
		}

		if(sysLoader instanceof URLClassLoader) {
			Method method = URLClassLoader.class.getDeclaredMethod( "addURL", PARAMETERS_URL );
			method.setAccessible( true );
			method.invoke(sysLoader, new Object[]{ new File(path).getCanonicalFile().toURI().toURL() });
		}
	}
}