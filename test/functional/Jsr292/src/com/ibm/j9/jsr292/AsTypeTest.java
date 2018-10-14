package com.ibm.j9.jsr292;


/*******************************************************************************
 * Copyright (c) 2011, 2018 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.io.Serializable;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

import junit.framework.AssertionFailedError;
import junit.framework.TestListener;
import junit.framework.TestResult;

/**
 * Testing of the various aspects of the the MethodHandle.asType() method,
 * both explicitly and as exposed by various APIs.
 */
public class AsTypeTest {

	static boolean print = false; // Turns the printLn method into a no-op

	boolean explicit = false;
	boolean passed = true;

	public AsTypeTest(String name) {
	}

	public AsTypeTest(int explicit) {
		if (explicit == 1) {
			this.explicit = true;
		} else {
			this.explicit = false;
		}
	}

	/*
	 * CMVC : 192649
	 *
	 * Oracle-J9 mismatch in casting arguments
	 */
	@Test(groups = { "level.extended" })
	public void testAll() throws Throwable {
		System.out.println("testing asType");
		boolean passed = true;
		System.out.println("Testing implicit casting");
		AsTypeTest testObject = new AsTypeTest(0);
		testObject.doTestVoid();
		testObject.doTestReference();
		testObject.doTestByte();
		testObject.doTestBoolean();
		testObject.doTestShort();
		testObject.doTestChar();
		testObject.doTestInt();
		testObject.doTestLong();
		testObject.doTestFloat();
		testObject.doTestDouble();
		if (!testObject.passed) {
			passed = false;
		}

		System.out.println("Testing explicit casting");

		AsTypeTest testObject2 = new AsTypeTest(1);
		testObject2.doTestVoid();
		testObject2.doTestReference();
		testObject2.doTestByte();
		testObject2.doTestBoolean();
		testObject2.doTestShort();
		testObject2.doTestChar();
		testObject2.doTestInt();
		testObject2.doTestLong();
		testObject2.doTestFloat();
		testObject2.doTestDouble();

		if (!testObject2.passed) {
			passed = false;
		}
		AssertJUnit.assertTrue(passed);
	}

	static boolean floatsEqual(float a, float b) {
		Float f1 = new Float(a);
		Float f2 = new Float(b);

		if (f1.isNaN()) {
			return f2.isNaN();
		}
		if (f2.isNaN()) {
			return false;
		}
		return a == b;
	}

	static boolean floatsEqual(double a, double b) {
		Double d1 = new Double(a);
		Double d2 = new Double(b);

		if (d1.isNaN()) {
			return d2.isNaN();
		}
		if (d2.isNaN()) {
			return false;
		}
		return a == b;
	}

	static void println(String s) {
		if (print) {
			System.out.println(s);
		}
	}

	static boolean castToBoolean(boolean v) {
		return v;
	}

	static boolean castToBoolean(byte v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(short v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(char v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(int v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(long v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(float v) {
		return (((byte)v) & 1) != 0;
	}

	static boolean castToBoolean(double v) {
		return (((byte)v) & 1) != 0;
	}

	static int intFromBoolean(boolean v) {
		return (int)(v ? 1 : 0);
	}

	static long longFromBoolean(boolean v) {
		return (long)(v ? 1 : 0);
	}

	static float floatFromBoolean(boolean v) {
		return (float)(v ? 1 : 0);
	}

	static double doubleFromBoolean(boolean v) {
		return (double)(v ? 1 : 0);
	}

	static byte byteFromBoolean(boolean v) {
		return (byte)(v ? 1 : 0);
	}

	static short shortFromBoolean(boolean v) {
		return (short)(v ? 1 : 0);
	}

	static char charFromBoolean(boolean v) {
		return (char)(v ? 1 : 0);
	}

	public MethodHandle cast(MethodHandle mh, MethodType mt) throws Throwable {
		println("calling " + mh.type() + " as " + mt);
		if (explicit) {
			return MethodHandles.explicitCastArguments(mh, mt);
		} else {
			return mh.asType(mt);
		}
	}

	public void doTest() {
	}

	public byte doTest(byte v) {
		println("byte = " + v);
		return v;
	}

	public boolean doTest(boolean v) {
		println("boolean = " + v);
		return v;
	}

	public short doTest(short v) {
		println("short = " + v);
		return v;
	}

	public char doTest(char v) {
		println("char = " + (int)v);
		return v;
	}

	public int doTest(int v) {
		println("int = " + v);
		return v;
	}

	public float doTest(float v) {
		println("float = " + v);
		return v;
	}

	public double doTest(double v) {
		println("double = " + v);
		return v;
	}

	public long doTest(long v) {
		println("long = " + v);
		return v;
	}

	public Object doTest(Object v) {
		println("Object = " + v);
		return v;
	}

	public Object doTest(Number v) {
		println("Number = " + v);
		return v;
	}

	public Object doTest(String v) {
		println("String = " + v);
		return v;
	}

	public Object doTest(Serializable v) {
		println("Serializable = " + v);
		return v;
	}

	public String testString(String v) {
		println("String = " + v);
		return v;
	}


	public void exceptionDuringCast(MethodHandle mh, MethodType mt, Throwable t) {
		System.out.println("Casting " + mh.type() + " to " + mt + " -> " + t);
		passed = false;
		t.printStackTrace();
	}

	public void exceptionDuringInvoke(MethodHandle mh, MethodType mt, Throwable t) {
		System.out.println("Invoking " + mh.type() + " as " + mt + " -> " + t);
		passed = false;
		t.printStackTrace();
	}

	public void expectedExceptionDuringCast(MethodHandle mh, MethodType mt, Throwable t) {
		println("Casting " + mh.type() + " to " + mt + " -> got expected exception " + t);
	}

	public void expectedExceptionDuringInvoke(MethodHandle mh, MethodType mt, Throwable t) {
		println("Invoking " + mh.type() + " as " + mt + " -> got expected exception " + t);
	}

	public void noNPEDuringInvoke(MethodHandle mh, MethodType mt) {
		System.out.println("Invoking " + mh.type() + " as " + mt + " -> did not throw NPE");
		passed = false;
	}

	public void noClassCastDuringInvoke(MethodHandle mh, MethodType mt) {
		System.out.println("Invoking " + mh.type() + " as " + mt + " -> did not throw ClassCastException");
		passed = false;
	}

	public void noExceptionDuringCast(MethodHandle mh, MethodType mt) {
		System.out.println("Casting " + mh.type() + " to " + mt + " -> did not throw WrongMethodypeException");
		passed = false;
	}

	public void compare(MethodHandle mh, MethodType mt, boolean expected, boolean result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, byte expected, byte result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, char expected, char result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + (int)expected + " got " + (int)result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, short expected, short result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, int expected, int result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, long expected, long result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, float expected, float result) {
		if (!floatsEqual(expected, result)) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, double expected, double result) {
		if (!floatsEqual(expected, result)) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void compare(MethodHandle mh, MethodType mt, Object expected, Object result) {
		if (expected != result) {
			System.out.println("Invoking " + mh.type() + " as " + mt + " -> expected " + expected + " got " + result);
			passed = false;
		}
	}

	public void doTestVoid() throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;

		System.out.println("\ttesting void");


		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (int)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)0, (int)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (long)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)0, (long)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (float)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)0, (float)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (double)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)0, (double)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (byte)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)0, (byte)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (boolean)false);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, false, (boolean)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (char)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)0, (char)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (short)0);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)0, (short)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)V", null);
		try {
			c = cast(s, mt);
			try {
				c.invokeExact(this, (Object)null);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("()V", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, null, (Object)c.invokeExact(this));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestReference() throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;

		System.out.println("\ttesting reference");

		/* unboxing NULL should give 0 for explicit casting, otherwise NPE */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				int result = (int)c.invokeExact(this, (Object) null);
				if (explicit) {
					compare(s, mt, 0, result);
				} else {
					noNPEDuringInvoke(s, mt);
				}
			} catch (NullPointerException t) {
				if (explicit) {
					// This should return zero, not throw a NPE
					throw t;
				}
				expectedExceptionDuringInvoke(s, mt, t);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* widening reference cast */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/String;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				Object expected = "argle";
				Object result = (Object)c.invokeExact(this, (String) expected);
				compare(s, mt, expected, result);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* narrowing reference cast */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				Object result = (Object)c.invokeExact(this, (Object) new Object());
				noClassCastDuringInvoke(s, mt);
			} catch (ClassCastException t) {
				expectedExceptionDuringInvoke(s, mt, t);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* narrowing reference cast of null */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, null, (Object)c.invokeExact(this, (Object) null));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* cast to implemented interface */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/io/Serializable;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/String;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, "argle", (Object)c.invokeExact(this, (String) "argle"));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* cast to unimplemented interface */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/io/Serializable;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				Object o = new Object();
				compare(s, mt, o, (Object)c.invokeExact(this, (Object) o));
				if (!explicit) {
					noClassCastDuringInvoke(s, mt);
				}
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - unboxing NULL should give 0 for explicit casting, NPE otherwise*/

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				int result = (int)c.invokeExact(this, (Object) null);
				if (explicit) {
					compare(s, mt, 0, result);
				} else {
					noNPEDuringInvoke(s, mt);
				}
			} catch (NullPointerException t) {
				if (explicit) {
					// explicit casting should give a 0 argument, not a NPE
					throw t;
				}
				expectedExceptionDuringInvoke(s, mt, t);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - widening reference cast */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "testString", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/String;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/String;)Ljava/lang/Object;", null);
		try {
			c = cast(s, mt);
			try {
				Object expected = "argle";
				Object result = (Object)c.invokeExact(this, (String) expected);
				compare(s, mt, expected, result);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - narrowing reference cast */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/lang/String;", null);
		try {
			c = cast(s, mt);
			try {
				String result = (String)c.invokeExact(this, new Object());
				noClassCastDuringInvoke(s, mt);
			} catch (ClassCastException t) {
				expectedExceptionDuringInvoke(s, mt, t);
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - narrowing reference cast of null */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/lang/String;", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, null, (Object)(String)c.invokeExact(this, (Object) null));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - cast to implemented interface */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/String;)Ljava/io/Serializable;", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, "argle", (String)(Serializable)c.invokeExact(this, (String) "argle"));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* return - cast to unimplemented interface */

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Ljava/io/Serializable;", null);
		try {
			c = cast(s, mt);
			try {
				Object o = new Object();
				compare(s, mt, o, (Object)(Serializable)c.invokeExact(this, (Object) o));
				if (!explicit) {
					noClassCastDuringInvoke(s, mt);
				}
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestBoolean() throws Throwable {
		System.out.println("\ttesting boolean");
		doTestConvertBoolean(false);
		doTestConvertBoolean(true);
	}

	public void doTestByte() throws Throwable {
		System.out.println("\ttesting byte");
		doTestConvertByte((byte)0);
		doTestConvertByte((byte)1);
		doTestConvertByte((byte)-1);
		doTestConvertByte(Byte.MIN_VALUE);
		doTestConvertByte(Byte.MAX_VALUE);
	}

	public void doTestShort() throws Throwable {
		System.out.println("\ttesting short");
		doTestConvertShort((short)0);
		doTestConvertShort((short)1);
		doTestConvertShort((short)-1);
		doTestConvertShort(Byte.MIN_VALUE);
		doTestConvertShort(Byte.MAX_VALUE);
		doTestConvertShort(Short.MIN_VALUE);
		doTestConvertShort(Short.MAX_VALUE);
	}

	public void doTestChar() throws Throwable {
		System.out.println("\ttesting char");
		doTestConvertChar((char)0);
		doTestConvertChar((char)1);
		doTestConvertChar((char)Byte.MAX_VALUE);
		doTestConvertChar((char)Short.MAX_VALUE);
		doTestConvertChar(Character.MAX_VALUE);
	}

	public void doTestInt() throws Throwable {
		System.out.println("\ttesting int");
		doTestConvertInt(0);
		doTestConvertInt(1);
		doTestConvertInt(-1);
		doTestConvertInt(Byte.MIN_VALUE);
		doTestConvertInt(Byte.MAX_VALUE);
		doTestConvertInt(Short.MIN_VALUE);
		doTestConvertInt(Short.MAX_VALUE);
		doTestConvertInt(Character.MAX_VALUE);
		doTestConvertInt(Integer.MIN_VALUE);
		doTestConvertInt(Integer.MAX_VALUE);
	}

	public void doTestLong() throws Throwable {
		System.out.println("\ttesting long");
		doTestConvertLong(0);
		doTestConvertLong(1);
		doTestConvertLong(-1);
		doTestConvertLong(Byte.MIN_VALUE);
		doTestConvertLong(Byte.MAX_VALUE);
		doTestConvertLong(Short.MIN_VALUE);
		doTestConvertLong(Short.MAX_VALUE);
		doTestConvertLong(Character.MAX_VALUE);
		doTestConvertLong(Integer.MIN_VALUE);
		doTestConvertLong(Integer.MAX_VALUE);
		doTestConvertLong(Long.MIN_VALUE);
		doTestConvertLong(Long.MAX_VALUE);
	}

	public void doTestFloat() throws Throwable {
		System.out.println("\ttesting float");
		doTestConvertFloat((float)0);
		doTestConvertFloat((float)1);
		doTestConvertFloat((float)-1);
		doTestConvertFloat((float)Byte.MIN_VALUE);
		doTestConvertFloat((float)Byte.MAX_VALUE);
		doTestConvertFloat((float)Short.MIN_VALUE);
		doTestConvertFloat((float)Short.MAX_VALUE);
		doTestConvertFloat((float)Character.MAX_VALUE);
		doTestConvertFloat((float)Integer.MIN_VALUE);
		doTestConvertFloat((float)Integer.MAX_VALUE);
		doTestConvertFloat((float)Long.MIN_VALUE);
		doTestConvertFloat((float)Long.MAX_VALUE);
		doTestConvertFloat(Float.MIN_VALUE);
		doTestConvertFloat(Float.MAX_VALUE);
		doTestConvertFloat(Float.NaN);
		doTestConvertFloat(Float.NEGATIVE_INFINITY);
		doTestConvertFloat(Float.POSITIVE_INFINITY);
	}

	public void doTestDouble() throws Throwable {
		System.out.println("\ttesting double");
		doTestConvertDouble((double)0);
		doTestConvertDouble((double)1);
		doTestConvertDouble((double)-1);
		doTestConvertDouble((double)Byte.MIN_VALUE);
		doTestConvertDouble((double)Byte.MAX_VALUE);
		doTestConvertDouble((double)Short.MIN_VALUE);
		doTestConvertDouble((double)Short.MAX_VALUE);
		doTestConvertDouble((double)Character.MAX_VALUE);
		doTestConvertDouble((double)Integer.MIN_VALUE);
		doTestConvertDouble((double)Integer.MAX_VALUE);
		doTestConvertDouble((double)Long.MIN_VALUE);
		doTestConvertDouble((double)Long.MAX_VALUE);
		doTestConvertDouble(Double.MIN_VALUE);
		doTestConvertDouble(Double.MAX_VALUE);
		doTestConvertDouble(Double.NaN);
		doTestConvertDouble(Double.NEGATIVE_INFINITY);
		doTestConvertDouble(Double.POSITIVE_INFINITY);
	}



	public void doTestConvertBoolean(boolean v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Boolean(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, intFromBoolean(v), (int)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, intFromBoolean(v), (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, intFromBoolean(v), (int)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, intFromBoolean(v), (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, longFromBoolean(v), (long)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, longFromBoolean(v), (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, longFromBoolean(v), (long)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, longFromBoolean(v), (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)F", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, floatFromBoolean(v), (float)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, floatFromBoolean(v), (float)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)F", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, floatFromBoolean(v), (float)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, floatFromBoolean(v), (float)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)D", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, doubleFromBoolean(v), (double)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, doubleFromBoolean(v), (double)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)D", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, doubleFromBoolean(v), (double)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, doubleFromBoolean(v), (double)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, byteFromBoolean(v), (byte)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, byteFromBoolean(v), (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, byteFromBoolean(v), (byte)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, byteFromBoolean(v), (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, v, (boolean)c.invokeExact(this, (boolean)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, v, (boolean)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, v, (boolean)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, charFromBoolean(v), (char)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, charFromBoolean(v), (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, charFromBoolean(v), (char)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, charFromBoolean(v), (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, shortFromBoolean(v), (short)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, shortFromBoolean(v), (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, shortFromBoolean(v), (short)c.invokeExact(this, (boolean)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, shortFromBoolean(v), (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Z)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (boolean)v, ((Boolean) (Object)c.invokeExact(this, (boolean)v)).booleanValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}


	public void doTestConvertByte(byte v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Byte(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (byte)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (byte)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (byte)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (byte)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (byte)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;B)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, ((Byte) (Object)c.invokeExact(this, (byte)v)).byteValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, ((Byte) (Object)c.invokeExact(this, (byte)v)).byteValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestConvertShort(short v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Short(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (short)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (short)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;S)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, ((Short) (Object)c.invokeExact(this, (short)v)).shortValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, ((Short) (Object)c.invokeExact(this, (short)v)).shortValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}


	public void doTestConvertChar(char v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Character(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (char)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (char)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;C)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, ((Character) (Object)c.invokeExact(this, (char)v)).charValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestConvertFloat(float v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Float(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (long)v, (long)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (long)v, (long)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (float)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (float)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (float)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			if (explicit) {
				exceptionDuringInvoke(s, mt, t);
			} else {
				expectedExceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (float)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;F)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, ((Float) (Object)c.invokeExact(this, (float)v)).floatValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, ((Float) (Object)c.invokeExact(this, (float)v)).floatValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestConvertDouble(double v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Double(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (long)v, (long)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)J", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (long)v, (long)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)F", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (float)v, (float)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)F", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (float)v, (float)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (double)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			if (explicit) {
				exceptionDuringInvoke(s, mt, t);
			} else {
				expectedExceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (double)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;D)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, ((Double) (Object)c.invokeExact(this, (double)v)).doubleValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, ((Double) (Object)c.invokeExact(this, (double)v)).doubleValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestConvertInt(int v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Integer(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (int)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			if (explicit) {
				exceptionDuringInvoke(s, mt, t);
			} else {
				expectedExceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (int)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;I)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, ((Integer) (Object)c.invokeExact(this, (int)v)).intValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, ((Integer) (Object)c.invokeExact(this, (int)v)).intValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void doTestConvertLong(long v) throws Throwable {
		MethodHandle s;
		MethodHandle c;
		MethodType mt;
		Object o = new Long(v);

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(I)I", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)I", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (int)v, (int)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)I", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (int)v, (int)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (long)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)J", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, (long)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(F)F", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (long)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (long)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)F", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (float)v, (float)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(D)D", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (long)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (long)v));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)D", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (double)v, (double)c.invokeExact(this, (Object)o));
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(B)B", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)B", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (byte)v, (byte)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)B", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (byte)v, (byte)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Z)Z", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)Z", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)Z", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, castToBoolean(v), (boolean)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(C)C", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)C", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (char)v, (char)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)C", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (char)v, (char)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(S)S", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			if (explicit) {
				exceptionDuringInvoke(s, mt, t);
			} else {
				expectedExceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(J)J", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)S", null);
		try {
			c = cast(s, mt);
			if (explicit) {
				try {
					compare(s, mt, (short)v, (short)c.invokeExact(this, (long)v));
				} catch (Throwable t) {
					exceptionDuringInvoke(s, mt, t);
				}
			} else {
				noExceptionDuringCast(s, mt);
			}
		} catch (WrongMethodTypeException t) {
			wrongMethodType(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;Ljava/lang/Object;)S", null);
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (short)v, (short)c.invokeExact(this, (Object)o));
			} catch (ClassCastException t) {
				if (explicit) {
					exceptionDuringInvoke(s, mt, t);
				} else {
					expectedExceptionDuringInvoke(s, mt, t);
				}
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}

		/* test boxing */

		mt = MethodType.fromMethodDescriptorString("(Lcom/ibm/j9/jsr292/AsTypeTest;J)Ljava/lang/Object;", null);
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Object;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, ((Long) (Object)c.invokeExact(this, (long)v)).longValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/Number;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			try {
				compare(s, mt, (long)v, ((Long) (Object)c.invokeExact(this, (long)v)).longValue());
			} catch (Throwable t) {
				exceptionDuringInvoke(s, mt, t);
			}
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
		s = MethodHandles.lookup().findVirtual(AsTypeTest.class, "doTest", MethodType.fromMethodDescriptorString("(Ljava/lang/String;)Ljava/lang/Object;", null));
		try {
			c = cast(s, mt);
			noExceptionDuringCast(s, mt);
		} catch (WrongMethodTypeException t) {
			expectedExceptionDuringCast(s, mt, t);
		} catch (Throwable t) {
			exceptionDuringCast(s, mt, t);
		}
	}

	public void wrongMethodType(MethodHandle mh, MethodType mt, Throwable t) {
		if (explicit) {
			exceptionDuringCast(mh, mt, t);
		} else {
			expectedExceptionDuringCast(mh, mt, t);
		}
	}
}
