/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package org.openj9.test.varhandle;

import org.testng.annotations.Test;
import org.testng.Assert;

import java.lang.invoke.*;
import java.nio.*;

/**
 * Test ArrayVarHandle operations
 * 
 * @author Bjorn Vardal
 */
@Test(groups = { "level.extended" })
public class ArrayVarHandleTests {
	private final static VarHandle VH_BYTE = MethodHandles.arrayElementVarHandle(byte[].class);
	private final static VarHandle VH_CHAR = MethodHandles.arrayElementVarHandle(char[].class);
	private final static VarHandle VH_DOUBLE = MethodHandles.arrayElementVarHandle(double[].class);
	private final static VarHandle VH_FLOAT = MethodHandles.arrayElementVarHandle(float[].class);
	private final static VarHandle VH_INT = MethodHandles.arrayElementVarHandle(int[].class);
	private final static VarHandle VH_LONG = MethodHandles.arrayElementVarHandle(long[].class);
	private final static VarHandle VH_SHORT = MethodHandles.arrayElementVarHandle(short[].class);
	private final static VarHandle VH_BOOLEAN = MethodHandles.arrayElementVarHandle(boolean[].class);
	private final static VarHandle VH_STRING = MethodHandles.arrayElementVarHandle(String[].class);
	private final static VarHandle VH_CLASS = MethodHandles.arrayElementVarHandle(Class[].class);
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code byte[]}.
	 */
	@Test
	public void testByte() {
		byte[] array = null;
		array = ArrayHelper.reset(array);

		/* Get */
		byte bFromVH = (byte)VH_BYTE.get(array, 0);
		Assert.assertEquals((byte)1, bFromVH);
		
		/* Set */
		VH_BYTE.set(array, 0, (byte)4);
		Assert.assertEquals((byte)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		bFromVH = (byte) VH_BYTE.getOpaque(array, 0);
		Assert.assertEquals((byte)1, bFromVH);

		/* SetOpaque */
		VH_BYTE.setOpaque(array, 0, (byte)4);
		Assert.assertEquals((byte)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		bFromVH = (byte) VH_BYTE.getVolatile(array, 0);
		Assert.assertEquals((byte)1, bFromVH);

		/* SetVolatile */
		VH_BYTE.setVolatile(array, 0, (byte)4);
		Assert.assertEquals((byte)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		bFromVH = (byte) VH_BYTE.getAcquire(array, 0);
		Assert.assertEquals((byte)1, bFromVH);

		/* SetRelease */
		VH_BYTE.setRelease(array, 0, (byte)5);
		Assert.assertEquals((byte)5, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_BYTE.compareAndSet(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.compareAndSet(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		byte caeResult = (byte)VH_BYTE.compareAndExchange(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertEquals((byte)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchange(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertEquals((byte)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BYTE.weakCompareAndSet(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSet(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease(array, 0, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease(array, 0, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		bFromVH = (byte)VH_BYTE.getAndSet(array, 0, (byte)2);
		Assert.assertEquals(1, bFromVH);
		Assert.assertEquals(2, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		bFromVH = (byte)VH_BYTE.getAndAdd(array, 0, (byte)2);
		Assert.assertEquals(1, bFromVH);
		Assert.assertEquals(3, array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a byte[].
	 */
	@Test
	public void testByteLastElement() {
		byte[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		byte bFromVH = (byte)VH_BYTE.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], bFromVH);
		
		/* Set */
		VH_BYTE.set(array, array.length - 1, (byte)20);
		Assert.assertEquals((byte)20, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code char[]}.
	 */
	@Test
	public void testChar() {
		char[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		char cFromVH = (char)VH_CHAR.get(array, 0);
		Assert.assertEquals('1', cFromVH);
		
		/* Set */
		VH_CHAR.set(array, 0, '4');
		Assert.assertEquals('4', array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		cFromVH = (char) VH_CHAR.getOpaque(array, 0);
		Assert.assertEquals('1', cFromVH);

		/* SetOpaque */
		VH_CHAR.setOpaque(array, 0, '4');
		Assert.assertEquals('4', array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		cFromVH = (char) VH_CHAR.getVolatile(array, 0);
		Assert.assertEquals('1', cFromVH);

		/* SetVolatile */
		VH_CHAR.setVolatile(array, 0, '4');
		Assert.assertEquals('4', array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		cFromVH = (char) VH_CHAR.getAcquire(array, 0);
		Assert.assertEquals('1', cFromVH);

		/* SetRelease */
		VH_CHAR.setRelease(array, 0, '5');
		Assert.assertEquals('5', array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_CHAR.compareAndSet(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.compareAndSet(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		char caeResult = (char)VH_CHAR.compareAndExchange(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertEquals('1', caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchange(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (char)VH_CHAR.compareAndExchangeRelease(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeRelease(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertEquals('1', caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CHAR.weakCompareAndSet(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSet(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease(array, 0, '2', '3');
		Assert.assertEquals('1', array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease(array, 0, '1', '2');
		Assert.assertEquals('2', array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		cFromVH = (char)VH_CHAR.getAndSet(array, 0, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		cFromVH = (char)VH_CHAR.getAndAdd(array, 0, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2' + '1', array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a char[].
	 */
	@Test
	public void testCharLastElement() {
		char[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		char cFromVH = (char)VH_CHAR.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], cFromVH);
		
		/* Set */
		VH_CHAR.set(array, array.length - 1, '9');
		Assert.assertEquals('9', array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code double[]}.
	 */
	@Test
	public void testDouble() {
		double[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		double dFromVH = (double)VH_DOUBLE.get(array, 0);
		Assert.assertEquals(1.1, dFromVH);
		
		/* Set */
		VH_DOUBLE.set(array, 0, 4.4);
		Assert.assertEquals(4.4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		dFromVH = (double) VH_DOUBLE.getOpaque(array, 0);
		Assert.assertEquals(1.1, dFromVH);

		/* SetOpaque */
		VH_DOUBLE.setOpaque(array, 0, 4.4);
		Assert.assertEquals(4.4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		dFromVH = (double) VH_DOUBLE.getVolatile(array, 0);
		Assert.assertEquals(1.1, dFromVH);

		/* SetVolatile */
		VH_DOUBLE.setVolatile(array, 0, 4.4);
		Assert.assertEquals(4.4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		dFromVH = (double) VH_DOUBLE.getAcquire(array, 0);
		Assert.assertEquals(1.1, dFromVH);

		/* SetRelease */
		VH_DOUBLE.setRelease(array, 0, 5.5);
		Assert.assertEquals(5.5, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_DOUBLE.compareAndSet(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.compareAndSet(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		double caeResult = (double)VH_DOUBLE.compareAndExchange(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertEquals(1.1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchange(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertEquals(1.1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertEquals(1.1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertEquals(1.1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertEquals(1.1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertEquals(1.1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(array, 0, 2.2, 3.3);
		Assert.assertEquals(1.1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(array, 0, 1.1, 2.2);
		Assert.assertEquals(2.2, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		dFromVH = (double)VH_DOUBLE.getAndSet(array, 0, 2.2);
		Assert.assertEquals(1.1, dFromVH);
		Assert.assertEquals(2.2, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		dFromVH = (double)VH_DOUBLE.getAndAdd(array, 0, 2.2);
		Assert.assertEquals(1.1, dFromVH);
		Assert.assertEquals((1.1 + 2.2), array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a double[].
	 */
	@Test
	public void testDoubleLastElement() {
		double[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		double dFromVH = (double)VH_DOUBLE.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], dFromVH);
		
		/* Set */
		VH_DOUBLE.set(array, array.length - 1, 20.0);
		Assert.assertEquals(20.0, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code float[]}.
	 */
	@Test
	public void testFloat() {
		float[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		float fFromVH = (float)VH_FLOAT.get(array, 0);
		Assert.assertEquals(1.1f, fFromVH);
		
		/* Set */
		VH_FLOAT.set(array, 0, 4.4f);
		Assert.assertEquals(4.4f, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		fFromVH = (float) VH_FLOAT.getOpaque(array, 0);
		Assert.assertEquals(1.1f, fFromVH);

		/* SetOpaque */
		VH_FLOAT.setOpaque(array, 0, 4.4f);
		Assert.assertEquals(4.4f, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		fFromVH = (float) VH_FLOAT.getVolatile(array, 0);
		Assert.assertEquals(1.1f, fFromVH);

		/* SetVolatile */
		VH_FLOAT.setVolatile(array, 0, 4.4f);
		Assert.assertEquals(4.4f, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		fFromVH = (float) VH_FLOAT.getAcquire(array, 0);
		Assert.assertEquals(1.1f, fFromVH);

		/* SetRelease */
		VH_FLOAT.setRelease(array, 0, 5.5f);
		Assert.assertEquals(5.5f, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_FLOAT.compareAndSet(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.compareAndSet(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		float caeResult = (float)VH_FLOAT.compareAndExchange(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertEquals(1.1f, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchange(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertEquals(1.1f, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertEquals(1.1f, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertEquals(1.1f, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertEquals(1.1f, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertEquals(1.1f, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(array, 0, 2.2f, 3.3f);
		Assert.assertEquals(1.1f, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(array, 0, 1.1f, 2.2f);
		Assert.assertEquals(2.2f, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		fFromVH = (float)VH_FLOAT.getAndSet(array, 0, 2.2f);
		Assert.assertEquals(1.1f, fFromVH);
		Assert.assertEquals(2.2f, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		fFromVH = (float)VH_FLOAT.getAndAdd(array, 0, 2.2f);
		Assert.assertEquals(1.1f, fFromVH);
		Assert.assertEquals((1.1f + 2.2f), array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a float[].
	 */
	@Test
	public void testFloatLastElement() {
		float[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		float fFromVH = (float)VH_FLOAT.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], fFromVH);
		
		/* Set */
		VH_FLOAT.set(array, array.length - 1, 20.0f);
		Assert.assertEquals(20.0f, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code int[]}.
	 */
	@Test
	public void testInt() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		int iFromVH = (int)VH_INT.get(array, 0);
		Assert.assertEquals(1, iFromVH);
		
		/* Set */
		VH_INT.set(array, 0, 4);
		Assert.assertEquals(4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		iFromVH = (int) VH_INT.getOpaque(array, 0);
		Assert.assertEquals(1, iFromVH);

		/* SetOpaque */
		VH_INT.setOpaque(array, 0, 4);
		Assert.assertEquals(4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		iFromVH = (int) VH_INT.getVolatile(array, 0);
		Assert.assertEquals(1, iFromVH);

		/* SetVolatile */
		VH_INT.setVolatile(array, 0, 4);
		Assert.assertEquals(4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		iFromVH = (int) VH_INT.getAcquire(array, 0);
		Assert.assertEquals(1, iFromVH);

		/* SetRelease */
		VH_INT.setRelease(array, 0, 5);
		Assert.assertEquals(5, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_INT.compareAndSet(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_INT.compareAndSet(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		int caeResult = (int)VH_INT.compareAndExchange(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertEquals(1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (int)VH_INT.compareAndExchange(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (int)VH_INT.compareAndExchangeAcquire(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeAcquire(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (int)VH_INT.compareAndExchangeRelease(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeRelease(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertEquals(1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_INT.weakCompareAndSet(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSet(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(array, 0, 2, 3);
		Assert.assertEquals(1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(array, 0, 1, 2);
		Assert.assertEquals(2, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		iFromVH = (int)VH_INT.getAndSet(array, 0, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		iFromVH = (int)VH_INT.getAndAdd(array, 0, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a int[].
	 */
	@Test
	public void testIntLastElement() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		int iFromVH = (int)VH_INT.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], iFromVH);
		
		/* Set */
		VH_INT.set(array, array.length - 1, 20);
		Assert.assertEquals(20, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code long[]}.
	 */
	@Test
	public void testLong() {
		long[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		long jFromVH = (long)VH_LONG.get(array, 0);
		Assert.assertEquals(1L, jFromVH);
		
		/* Set */
		VH_LONG.set(array, 0, 4L);
		Assert.assertEquals(4L, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		jFromVH = (long) VH_LONG.getOpaque(array, 0);
		Assert.assertEquals(1L, jFromVH);

		/* SetOpaque */
		VH_LONG.setOpaque(array, 0, 4L);
		Assert.assertEquals(4L, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		jFromVH = (long) VH_LONG.getVolatile(array, 0);
		Assert.assertEquals(1L, jFromVH);

		/* SetVolatile */
		VH_LONG.setVolatile(array, 0, 4L);
		Assert.assertEquals(4L, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		jFromVH = (long) VH_LONG.getAcquire(array, 0);
		Assert.assertEquals(1L, jFromVH);

		/* SetRelease */
		VH_LONG.setRelease(array, 0, 5L);
		Assert.assertEquals(5L, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_LONG.compareAndSet(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.compareAndSet(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		long caeResult = (long)VH_LONG.compareAndExchange(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertEquals(1L, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (long)VH_LONG.compareAndExchange(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (long)VH_LONG.compareAndExchangeRelease(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeRelease(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertEquals(1L, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_LONG.weakCompareAndSet(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSet(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(array, 0, 2L, 3L);
		Assert.assertEquals(1L, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(array, 0, 1L, 2L);
		Assert.assertEquals(2L, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		jFromVH = (long)VH_LONG.getAndSet(array, 0, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		jFromVH = (long)VH_LONG.getAndAdd(array, 0, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a long[].
	 */
	@Test
	public void testLongLastElement() {
		long[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		long jFromVH = (long)VH_LONG.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], jFromVH);
		
		/* Set */
		VH_LONG.set(array, array.length - 1, 20L);
		Assert.assertEquals(20L, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code String[]}.
	 */
	@Test
	public void testReference() {
		String[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		String lFromVH = (String)VH_STRING.get(array, 0);
		Assert.assertEquals("1", lFromVH);
		
		/* Set */
		VH_STRING.set(array, 0, "4");
		Assert.assertEquals("4", array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		lFromVH = (String) VH_STRING.getOpaque(array, 0);
		Assert.assertEquals("1", lFromVH);

		/* SetOpaque */
		VH_STRING.setOpaque(array, 0, "4");
		Assert.assertEquals("4", array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		lFromVH = (String) VH_STRING.getVolatile(array, 0);
		Assert.assertEquals("1", lFromVH);

		/* SetVolatile */
		VH_STRING.setVolatile(array, 0, "4");
		Assert.assertEquals("4", array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		lFromVH = (String) VH_STRING.getAcquire(array, 0);
		Assert.assertEquals("1", lFromVH);

		/* SetRelease */
		VH_STRING.setRelease(array, 0, "5");
		Assert.assertEquals("5", array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_STRING.compareAndSet(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.compareAndSet(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		String caeResult = (String)VH_STRING.compareAndExchange(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertEquals("1", caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (String)VH_STRING.compareAndExchange(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (String)VH_STRING.compareAndExchangeAcquire(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeAcquire(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (String)VH_STRING.compareAndExchangeRelease(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeRelease(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertEquals("1", caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_STRING.weakCompareAndSet(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSet(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease(array, 0, "2", "3");
		Assert.assertEquals("1", array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease(array, 0, "1", "2");
		Assert.assertEquals("2", array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		lFromVH = (String)VH_STRING.getAndSet(array, 0, "2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", array[0]);
		
		/* GetAndAdd */
		try {
			lFromVH = (String)VH_STRING.getAndAdd(array, 0, "2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code Class[]}.
	 */
	@Test
	public void testReferenceOtherType() {
		Class[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		Class<?> lFromVH = (Class<?>)VH_CLASS.get(array, 0);
		Assert.assertEquals(Object.class, lFromVH);
		
		/* Set */
		VH_CLASS.set(array, 0, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		lFromVH = (Class<?>) VH_CLASS.getOpaque(array, 0);
		Assert.assertEquals(Object.class, lFromVH);

		/* SetOpaque */
		VH_CLASS.setOpaque(array, 0, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		lFromVH = (Class<?>) VH_CLASS.getVolatile(array, 0);
		Assert.assertEquals(Object.class, lFromVH);

		/* SetVolatile */
		VH_CLASS.setVolatile(array, 0, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		lFromVH = (Class<?>) VH_CLASS.getAcquire(array, 0);
		Assert.assertEquals(Object.class, lFromVH);

		/* SetRelease */
		VH_CLASS.setRelease(array, 0, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_CLASS.compareAndSet(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_CLASS.compareAndSet(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		Class<?> caeResult = (Class<?>)VH_CLASS.compareAndExchange(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchange(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeAcquire(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeAcquire(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeRelease(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeRelease(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertEquals(Object.class, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CLASS.weakCompareAndSet(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSet(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CLASS.weakCompareAndSetAcquire(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSetAcquire(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_CLASS.weakCompareAndSetRelease(array, 0, String.class, Integer.class);
		Assert.assertEquals(Object.class, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSetRelease(array, 0, Object.class, Integer.class);
		Assert.assertEquals(Integer.class, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		lFromVH = (Class<?>)VH_CLASS.getAndSet(array, 0, Integer.class);
		Assert.assertEquals(Object.class, lFromVH);
		Assert.assertEquals(Integer.class, array[0]);
		
		/* GetAndAdd */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndAdd(array, 0, Integer.class);
			Assert.fail("Successfully added reference types (Class)");
		} catch (UnsupportedOperationException e) {	}
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a String[].
	 */
	@Test
	public void testReferenceLastElement() {
		String[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		String lFromVH = (String)VH_STRING.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], lFromVH);
		
		/* Set */
		VH_STRING.set(array, array.length - 1, "20");
		Assert.assertEquals("20", array[array.length - 1]);
	}
		
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code short[]}.
	 */
	@Test
	public void testShort() {
		short[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		short sFromVH = (short)VH_SHORT.get(array, 0);
		Assert.assertEquals((short)1, sFromVH);
		
		/* Set */
		VH_SHORT.set(array, 0, (short)4);
		Assert.assertEquals((short)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetOpaque */
		sFromVH = (short) VH_SHORT.getOpaque(array, 0);
		Assert.assertEquals((short)1, sFromVH);

		/* SetOpaque */
		VH_SHORT.setOpaque(array, 0, (short)4);
		Assert.assertEquals((short)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetVolatile */
		sFromVH = (short) VH_SHORT.getVolatile(array, 0);
		Assert.assertEquals((short)1, sFromVH);

		/* SetVolatile */
		VH_SHORT.setVolatile(array, 0, (short)4);
		Assert.assertEquals((short)4, array[0]);

		array = ArrayHelper.reset(array);
		/* GetAcquire */
		sFromVH = (short) VH_SHORT.getAcquire(array, 0);
		Assert.assertEquals((short)1, sFromVH);

		/* SetRelease */
		VH_SHORT.setRelease(array, 0, (short)5);
		Assert.assertEquals((short)5, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_SHORT.compareAndSet(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.compareAndSet(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		short caeResult = (short)VH_SHORT.compareAndExchange(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertEquals((short)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchange(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (short)VH_SHORT.compareAndExchangeRelease(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeRelease(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertEquals((short)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_SHORT.weakCompareAndSet(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSet(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease(array, 0, (short)2, (short)3);
		Assert.assertEquals((short)1, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease(array, 0, (short)1, (short)2);
		Assert.assertEquals((short)2, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		sFromVH = (short)VH_SHORT.getAndSet(array, 0, (short)2);
		Assert.assertEquals(1, sFromVH);
		Assert.assertEquals(2, array[0]);
		
		/* GetAndAdd */
		array = ArrayHelper.reset(array);
		sFromVH = (short)VH_SHORT.getAndAdd(array, 0, (short)2);
		Assert.assertEquals(1, sFromVH);
		Assert.assertEquals(3, array[0]);
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a short[].
	 */
	@Test
	public void testShortLastElement() {
		short[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		short sFromVH = (short)VH_SHORT.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], sFromVH);
		
		/* Set */
		VH_SHORT.set(array, array.length - 1, (short)20);
		Assert.assertEquals((short)20, array[array.length - 1]);
	}
	
	/**
	 * Perform all the operations available on an ArrayVarHandle referencing a {@code boolean[]}.
	 */
	@Test
	public void testBoolean() {
		boolean[] array = null;
		array = ArrayHelper.reset(array);

		/* Get */
		boolean zFromVH = (boolean)VH_BOOLEAN.get(array, 0);
		Assert.assertEquals(true, zFromVH);
		
		/* Set */
		VH_BOOLEAN.set(array, 0, false);
		Assert.assertEquals(false, array[0]);
		
		/* GetOpaque */
		zFromVH = (boolean) VH_BOOLEAN.getOpaque(array, 0);
		Assert.assertEquals(false, zFromVH);
		
		/* SetOpaque */
		VH_BOOLEAN.setOpaque(array, 0, true);
		Assert.assertEquals(true, array[0]);
		
		/* GetVolatile */
		zFromVH = (boolean) VH_BOOLEAN.getVolatile(array, 0);
		Assert.assertEquals(true, zFromVH);
		
		/* SetVolatile */
		VH_BOOLEAN.setVolatile(array, 0, false);
		Assert.assertEquals(false, array[0]);
		
		/* GetAcquire */
		zFromVH = (boolean) VH_BOOLEAN.getAcquire(array, 0);
		Assert.assertEquals(false, zFromVH);
		
		/* SetRelease */
		VH_BOOLEAN.setRelease(array, 0, true);
		Assert.assertEquals(true, array[0]);
		
		/* CompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		boolean casResult = (boolean)VH_BOOLEAN.compareAndSet(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.compareAndSet(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		array = ArrayHelper.reset(array);
		boolean caeResult = (boolean)VH_BOOLEAN.compareAndExchange(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertEquals(true, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchange(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		array = ArrayHelper.reset(array);
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertEquals(true, caeResult);
		
		/* WeakCompareAndSet - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		array = ArrayHelper.reset(array);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(array, 0, false, false);
		Assert.assertEquals(true, array[0]);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(array, 0, true, false);
		Assert.assertEquals(false, array[0]);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		array = ArrayHelper.reset(array);
		zFromVH = (boolean)VH_BOOLEAN.getAndSet(array, 0, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, array[0]);

		try {
			/* GetAndAdd */
			zFromVH = (boolean)VH_BOOLEAN.getAndAdd(array, 0, false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}

	/**
	 * Get and set the last element of a ArrayVarHandle referencing a boolean[].
	 */
	@Test
	public void testBooleanLastElement() {
		boolean[] array = null;
		array = ArrayHelper.reset(array);
		
		/* Get */
		boolean zFromVH = (boolean)VH_BOOLEAN.get(array, array.length - 1);
		Assert.assertEquals(array[array.length - 1], zFromVH);
		
		/* Set */
		VH_BOOLEAN.set(array, array.length - 1, true);
		Assert.assertEquals(true, array[array.length - 1]);
	}
	
	/**
	 * Cast the {@code int[]} receiver object to {@link Object}. 
	 */
	@Test
	public void testObjectReceiver() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set((Object)array, 0, 2);
		Assert.assertEquals(2, array[0]);
	}
	
	/**
	 * Pass an invalid type, cast to {@link Object}, as the {@code int[]} receiver object.
	 */
	@Test(expectedExceptions = ClassCastException.class)
	public void testInvalidObjectReceiver() {
		VH_INT.set((Object)VH_INT, 0, 2);
	}
	
	/**
	 * Pass {@code null}, cast to {@code int[]}, as the {@code int[]} receiver object.
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullReceiver() {
		VH_INT.set((int[])null, 0, 2);
	}
	
	/**
	 * Pass an {@code int[]} as the {@code byte[]} receiver object.
	 */
	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testIntArrayAsByteArray() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_BYTE.set(array, 0, 2);
	}
	
	/**
	 * Write to an index past the end of the int[].
	 */
	@Test(expectedExceptions = ArrayIndexOutOfBoundsException.class)
	public void testInvalidIndex() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set(array, array.length, 123);
	}
	
	/**
	 * Write to a negative index.
	 */
	@Test(expectedExceptions = ArrayIndexOutOfBoundsException.class)
	public void testNegativeIndex() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set(array, -1, 123);
	}
	
	/**
	 * Use an index of type {@code long}.
	 */
	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testLongIndex() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set(array, (long)1, 123);
	}
	
	/**
	 * Use an index of type {@code short}.
	 */
	@Test
	public void testShortIndex() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set(array, (short)1, 123);
		Assert.assertEquals(array[1], 123);
	}
	
	/**
	 * Use an index of type {@link Integer}.
	 */
	@Test
	public void testBoxedIndex() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		VH_INT.set(array, Integer.valueOf(1), 123);
		Assert.assertEquals(array[1], 123);
	}
	
	/**
	 * Call {@link ArrayVarHandleTests#commonVarHandleCallSite(VarHandle, Object) commonVarHandleCallSite}
	 * using different VarHandles kinds:
	 * <ul>
	 * <li>Array with int[]</li>
	 * <li>ByteArrayView (int) with byte[]</li>
	 * <li>ByteBufferView (int) with ByteBuffer</li>
	 * </ul>
	 */
	@Test
	public void testCommonCallSite() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		ByteArrayViewHelper.reset();
		ByteBufferViewHelper.reset();
		
		VarHandle byteArrayViewVH = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
		VarHandle byteBufferViewVH = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
		
		commonVarHandleCallSite(VH_INT, array);
		commonVarHandleCallSite(byteArrayViewVH, ByteArrayViewHelper.b);
		commonVarHandleCallSite(byteBufferViewVH, ByteBufferViewHelper.onHeap);
	}
	
	private int commonVarHandleCallSite(VarHandle vh, Object rec) {
		return (int)vh.get(rec, 1);
	}
}
