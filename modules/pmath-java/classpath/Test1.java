public class Test1{
	public static int add(int a, int b){
		return a + b;
	}
	
	public static double div(double a, double b){
		return a / b;
	}
	
	public static void endlessLoop(){
		while(true){
		}
	}
	
	public static void interruptableLoop(){
		while(!java.lang.Thread.currentThread().isInterrupted()){
		}
	}
}
