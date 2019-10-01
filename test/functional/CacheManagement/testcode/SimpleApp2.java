import java.util.regex.*;

public class SimpleApp2 {
	public void call() {
		/*
		 * CMVC 186357 : Use Pattern class to ensure classes in java.util.regex
		 * package are loaded.
		 */
		boolean match = false;
		String classNameRegex = "[a-zA-Z_][[\\w]|\\$]*";
		match = Pattern.matches(classNameRegex, "SimpleApp2");
	}
}
