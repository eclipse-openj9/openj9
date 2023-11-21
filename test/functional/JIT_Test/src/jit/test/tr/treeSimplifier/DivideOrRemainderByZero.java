/*******************************************************************************
 * Copyright IBM Corp. and others 2023
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package jit.test.tr.treeSimplifier;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class DivideOrRemainderByZero {
	public static class ShouldExpectException {
		public boolean expect = false;
	}

	public static int copiedIntValue;
	public static long copiedLongValue;

	public static final int copyValue(int value) {
		return (copiedIntValue = value);
	}

	public static final long copyValue(long value) {
		return (copiedLongValue = value);
	}

	public static final int div(int op1, int op2) {
		return op1 / op2;
	}

	public static final long div(long op1, int op2) {
		return op1 / op2;
	}

	public static final long div(int op1, long op2) {
		return op1 / op2;
	}

	public static final long div(long op1, long op2) {
		return op1 / op2;
	}

	public static final int rem(int op1, int op2) {
		return op1 % op2;
	}

	public static final long rem(long op1, int op2) {
		return op1 % op2;
	}

	public static final long rem(int op1, long op2) {
		return op1 % op2;
	}

	public static final long rem(long op1, long op2) {
		return op1 % op2;
	}

	public static final int checkDivIByI(int dividend, int divisor) {
		return dividend / divisor;
	}

	public static final long checkDivI2LByI2La(int dividend, int divisor) {
		return ((long) dividend) / divisor;
	}

	public static final long checkDivI2LByI2Lb(int dividend, int divisor) {
		return dividend / ((long) divisor);
	}

	public static final long checkDivIByL(int dividend, long divisor) {
		return dividend / divisor;
	}

	public static final long checkDivLByI(long dividend, int divisor) {
		return dividend / divisor;
	}

	public static final long checkDivLByL(long dividend, long divisor) {
		return dividend / divisor;
	}

	public static final int checkDivDivIByIByI(int dividend, int divisor1, int divisor2) {
		return (dividend / divisor1) / divisor2;
	}

	public static final long checkDivDivLByLByL(long dividend, long divisor1, long divisor2) {
		return (dividend / divisor1) / divisor2;
	}

	public static final int checkDivIByDivIByI(int dividend1, int dividend2, int divisor) {
		return dividend1 / (dividend2 / divisor);
	}

	public static final long checkDivLByDivLByL(long dividend1, long dividend2, long divisor) {
		return dividend1 / (dividend2 / divisor);
	}

	public static final long checkDivDivLByLByDivLByL(long dividend1, long dividend2, long divisor1, long divisor2) {
		return (dividend1 / divisor1) / (dividend2 / divisor2);
	}

	public static final int checkDivDivIByIByDivIByI(int dividend1, int dividend2, int divisor1, int divisor2) {
		return (dividend1 / divisor1) / (dividend2 / divisor2);
	}

	public static final int checkRemIByI(int dividend, int divisor) {
		return dividend % divisor;
	}

	public static final long checkRemI2LByI2La(int dividend, int divisor) {
		return ((long) dividend) % divisor;
	}

	public static final long checkRemI2LByI2Lb(int dividend, int divisor) {
		return dividend % ((long) divisor);
	}

	public static final long checkRemIByL(int dividend, long divisor) {
		return dividend % divisor;
	}

	public static final long checkRemLByI(long dividend, int divisor) {
		return dividend % divisor;
	}

	public static final long checkRemLByL(long dividend, long divisor) {
		return dividend % divisor;
	}

	public static final int checkRemRemIByIByI(int dividend, int divisor1, int divisor2) {
		return (dividend % divisor1) % divisor2;
	}

	public static final long checkRemRemLByLByL(long dividend, long divisor1, long divisor2) {
		return (dividend % divisor1) % divisor2;
	}

	public static final int checkRemIByRemIByI(int dividend1, int dividend2, int divisor) {
		return dividend1 % (dividend2 % divisor);
	}

	public static final long checkRemLByRemLByL(long dividend1, long dividend2, long divisor) {
		return dividend1 % (dividend2 % divisor);
	}

	public static final long checkRemRemLByLByRemLByL(long dividend1, long dividend2, long divisor1, long divisor2) {
		return (dividend1 % divisor1) % (dividend2 % divisor2);
	}

	public static final int checkRemRemIByIByRemIByI(int dividend1, int dividend2, int divisor1, int divisor2) {
		return (dividend1 % divisor1) % (dividend2 % divisor2);
	}

	static class iiSecondOpClass {
		public int op2;
		public boolean expectException;

		public iiSecondOpClass(int op2, boolean expectException) {
			this.op2 = op2;
			this.expectException = expectException;
		}
	}

	public static iiSecondOpClass[] iiSecondOp = {
			new iiSecondOpClass(0, true), new iiSecondOpClass(1, false),
			new iiSecondOpClass(-1, false), new iiSecondOpClass(16, false),
			new iiSecondOpClass(10, false), new iiSecondOpClass(100, false) };

	public static int dispatchToIIRI(int meth, int op1, int op2) throws Throwable {
		switch (meth) {
		case 0: {
			return div(op1, op2);
		}
		case 1: {
			return rem(op1, op2);
		}
		case 2: {
			return copyValue(div(copyValue(op1), copyValue(op2)));
		}
		case 3: {
			return copyValue(rem(copyValue(op1), copyValue(op2)));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0;
	}

	public static int dispatchToIIRIArgCase(int meth, int op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			ee.expect = true;
			return dispatchToIIRI(meth, op1, 0);
		}
		case 1: {
			return dispatchToIIRI(meth, op1, 1);
		}
		case 2: {
			return dispatchToIIRI(meth, op1, -1);
		}
		case 3: {
			return dispatchToIIRI(meth, op1, 16);
		}
		case 4: {
			return dispatchToIIRI(meth, op1, 10);
		}
		case 5: {
			return dispatchToIIRI(meth, op1, 100);
		}
		default: {
			ee.expect = iiSecondOp[argCase-6].expectException;
			return dispatchToIIRI(meth, op1, iiSecondOp[argCase-6].op2);
		}
		}
	}

	@Test
	public void testForDivisionByZeroIntArithmeticException() throws Throwable {
		for (int i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 12; argCase++) {
				for (int meth = 0; meth < 4; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToIIRIArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	public static long dispatchToIIRL(int meth, int op1, int op2) throws Throwable {
		switch (meth) {
		case 0: {
			return div((long) op1, (long) op2);
		}
		case 1: {
			return copyValue(div(copyValue((long) op1), copyValue((long) op2)));
		}
		case 2: {
			return rem((long) op1, (long) op2);
		}
		case 3: {
			return copyValue(rem(copyValue((long) op1), copyValue((long) op2)));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0L;
	}

	public static long dispatchToIIRLArgCase(int meth, int op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			ee.expect = true;
			return dispatchToIIRL(meth, op1, 0);
		}
		case 1: {
			return dispatchToIIRL(meth, op1, 1);
		}
		case 2: {
			return dispatchToIIRL(meth, op1, -1);
		}
		case 3: {
			return dispatchToIIRL(meth, op1, 16);
		}
		case 4: {
			return dispatchToIIRL(meth, op1, 10);
		}
		case 5: {
			return dispatchToIIRL(meth, op1, 100);
		}
		default: {
			ee.expect = iiSecondOp[argCase-6].expectException;
			return dispatchToIIRL(meth, op1, iiSecondOp[argCase-6].op2);
		}
		}
	}

	@Test
	public static void testForRemainderDivisionByZeroIntCastToLongArithmeticException() throws Throwable {
		for (int i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 12; argCase++) {
				for (int meth = 0; meth < 4; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToIIRLArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	public static long dispatchToIIIRI(int meth, int op1, int op2, int op3) throws Throwable {
		switch (meth) {
		case 0: {
			return div(op1, div(op2, op3));
		}
		case 1: {
			return div(div(op1, op2), op3);
		}
		case 2: {
			return rem(op1, rem(op2, op3));
		}
		case 3: {
			return rem(op1, rem(op2, op3));
		}
		case 4: {
			return copyValue(div(copyValue(op1), copyValue(div(copyValue(op2), copyValue(op3)))));
		}
		case 5: {
			return copyValue(div(copyValue(div(copyValue(op1), copyValue(op2))), copyValue(op3)));
		}
		case 6: {
			return copyValue(rem(copyValue(op1), copyValue(rem(copyValue(op2), copyValue(op3)))));
		}
		case 7: {
			return copyValue(rem(copyValue(rem(copyValue(op1), copyValue(op2))), copyValue(op3)));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0;
	}

	static class iiiSecondAndThirdOpClass extends iiSecondOpClass {
		public int op3;

		public iiiSecondAndThirdOpClass(int op2, int op3, boolean expectException) {
			super(op2, expectException);
			this.op3 = op3;
		}
	}

	public static iiiSecondAndThirdOpClass[] iiiTrailingOps = {
			new iiiSecondAndThirdOpClass(13, 5, false), new iiiSecondAndThirdOpClass(20, 8, false),
			new iiiSecondAndThirdOpClass(0, 5, true), new iiiSecondAndThirdOpClass(5, 0, true) };

	public static long dispatchToIIIRIArgCase(int meth, int op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			return dispatchToIIIRI(meth, op1, 13, 5);
		}
		case 1: {
			return dispatchToIIIRI(meth, op1, 20, 8);
		}
		case 2: {
			ee.expect = true;
			return dispatchToIIIRI(meth, op1, 0, 5);
		}
		case 3: {
			ee.expect = true;
			return dispatchToIIIRI(meth, op1, 5, 0);
		}
		default: {
			ee.expect = iiiTrailingOps[argCase-4].expectException;
			return dispatchToIIIRI(meth, op1, iiiTrailingOps[argCase-4].op2,
								   iiiTrailingOps[argCase-4].op3);
		}
		}
	}

	@Test
	public void testForRemainderDivisionByZeroTwiceIntArithmeticException() throws Throwable {
		for (int i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 8; argCase++) {
				for (int meth = 0; meth < 8; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToLLLRLArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	static class iiiiSecondThirdAndFourthOpClass extends iiiSecondAndThirdOpClass {
		public int op4;

		public iiiiSecondThirdAndFourthOpClass(int op2, int op3, int op4, boolean expectException) {
			super(op2, op3, expectException);
			this.op4 = op4;
		}
	}

	public static iiiiSecondThirdAndFourthOpClass[] iiiiTrailingOps = {
			new iiiiSecondThirdAndFourthOpClass(32, 13, 5, false), new iiiiSecondThirdAndFourthOpClass(65, 20, 8, false),
			new iiiiSecondThirdAndFourthOpClass(0, 13, 5, true), new iiiiSecondThirdAndFourthOpClass(32, 0, 5, true),
			new iiiiSecondThirdAndFourthOpClass(32, 5, 0, true) };


	public static long dispatchToIIIIRI(int meth, int op1, int op2, int op3, int op4) throws Throwable {
		switch (meth) {
		case 0: {
			return div(div(op1, op2), div(op3, op4));
		}
		case 1: {
			return rem(rem(op1, op2), rem(op3, op4));
		}
		case 2: {
			return copyValue(div(copyValue(div(copyValue(op1), copyValue(op2))),
								 copyValue(div(copyValue(op3), copyValue(op4)))));
		}
		case 3: {
			return copyValue(rem(copyValue(rem(copyValue(op1), copyValue(op2))),
								 copyValue(rem(copyValue(op3), copyValue(op4)))));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0;
	}

	public static long dispatchToIIIIRIArgCase(int meth, int op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			return dispatchToIIIIRI(meth, op1, 32, iiiiTrailingOps[argCase].op3, 5);
		}
		case 1: {
			return dispatchToIIIIRI(meth, op1, 65, iiiiTrailingOps[argCase].op3, 8);
		}
		case 2: {
			ee.expect = true;
			return dispatchToIIIIRI(meth, op1, 0, iiiiTrailingOps[argCase].op3, 5);
		}
		case 3: {
			ee.expect = true;
			return dispatchToIIIIRI(meth, op1, 32, iiiiTrailingOps[argCase].op3, 5);
		}
		case 4: {
			ee.expect = true;
			return dispatchToIIIIRI(meth, op1, 32, iiiiTrailingOps[argCase].op3, 0);
		}
		case 5:
		case 6:
		case 7:
		case 8:
		case 9: {
			ee.expect = iiiiTrailingOps[argCase-5].expectException;
			return dispatchToIIIIRI(meth, op1, iiiiTrailingOps[argCase-5].op2,
									iiiiTrailingOps[argCase-5].op3, iiiiTrailingOps[argCase-5].op4);
		}
		case 10: {
			return dispatchToIIIIRI(meth, Integer.MIN_VALUE, -1, 47, 32);
		}
		case 11: {
			return dispatchToIIIIRI(meth, 65535, 17, 47, -32);
		}
		case 12: {
			return dispatchToIIIIRI(meth, iiiiTrailingOps.length, 4, -47, 32);
		}
		case 13: {
			return dispatchToIIIIRI(meth, -iiiiTrailingOps.length, 4, -47, -32);
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}

		return 0;
	}

	@Test
	public void testForRemainderDivisionByZeroThriceIntArithmeticException() throws Throwable {
		for (int i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 14; argCase++) {
				for (int meth = 0; meth < 4; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToIIIIRIArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	static class llSecondOpClass {
		public long op2;
		public boolean expectException;

		public llSecondOpClass(long op2, boolean expectException) {
			this.op2 = op2;
			this.expectException = expectException;
		}
	}

	public static llSecondOpClass[] llSecondOp = {
			new llSecondOpClass(0L, true), new llSecondOpClass(1L, false),
			new llSecondOpClass(-1L, false), new llSecondOpClass(16L, false),
			new llSecondOpClass(10L, false), new llSecondOpClass(100L, false) };

	public static long dispatchToLLRL(int meth, long op1, long op2) throws Throwable {
		switch (meth) {
		case 0: {
			return div(op1, op2);
		}
		case 1: {
			return rem(op1, op2);
		}
		case 2: {
			return copyValue(div(copyValue(op1), copyValue(op2)));
		}
		case 3: {
			return copyValue(rem(copyValue(op1), copyValue(op2)));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0L;
	}

	public static long dispatchToLLRLArgCase(int meth, long op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			ee.expect = true;
			return dispatchToLLRL(meth, op1, 0L);
		}
		case 1: {
			return dispatchToLLRL(meth, op1, 1L);
		}
		case 2: {
			return dispatchToLLRL(meth, op1, -1L);
		}
		case 3: {
			return dispatchToLLRL(meth, op1, 16L);
		}
		case 4: {
			return dispatchToLLRL(meth, op1, 10L);
		}
		case 5: {
			return dispatchToLLRL(meth, op1, 100L);
		}
		default: {
			ee.expect = llSecondOp[argCase-6].expectException;
			return dispatchToLLRL(meth, op1, llSecondOp[argCase-6].op2);
		}
		}
	}

	@Test
	public void testForDivisionByZeroLongArithmeticException() throws Throwable {
		for (long i = 0L; i < 100L; i++) {
			for (int argCase = 0; argCase < 12; argCase++) {
				for (int meth = 0; meth < 4; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToLLRLArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	public static long dispatchToLLLRL(int meth, long op1, long op2, long op3) throws Throwable {
		switch (meth) {
		case 0: {
			return div(op1, div(op2, op3));
		}
		case 1: {
			return div(div(op1, op2), op3);
		}
		case 2: {
			return rem(op1, rem(op2, op3));
		}
		case 3: {
			return rem(op1, rem(op2, op3));
		}
		case 4: {
			return copyValue(div(copyValue(op1), copyValue(div(copyValue(op2), copyValue(op3)))));
		}
		case 5: {
			return copyValue(div(copyValue(div(copyValue(op1), copyValue(op2))), copyValue(op3)));
		}
		case 6: {
			return copyValue(rem(copyValue(op1), copyValue(rem(copyValue(op2), copyValue(op3)))));
		}
		case 7: {
			return copyValue(rem(copyValue(rem(copyValue(op1), copyValue(op2))), copyValue(op3)));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0L;
	}

	static class lllSecondAndThirdOpClass extends llSecondOpClass {
		public long op3;

		public lllSecondAndThirdOpClass(long op2, long op3, boolean expectException) {
			super(op2, expectException);
			this.op3 = op3;
		}
	}

	public static lllSecondAndThirdOpClass[] lllTrailingOps = {
			new lllSecondAndThirdOpClass(13, 5, false), new lllSecondAndThirdOpClass(20, 8, false),
			new lllSecondAndThirdOpClass(0, 5, true), new lllSecondAndThirdOpClass(5, 0, true) };

	public static long dispatchToLLLRLArgCase(int meth, long op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			return dispatchToLLLRL(meth, op1, 13L, 5L);
		}
		case 1: {
			return dispatchToLLLRL(meth, op1, 20L, 8L);
		}
		case 2: {
			ee.expect = true;
			return dispatchToLLLRL(meth, op1, 0L, 5L);
		}
		case 3: {
			ee.expect = true;
			return dispatchToLLLRL(meth, op1, 5L, 0L);
		}
		default: {
			ee.expect = lllTrailingOps[argCase-4].expectException;
			return dispatchToLLLRL(meth, op1, lllTrailingOps[argCase-4].op2,
								   lllTrailingOps[argCase-4].op3);
		}
		}
	}

	@Test
	public void testForRemainderDivisionByZeroTwiceLongArithmeticException() throws Throwable {
		for (long i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 8; argCase++) {
				for (int meth = 0; meth < 8; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToLLLRLArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}

	static class llllSecondThirdAndFourthOpClass extends lllSecondAndThirdOpClass {
		public long op4;

		public llllSecondThirdAndFourthOpClass(long op2, long op3, long op4, boolean expectException) {
			super(op2, op3, expectException);
			this.op4 = op4;
		}
	}

	public static llllSecondThirdAndFourthOpClass[] llllTrailingOps = {
			new llllSecondThirdAndFourthOpClass(32L, 13L, 5L, false), new llllSecondThirdAndFourthOpClass(65L, 20L, 8L, false),
			new llllSecondThirdAndFourthOpClass(0L, 13L, 5L, true), new llllSecondThirdAndFourthOpClass(32L, 0L, 5L, true),
			new llllSecondThirdAndFourthOpClass(32L, 5L, 0L, true) };


	public static long dispatchToLLLLRL(int meth, long op1, long op2, long op3, long op4) throws Throwable {
		switch (meth) {
		case 0: {
			return div(div(op1, op2), div(op3, op4));
		}
		case 1: {
			return rem(rem(op1, op2), rem(op3, op4));
		}
		case 2: {
			return copyValue(div(copyValue(div(copyValue(op1), copyValue(op2))),
								 copyValue(div(copyValue(op3), copyValue(op4)))));
		}
		case 3: {
			return copyValue(rem(copyValue(rem(copyValue(op1), copyValue(op2))),
								 copyValue(rem(copyValue(op3), copyValue(op4)))));
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0L;
	}

	public static long dispatchToLLLLRLArgCase(int meth, long op1, int argCase, ShouldExpectException ee) throws Throwable {
		ee.expect = false;

		switch (argCase) {
		case 0: {
			return dispatchToLLLLRL(meth, op1, 32L, llllTrailingOps[argCase].op3, 5L);
		}
		case 1: {
			return dispatchToLLLLRL(meth, op1, 65L, llllTrailingOps[argCase].op3, 8L);
		}
		case 2: {
			ee.expect = true;
			return dispatchToLLLLRL(meth, op1, 0L, llllTrailingOps[argCase].op3, 5L);
		}
		case 3: {
			ee.expect = true;
			return dispatchToLLLLRL(meth, op1, 32L, llllTrailingOps[argCase].op3, 5L);
		}
		case 4: {
			ee.expect = true;
			return dispatchToLLLLRL(meth, op1, 32L, llllTrailingOps[argCase].op3, 0L);
		}
		case 5:
		case 6:
		case 7:
		case 8:
		case 9: {
			ee.expect = llllTrailingOps[argCase-5].expectException;
			return dispatchToLLLLRL(meth, op1, llllTrailingOps[argCase-5].op2,
									llllTrailingOps[argCase-5].op3, llllTrailingOps[argCase-5].op4);
		}
		case 10: {
			return dispatchToLLLLRL(meth, Long.MIN_VALUE, -1, 47L, 32L);
		}
		case 11: {
			return dispatchToLLLLRL(meth, 65535L, 17L, 47L, -32L);
		}
		case 12: {
			return dispatchToLLLLRL(meth, (long)llllTrailingOps.length, 4L, -47L, 32L);
		}
		case 13: {
			return dispatchToLLLLRL(meth, - (long)llllTrailingOps.length, 4L, -47L, -32L);
		}
		default: {
			AssertJUnit.assertTrue(false);
		}
		}
		return 0L;
	}

	@Test
	public void testForRemainderDivisionByZeroThriceLongArithmeticException() throws Throwable {
		for (long i = 0; i < 100; i++) {
			for (int argCase = 0; argCase < 14; argCase++) {
				for (int meth = 0; meth < 4; meth++) {
					boolean caughtException = false;
					ShouldExpectException ee = new ShouldExpectException();

					try {
						dispatchToLLLLRLArgCase(meth, i, argCase, ee);
					} catch (ArithmeticException ae) {
						// Exception must be an instance of ArithmeticException
						// itself, not a subclass of ArithmeticException
						AssertJUnit.assertEquals(ae.getClass(), ArithmeticException.class);
						AssertJUnit.assertTrue(ee.expect);
						caughtException = true;
					}

					AssertJUnit.assertEquals(ee.expect, caughtException);
				}
			}
		}
	}
}
