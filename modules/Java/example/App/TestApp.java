import java.io.*;

public class TestApp {
	static class CapturedLocalValues {
		public boolean quit;
	}
	
	public static void main(String[] args) {
		System.out.println("-Dpmath.core.base_directory = " + System.getProperty("pmath.core.base_directory"));
		System.out.println("-Dpmath.core.dll            = " + System.getProperty("pmath.core.dll"));
		System.out.println("-Dpmath.binding.dll         = " + System.getProperty("pmath.binding.dll"));
		System.out.println();
		System.out.println("pMath hosted in a Java app. Type 'QUIT' or 'Quit()' to exit.");
		
		pmath.Core.execute("Print(`1`)", "Hello");
		
		Console console = System.console();
		if (console == null) {
			System.err.println("No console found.");
			return;
		}
		
		CapturedLocalValues locals = new CapturedLocalValues();
		locals.quit = false;
		pmath.Core.execute(
			"Unprotect(Quit);" +
			"Quit()::= `1` @ quit:= True;" + // ignoring Quit(~exitcode)
			"Protect(Quit);", 
			locals);
		
		while(!locals.quit) {
			System.out.print("\n<  ");
			String input = console.readLine();
			if("QUIT".equals(input))
				break;
				
			Object result = pmath.Core.execute(input);
			if(result != null){
				System.out.println(" > " + result);
			}
		}
		
		System.out.println("Bye.");
	}
}
