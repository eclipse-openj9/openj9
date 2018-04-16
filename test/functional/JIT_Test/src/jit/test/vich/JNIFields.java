/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class JNIFields {
	private static Logger logger = Logger.getLogger(JNIFields.class);
	Timer timer;
	static final int loopCount = 1000000;

	public JNIFields() {
		timer = new Timer ();
	}
	
	/*
	#(boolean byte char short int long float double Object) do: [:type | | Type |
	Type := (type first asUppercase asString) , (type copyFrom: 2 to: type size).
	Transcript
		cr;
		cr; tab; show: ('public static %1 static%2;' bindWith: type with: Type);
		cr; tab; show: ('public native void setStatic%2(int count, %1 value);' bindWith: type with: Type);
		cr; tab; show: ('public native %1 getStatic%2(int count);' bindWith: type with: Type);
		cr;
		cr; tab; show: ('public %1 instance%2;' bindWith: type with: Type);
		cr; tab; show: ('public native void setInstance%2(int count, %1 value);' bindWith: type with: Type);
		cr; tab; show: ('public native %1 getInstance%2(int count);' bindWith: type with: Type) ]
	*/



	public static boolean staticBoolean;
	public native void setStaticBoolean(int count, boolean value);
	public native boolean getStaticBoolean(int count);

	public boolean instanceBoolean;
	public native void setInstanceBoolean(int count, boolean value);
	public native boolean getInstanceBoolean(int count);

	public static byte staticByte;
	public native void setStaticByte(int count, byte value);
	public native byte getStaticByte(int count);

	public byte instanceByte;
	public native void setInstanceByte(int count, byte value);
	public native byte getInstanceByte(int count);

	public static char staticChar;
	public native void setStaticChar(int count, char value);
	public native char getStaticChar(int count);

	public char instanceChar;
	public native void setInstanceChar(int count, char value);
	public native char getInstanceChar(int count);

	public static short staticShort;
	public native void setStaticShort(int count, short value);
	public native short getStaticShort(int count);

	public short instanceShort;
	public native void setInstanceShort(int count, short value);
	public native short getInstanceShort(int count);

	public static int staticInt;
	public native void setStaticInt(int count, int value);
	public native int getStaticInt(int count);

	public int instanceInt;
	public native void setInstanceInt(int count, int value);
	public native int getInstanceInt(int count);

	public static long staticLong;
	public native void setStaticLong(int count, long value);
	public native long getStaticLong(int count);

	public long instanceLong;
	public native void setInstanceLong(int count, long value);
	public native long getInstanceLong(int count);

	public static float staticFloat;
	public native void setStaticFloat(int count, float value);
	public native float getStaticFloat(int count);

	public float instanceFloat;
	public native void setInstanceFloat(int count, float value);
	public native float getInstanceFloat(int count);

	public static double staticDouble;
	public native void setStaticDouble(int count, double value);
	public native double getStaticDouble(int count);

	public double instanceDouble;
	public native void setInstanceDouble(int count, double value);
	public native double getInstanceDouble(int count);

	public static Object staticObject;
	public native void setStaticObject(int count, Object value);
	public native Object getStaticObject(int count);

	public Object instanceObject;
	public native void setInstanceObject(int count, Object value);
	public native Object getInstanceObject(int count);
	

public native void setStaticBoolean(boolean value);

@Test(groups = { "level.sanity","component.jit" })
public void testJNIFields() {
	int i;
	try {
		System.loadLibrary("j9ben");
		setStaticBoolean(0, true);
		getStaticBoolean(0);
		setInstanceBoolean(0, true);
		getInstanceBoolean(0);
		setStaticByte(0, (byte)69);
		getStaticByte(0);
		setInstanceByte(0, (byte)69);
		getInstanceByte(0);
		setStaticChar(0, 'X');
		getStaticChar(0);
		setInstanceChar(0, 'X');
		getInstanceChar(0);
		setStaticShort(0, (short)8096);
		getStaticShort(0);
		setInstanceShort(0, (short)8096);
		getInstanceShort(0);
		setStaticInt(0, 0xDEADBEEF);
		getStaticInt(0);
		setInstanceInt(0, 0xDEADBEEF);
		getInstanceInt(0);
		setStaticLong(0, 0xDEADBEEFFB1C1A00L);
		getStaticLong(0);
		setInstanceLong(0, 0xDEADBEEFFB1C1A00L);
		getInstanceLong(0);
		setStaticFloat(0, 1.0f);
		getStaticFloat(0);
		setInstanceFloat(0, 1.0f);
		getInstanceFloat(0);
		setStaticDouble(0, 3.14159);
		getStaticDouble(0);
		setInstanceDouble(0, 3.14159);
		getInstanceDouble(0);
		setStaticObject(0, this);
		getStaticObject(0);
		setInstanceObject(0, this);
		getInstanceObject(0);
	} catch (UnsatisfiedLinkError e) {
		Assert.fail("No natives for JNI tests");
	}
	
	timer.reset();
	setStaticBoolean(loopCount, false);
	timer.mark();
	logger.info("setStaticBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticBoolean(loopCount);
	timer.mark();
	logger.info("getStaticBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceBoolean(loopCount, false);
	timer.mark();
	logger.info("setInstanceBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceBoolean(loopCount);
	timer.mark();
	logger.info("getInstanceBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticByte(loopCount, (byte)69);
	timer.mark();
	logger.info("setStaticByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticByte(loopCount);
	timer.mark();
	logger.info("getStaticByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceByte(loopCount, (byte)69);
	timer.mark();
	logger.info("setInstanceByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceByte(loopCount);
	timer.mark();
	logger.info("getInstanceByte(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticChar(loopCount, 'X');
	timer.mark();
	logger.info("setStaticChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticChar(loopCount);
	timer.mark();
	logger.info("getStaticChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceChar(loopCount, 'X');
	timer.mark();
	logger.info("setInstanceChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceChar(loopCount);
	timer.mark();
	logger.info("getInstanceChar(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticShort(loopCount, (short)8096);
	timer.mark();
	logger.info("setStaticShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticShort(loopCount);
	timer.mark();
	logger.info("getStaticShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceShort(loopCount, (short)8096);
	timer.mark();
	logger.info("setInstanceShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceShort(loopCount);
	timer.mark();
	logger.info("getInstanceShort(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticInt(loopCount, 0xDEADBEEF);
	timer.mark();
	logger.info("setStaticInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticInt(loopCount);
	timer.mark();
	logger.info("getStaticInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceInt(loopCount, 0xDEADBEEF);
	timer.mark();
	logger.info("setInstanceInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceInt(loopCount);
	timer.mark();
	logger.info("getInstanceInt(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticLong(loopCount, 0xDEADBEEFFB1C1A00L);
	timer.mark();
	logger.info("setStaticLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticLong(loopCount);
	timer.mark();
	logger.info("getStaticLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceLong(loopCount, 0xDEADBEEFFB1C1A00L);
	timer.mark();
	logger.info("setInstanceLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceLong(loopCount);
	timer.mark();
	logger.info("getInstanceLong(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticFloat(loopCount, 1.0f);
	timer.mark();
	logger.info("setStaticFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticFloat(loopCount);
	timer.mark();
	logger.info("getStaticFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceFloat(loopCount, 1.0f);
	timer.mark();
	logger.info("setInstanceFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceFloat(loopCount);
	timer.mark();
	logger.info("getInstanceFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticDouble(loopCount, 3.14159);
	timer.mark();
	logger.info("setStaticDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticDouble(loopCount);
	timer.mark();
	logger.info("getStaticDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceDouble(loopCount, 3.14159);
	timer.mark();
	logger.info("setInstanceDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceDouble(loopCount);
	timer.mark();
	logger.info("getInstanceDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));


	timer.reset();
	setStaticObject(loopCount, this);
	timer.mark();
	logger.info("setStaticObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getStaticObject(loopCount);
	timer.mark();
	logger.info("getStaticObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	setInstanceObject(loopCount, this);
	timer.mark();
	logger.info("setInstanceObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	getInstanceObject(loopCount);
	timer.mark();
	logger.info("getInstanceObject(" + loopCount + "); = "+ Long.toString(timer.delta()));
}
}
