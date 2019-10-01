/* This class is used by TestDestroyCache.java to keep a shared class cache active */
public class InfiniteLoop {
	public static void main(String args[]) throws Exception {
		/* Do not change following print statement. 
		 * It will break TestDestroyCache tests as it checks for it in the output.
		 */
		System.out.println("Running infinite loop");
		Object lock = new Object();
		synchronized (lock) {
			lock.wait();
		}
	}
}