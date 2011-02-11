public class Test1{
	public static int add(int a, int b){
		return a + b;
	}
	
	public static int div(int a, int b){
		return a / b;
	}
	
	public static Object identity(Object src){
		return src;
	}
	
	public Object self(){
		return this;
	}
    
    public static int recursiveCall(int depth){
        if(depth > 0)
            return 1 + recursiveCall(depth - 1);
        return 0;
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
