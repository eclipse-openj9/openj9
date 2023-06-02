package jit.test.tr.BNDCHKImplicitNull;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class BNDCHKImplicitNullTest {

	static class C {
		private int idx = 1;
		private double[] arr;

		public void shouldThrowNullPointerException() {
			for (int i = 0; i < 1; ++i) {
				double x = arr[idx];
			}
		}
	}

	@Test
	public void testFoldedImplicitNULLCHK() {
		C c = new C();
		for (int i = 0; i < 10000; ++i) {
			try {
				c.shouldThrowNullPointerException();
				AssertJUnit.fail("failed to throw null pointer exception");
			} catch (NullPointerException ignored) {}
		}
	}
}
