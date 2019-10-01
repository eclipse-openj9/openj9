package tests.sharedclasses;

public class TestFailedException extends RuntimeException {
	private static final long serialVersionUID = 1L;

	TestFailedException(String reason) {
	  super(reason);
    }
}
