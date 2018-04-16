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

public class JNIArray {
	private static Logger logger = Logger.getLogger(JNIArray.class);
	Timer timer;

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}

	public JNIArray() {
		timer = new Timer ();
	}
	
	static final int loopCount = 10000;
	static final int maxArraySizeExponent = 4;
	
	public native void getIntArrayElements(int[] array, int loopCount);
	public native void getByteArrayElements(byte[] array, int loopCount);
	public native void getLongArrayElements(long[] array, int loopCount);
	public native void getFloatArrayElements(float[] array, int loopCount);
	public native void getDoubleArrayElements(double[] array, int loopCount);
	
	public native void getPrimitiveArrayCritical(int[] array, int loopCount);

	public void fillByteArray(byte[] array)
	{
		for (int i = 0; i < array.length; i++)
			array[i] = (byte)i;
	}
	
	public void fillIntArray(int[] array)
	{
		for (int i = 0; i < array.length; i++)
			array[i] = i;
	}
	
	public void fillDoubleArray(double[] array)
	{
		for (int i = 0; i < array.length; i++)
			array[i] = (double)i;		
	}
	
	public void fillLongArray(long[] array)
	{
		for (int i = 0; i < array.length; i++)
			array[i] = (long)i;	
	}
	
		
	public void fillFloatArray(float[] array)
	{
		for (int i = 0; i < array.length; i++)
			array[i] = (float)i;
	}
	
	public void GetByteArrayElementsBench()
	{
		byte[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new byte[size];
			fillByteArray(array);
			timer.reset();
			getByteArrayElements(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleaseByteArrayElements calls (size " + size + ") = " + timer.delta());
		}
	}	
		
	public void GetIntArrayElementsBench()
	{
		int[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new int[size];
			fillIntArray(array);
			timer.reset();
			getIntArrayElements(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleaseIntArrayElements calls (size " + size + ") = " + timer.delta());
		}
	}
	
	public void GetLongArrayElementsBench()
	{
		long[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new long[size];
			fillLongArray(array);
			timer.reset();
			getLongArrayElements(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleaseLongArrayElements calls (size " + size + ") = " + timer.delta());
		}
	}
	
	public void GetFloatArrayElementsBench()
	{
		float[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new float[size];
			fillFloatArray(array);
			timer.reset();
			getFloatArrayElements(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleaseFloatArrayElements calls (size " + size + ") = " + timer.delta());
		}
	}
	
	public void GetDoubleArrayElementsBench()
	{
		double[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new double[size];
			fillDoubleArray(array);
			timer.reset();
			getDoubleArrayElements(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleaseDoubleArrayElements calls (size " + size + ") = " + timer.delta());
		}
	}

	public void GetPrimitiveArrayCriticalBench()
	{
		int[] array;
		int size;
		
		for (int i  = 0; i <= maxArraySizeExponent; i++)
		{
			size = (int)Math.pow(10, i);
			array = new int[size];
			fillIntArray(array);
			timer.reset();
			getPrimitiveArrayCritical(array, loopCount);
			timer.mark();
			logger.info(loopCount + " Get/ReleasePrimitiveArrayCritical(int) calls (size " + size + ") = " + timer.delta());
		}
		
	}

	@Test(groups = { "level.sanity","component.jit" })
	public void testJNIArray()
	{
		
		try
		{
			int[] intArray = new int[1];
			getIntArrayElements(intArray, 1);
			
			byte[] byteArray = new byte[1];
			getByteArrayElements(byteArray, 1);
			
			long[] longArray = new long[1];
			getLongArrayElements(longArray, 1);
			
			float[] floatArray = new float[1];
			getFloatArrayElements(floatArray, 1);
			
			double[] doubleArray = new double[1];
			getDoubleArrayElements(doubleArray, 1);
			
			getPrimitiveArrayCritical(intArray, 1);
			
			
		} catch (UnsatisfiedLinkError e) {
			Assert.fail("No natives for JNI tests");
		}
		
		GetByteArrayElementsBench();
		GetIntArrayElementsBench();
		GetLongArrayElementsBench();
		GetFloatArrayElementsBench();
		GetDoubleArrayElementsBench();
		
		GetPrimitiveArrayCriticalBench();
	}
	
	
	
}
