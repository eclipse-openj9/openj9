/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on May 2, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package j9vm.test.jni;

/**
 * @author willy
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class VolatileTest {

	public volatile int volatileInt;
	public volatile float volatileFloat;
	public volatile long volatileLong;
	public volatile double volatileDouble;

	public static volatile int volatileStaticInt;
	public static volatile float volatileStaticFloat;
	public static volatile long volatileStaticLong;
	public static volatile double volatileStaticDouble;

	public static void main(String[] args) {
		try {
			System.loadLibrary("j9ben");

			VolatileTest var = new VolatileTest();
			var.testInt();
			var.testFloat();
			var.testLong();
			var.testDouble();

			testStaticInt();
			testStaticFloat();
			testStaticLong();
			testStaticDouble();

		} catch (UnsatisfiedLinkError e) {
			System.out.println("Problem opening JNI library");
			e.printStackTrace();
			throw new RuntimeException();
		}
	}

	void testInt() {
		setVolatileInt(0xFAB0F00D);
		if(getVolatileInt() != 0xFAB0F00D) {
			throw new RuntimeException("volatile int problem");
		}
	}

	void testFloat() {
		setVolatileFloat(1.234567F);
		if(getVolatileFloat() != 1.234567F) {
			throw new RuntimeException("volatile float problem");
		}
	}

	void testLong() {
		setVolatileLong(0xFAB0F00DBABEBABEL);
		if(getVolatileLong() != 0xFAB0F00DBABEBABEL) {
			throw new RuntimeException("volatile long problem");
		}
	}
	void testDouble() {
		setVolatileDouble(8.765432);
		if(getVolatileDouble() != 8.765432) {
			throw new RuntimeException("volatile double problem");
		}
	}

	static void testStaticInt() {
		setVolatileStaticInt(0xFAB0F00D);
		if(getVolatileStaticInt() != 0xFAB0F00D) {
			throw new RuntimeException("volatile Static int problem");
		}
	}

	static void testStaticFloat() {
		setVolatileStaticFloat(1.234567F);
		if(getVolatileStaticFloat() != 1.234567F) {
			throw new RuntimeException("volatile Static float problem");
		}
	}

	static void testStaticLong() {
		setVolatileStaticLong(0xFAB0F00DBABEBABEL);
		if(getVolatileStaticLong() != 0xFAB0F00DBABEBABEL) {
			throw new RuntimeException("volatile Static long problem");
		}
	}
	static void testStaticDouble() {
		setVolatileStaticDouble(8.765432);
		if(getVolatileStaticDouble() != 8.765432) {
			throw new RuntimeException("volatile Static double problem");
		}
	}

	public native int getVolatileInt();
	public native float getVolatileFloat();
	public native long getVolatileLong();
	public native double getVolatileDouble();

	public native void setVolatileInt(int anInt);
	public native void setVolatileFloat(float aFloat);
	public native void setVolatileLong(long aLong);
	public native void setVolatileDouble(double aDouble);

	public static native int getVolatileStaticInt();
	public static native float getVolatileStaticFloat();
	public static native long getVolatileStaticLong();
	public static native double getVolatileStaticDouble();

	public static native void setVolatileStaticInt(int anInt);
	public static native void setVolatileStaticFloat(float aFloat);
	public static native void setVolatileStaticLong(long aLong);
	public static native void setVolatileStaticDouble(double aDouble);

}
