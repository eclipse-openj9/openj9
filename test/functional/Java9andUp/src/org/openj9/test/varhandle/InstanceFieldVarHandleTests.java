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

/**
 * Test InstanceFieldVarHandle operations
 * 
 * @author Bjorn Vardal
 */
@Test(groups = { "level.extended" })
public class InstanceFieldVarHandleTests {
	private final static VarHandle VH_BYTE;
	private final static VarHandle VH_CHAR;
	private final static VarHandle VH_DOUBLE;
	private final static VarHandle VH_FLOAT;
	private final static VarHandle VH_INT;
	private final static VarHandle VH_LONG;
	private final static VarHandle VH_SHORT;
	private final static VarHandle VH_BOOLEAN;
	private final static VarHandle VH_STRING;
	private final static VarHandle VH_INTEGER;
	private final static VarHandle VH_FINAL_INT;
	private final static VarHandle VH_CHILD;
	private final static VarHandle VH_PARENT;
	
	static {
		try {
			VH_BYTE = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "b", byte.class);
			VH_CHAR = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "c", char.class);
			VH_DOUBLE = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "d", double.class);
			VH_FLOAT = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "f", float.class);
			VH_INT = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
			VH_LONG = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "j", long.class);
			VH_SHORT = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "s", short.class);;
			VH_BOOLEAN = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "z", boolean.class);
			VH_STRING = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", String.class);
			VH_INTEGER = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "integer", Integer.class);
			VH_FINAL_INT = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "finalI", int.class);
			VH_CHILD = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "child", Child.class);
			VH_PARENT = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "parent", Parent.class);
		} catch (Throwable t) {
			throw new Error(t);
		}
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link byte} field.
	 */
	@Test
	public void testByte() {
		/* Setup */
		InstanceHelper h = new InstanceHelper((byte)1);
		
		/* Get */
		byte bFromVH = (byte)VH_BYTE.get(h);
		Assert.assertEquals((byte)1, bFromVH);
		
		/* Set */
		VH_BYTE.set(h, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		
		/* GetOpaque */
		bFromVH = (byte) VH_BYTE.getOpaque(h);
		Assert.assertEquals((byte)2, bFromVH);
		
		/* SetOpaque */
		VH_BYTE.setOpaque(h, (byte)3);
		Assert.assertEquals((byte)3, h.b);
		
		/* GetVolatile */
		bFromVH = (byte) VH_BYTE.getVolatile(h);
		Assert.assertEquals((byte)3, bFromVH);
		
		/* SetVolatile */
		VH_BYTE.setVolatile(h, (byte)4);
		Assert.assertEquals((byte)4, h.b);
		
		/* GetAcquire */
		bFromVH = (byte) VH_BYTE.getAcquire(h);
		Assert.assertEquals((byte)4, bFromVH);
		
		/* SetRelease */
		VH_BYTE.setRelease(h, (byte)5);
		Assert.assertEquals((byte)5, h.b);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper((byte)1);
		boolean casResult = (boolean)VH_BYTE.compareAndSet(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.compareAndSet(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper((byte)1);
		byte caeResult = (byte)VH_BYTE.compareAndExchange(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertEquals((byte)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchange(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper((byte)1);
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper((byte)1);
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper((byte)1);
		casResult = (boolean)VH_BYTE.weakCompareAndSet(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSet(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper((byte)1);
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper((byte)1);
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper((byte)1);
		casResult = (boolean)VH_BYTE.weakCompareAndSetPlain(h, (byte)2, (byte)3);
		Assert.assertEquals((byte)1, h.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetPlain(h, (byte)1, (byte)2);
		Assert.assertEquals((byte)2, h.b);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndSet(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndSetAcquire(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
		
		/* GetAndSetRelease */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndSetRelease(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
		
		/* GetAndAdd */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndAdd(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndAddAcquire(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* GetAndAddRelease */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndAddRelease(h, (byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseAnd(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, h.b);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseAndAcquire(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, h.b);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseAndRelease(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, h.b);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseOr(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseOrAcquire(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseOrRelease(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, h.b);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseXor(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseXorAcquire(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper((byte)1);
		bFromVH = (byte)VH_BYTE.getAndBitwiseXorRelease(h, (byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, h.b);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link char} field.
	 */
	@Test
	public void testChar() {
		/* Setup */
		InstanceHelper h = new InstanceHelper('1');
		
		/* Get */
		char cFromVH = (char)VH_CHAR.get(h);
		Assert.assertEquals('1', cFromVH);
		
		/* Set */
		VH_CHAR.set(h, '2');
		Assert.assertEquals('2', h.c);
		
		/* GetOpaque */
		cFromVH = (char) VH_CHAR.getOpaque(h);
		Assert.assertEquals('2', cFromVH);
		
		/* SetOpaque */
		VH_CHAR.setOpaque(h, '3');
		Assert.assertEquals('3', h.c);
		
		/* GetVolatile */
		cFromVH = (char) VH_CHAR.getVolatile(h);
		Assert.assertEquals('3', cFromVH);
		
		/* SetVolatile */
		VH_CHAR.setVolatile(h, '4');
		Assert.assertEquals('4', h.c);
		
		/* GetAcquire */
		cFromVH = (char) VH_CHAR.getAcquire(h);
		Assert.assertEquals('4', cFromVH);
		
		/* SetRelease */
		VH_CHAR.setRelease(h, '5');
		Assert.assertEquals('5', h.c);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper('1');
		boolean casResult = (boolean)VH_CHAR.compareAndSet(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.compareAndSet(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper('1');
		char caeResult = (char)VH_CHAR.compareAndExchange(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertEquals('1', caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchange(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper('1');
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper('1');
		caeResult = (char)VH_CHAR.compareAndExchangeRelease(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeRelease(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertEquals('1', caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper('1');
		casResult = (boolean)VH_CHAR.weakCompareAndSet(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSet(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper('1');
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper('1');
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper('1');
		casResult = (boolean)VH_CHAR.weakCompareAndSetPlain(h, '2', '3');
		Assert.assertEquals('1', h.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetPlain(h, '1', '2');
		Assert.assertEquals('2', h.c);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndSet(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', h.c);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndSetAcquire(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', h.c);
		
		/* GetAndSetRelease */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndSetRelease(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', h.c);
		
		/* GetAndAdd */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndAdd(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', h.c);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndAddAcquire(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', h.c);
		
		/* GetAndAddRelease */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndAddRelease(h, '2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', h.c);
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseAnd(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', h.c);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseAndAcquire(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', h.c);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseAndRelease(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', h.c);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseOr(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', h.c);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseOrAcquire(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', h.c);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseOrRelease(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', h.c);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseXor(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, h.c);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseXorAcquire(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, h.c);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper('1');
		cFromVH = (char)VH_CHAR.getAndBitwiseXorRelease(h, '3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, h.c);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link double} field.
	 */
	@Test
	public void testDouble() {
		/* Setup */
		InstanceHelper h = new InstanceHelper(1.0);
		
		/* Get */
		double dFromVH = (double)VH_DOUBLE.get(h);
		Assert.assertEquals(1.0, dFromVH);
		
		/* Set */
		VH_DOUBLE.set(h, 2.0);
		Assert.assertEquals(2.0, h.d);
		
		/* GetOpaque */
		dFromVH = (double) VH_DOUBLE.getOpaque(h);
		Assert.assertEquals(2.0, dFromVH);
		
		/* SetOpaque */
		VH_DOUBLE.setOpaque(h, 3.0);
		Assert.assertEquals(3.0, h.d);
		
		/* GetVolatile */
		dFromVH = (double) VH_DOUBLE.getVolatile(h);
		Assert.assertEquals(3.0, dFromVH);
		
		/* SetVolatile */
		VH_DOUBLE.setVolatile(h, 4.0);
		Assert.assertEquals(4.0, h.d);
		
		/* GetAcquire */
		dFromVH = (double) VH_DOUBLE.getAcquire(h);
		Assert.assertEquals(4.0, dFromVH);
		
		/* SetRelease */
		VH_DOUBLE.setRelease(h, 5.0);
		Assert.assertEquals(5.0, h.d);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper(1.0);
		boolean casResult = (boolean)VH_DOUBLE.compareAndSet(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.compareAndSet(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper(1.0);
		double caeResult = (double)VH_DOUBLE.compareAndExchange(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertEquals(1.0, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchange(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper(1.0);
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertEquals(1.0, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper(1.0);
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertEquals(1.0, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper(1.0);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper(1.0);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper(1.0);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper(1.0);
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetPlain(h, 2.0, 3.0);
		Assert.assertEquals(1.0, h.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetPlain(h, 1.0, 2.0);
		Assert.assertEquals(2.0, h.d);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndSet(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, h.d);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndSetAcquire(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, h.d);
		
		/* GetAndSetRelease */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndSetRelease(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, h.d);
		
		/* GetAndAdd */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndAdd(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, h.d);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndAddAcquire(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, h.d);
		
		/* GetAndAddRelease */
		h = new InstanceHelper(1.0);
		dFromVH = (double)VH_DOUBLE.getAndAddRelease(h, 2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, h.d);
		
		/* getAndBitwiseAnd */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAnd(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAndAcquire(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAndRelease(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOr(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOrAcquire(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOrRelease(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXor(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXorAcquire(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXorRelease(h, 3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link float} field.
	 */
	@Test
	public void testFloat() {
		/* Setup */
		InstanceHelper h = new InstanceHelper(1.0f);
		
		/* Get */
		float fFromVH = (float)VH_FLOAT.get(h);
		Assert.assertEquals(1.0f, fFromVH);
		
		/* Set */
		VH_FLOAT.set(h, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		
		/* GetOpaque */
		fFromVH = (float) VH_FLOAT.getOpaque(h);
		Assert.assertEquals(2.0f, fFromVH);
		
		/* SetOpaque */
		VH_FLOAT.setOpaque(h, 3.0f);
		Assert.assertEquals(3.0f, h.f);
		
		/* GetVolatile */
		fFromVH = (float) VH_FLOAT.getVolatile(h);
		Assert.assertEquals(3.0f, fFromVH);
		
		/* SetVolatile */
		VH_FLOAT.setVolatile(h, 4.0f);
		Assert.assertEquals(4.0f, h.f);
		
		/* GetAcquire */
		fFromVH = (float) VH_FLOAT.getAcquire(h);
		Assert.assertEquals(4.0f, fFromVH);
		
		/* SetRelease */
		VH_FLOAT.setRelease(h, 5.0f);
		Assert.assertEquals(5.0f, h.f);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper(1.0f);
		boolean casResult = (boolean)VH_FLOAT.compareAndSet(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.compareAndSet(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper(1.0f);
		float caeResult = (float)VH_FLOAT.compareAndExchange(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchange(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper(1.0f);
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper(1.0f);
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper(1.0f);
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper(1.0f);
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper(1.0f);
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper(1.0f);
		casResult = (boolean)VH_FLOAT.weakCompareAndSetPlain(h, 2.0f, 3.0f);
		Assert.assertEquals(1.0f, h.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetPlain(h, 1.0f, 2.0f);
		Assert.assertEquals(2.0f, h.f);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndSet(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, h.f);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndSetAcquire(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, h.f);
		
		/* GetAndSetRelease */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndSetRelease(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, h.f);
		
		/* GetAndAdd */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndAdd(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, h.f);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndAddAcquire(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, h.f);
		
		/* GetAndAddRelease */
		h = new InstanceHelper(1.0f);
		fFromVH = (float)VH_FLOAT.getAndAddRelease(h, 2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, h.f);
		
		/* getAndBitwiseAnd */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAnd(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAndAcquire(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAndRelease(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOr(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOrAcquire(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOrRelease(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXor(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXorAcquire(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXorRelease(h, 3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing an {@link int} field.
	 */
	@Test
	public void testInt() {
		/* Setup */
		InstanceHelper h = new InstanceHelper(1);
		
		/* Get */
		int iFromVH = (int)VH_INT.get(h);
		Assert.assertEquals(1, iFromVH);
		
		/* Set */
		VH_INT.set(h, 2);
		Assert.assertEquals(2, h.i);
		
		/* GetOpaque */
		iFromVH = (int) VH_INT.getOpaque(h);
		Assert.assertEquals(2, iFromVH);
		
		/* SetOpaque */
		VH_INT.setOpaque(h, 3);
		Assert.assertEquals(3, h.i);
		
		/* GetVolatile */
		iFromVH = (int) VH_INT.getVolatile(h);
		Assert.assertEquals(3, iFromVH);
		
		/* SetVolatile */
		VH_INT.setVolatile(h, 4);
		Assert.assertEquals(4, h.i);
		
		/* GetAcquire */
		iFromVH = (int) VH_INT.getAcquire(h);
		Assert.assertEquals(4, iFromVH);
		
		/* SetRelease */
		VH_INT.setRelease(h, 5);
		Assert.assertEquals(5, h.i);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper(1);
		boolean casResult = (boolean)VH_INT.compareAndSet(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_INT.compareAndSet(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper(1);
		int caeResult = (int)VH_INT.compareAndExchange(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertEquals(1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (int)VH_INT.compareAndExchange(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper(1);
		caeResult = (int)VH_INT.compareAndExchangeAcquire(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeAcquire(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper(1);
		caeResult = (int)VH_INT.compareAndExchangeRelease(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeRelease(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertEquals(1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper(1);
		casResult = (boolean)VH_INT.weakCompareAndSet(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSet(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper(1);
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper(1);
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper(1);
		casResult = (boolean)VH_INT.weakCompareAndSetPlain(h, 2, 3);
		Assert.assertEquals(1, h.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetPlain(h, 1, 2);
		Assert.assertEquals(2, h.i);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndSet(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndSetAcquire(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
		
		/* GetAndSetRelease */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndSetRelease(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
		
		/* GetAndAdd */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndAdd(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndAddAcquire(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* GetAndAddRelease */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndAddRelease(h, 2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseAnd(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, h.i);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseAndAcquire(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, h.i);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseAndRelease(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, h.i);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseOr(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseOrAcquire(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseOrRelease(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, h.i);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseXor(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseXorAcquire(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper(1);
		iFromVH = (int)VH_INT.getAndBitwiseXorRelease(h, 3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, h.i);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link long} field.
	 */
	@Test
	public void testLong() {
		/* Setup */
		InstanceHelper h = new InstanceHelper(1L);
		
		/* Get */
		long jFromVH = (long)VH_LONG.get(h);
		Assert.assertEquals(1L, jFromVH);
		
		/* Set */
		VH_LONG.set(h, 2L);
		Assert.assertEquals(2L, h.j);
		
		/* GetOpaque */
		jFromVH = (long) VH_LONG.getOpaque(h);
		Assert.assertEquals(2L, jFromVH);
		
		/* SetOpaque */
		VH_LONG.setOpaque(h, 3L);
		Assert.assertEquals(3L, h.j);
		
		/* GetVolatile */
		jFromVH = (long) VH_LONG.getVolatile(h);
		Assert.assertEquals(3L, jFromVH);
		
		/* SetVolatile */
		VH_LONG.setVolatile(h, 4L);
		Assert.assertEquals(4L, h.j);
		
		/* GetAcquire */
		jFromVH = (long) VH_LONG.getAcquire(h);
		Assert.assertEquals(4L, jFromVH);
		
		/* SetRelease */
		VH_LONG.setRelease(h, 5L);
		Assert.assertEquals(5L, h.j);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper(1L);
		boolean casResult = (boolean)VH_LONG.compareAndSet(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.compareAndSet(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper(1L);
		long caeResult = (long)VH_LONG.compareAndExchange(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertEquals(1L, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (long)VH_LONG.compareAndExchange(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper(1L);
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper(1L);
		caeResult = (long)VH_LONG.compareAndExchangeRelease(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeRelease(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertEquals(1L, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper(1L);
		casResult = (boolean)VH_LONG.weakCompareAndSet(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSet(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper(1L);
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper(1L);
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper(1L);
		casResult = (boolean)VH_LONG.weakCompareAndSetPlain(h, 2L, 3L);
		Assert.assertEquals(1L, h.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetPlain(h, 1L, 2L);
		Assert.assertEquals(2L, h.j);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndSet(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndSetAcquire(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
		
		/* GetAndSetRelease */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndSetRelease(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
		
		/* GetAndAdd */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndAdd(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndAddAcquire(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* GetAndAddRelease */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndAddRelease(h, 2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseAnd(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, h.j);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseAndAcquire(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, h.j);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseAndRelease(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, h.j);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseOr(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseOrAcquire(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseOrRelease(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, h.j);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseXor(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseXorAcquire(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper(1L);
		jFromVH = (long)VH_LONG.getAndBitwiseXorRelease(h, 3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, h.j);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link String} field.
	 */
	@Test
	public void testReference() {
		/* Setup */
		InstanceHelper h = new InstanceHelper("1");
		
		/* Get */
		String lFromVH = (String)VH_STRING.get(h);
		Assert.assertEquals("1", lFromVH);
		
		/* Set */
		VH_STRING.set(h, "2");
		Assert.assertEquals("2", h.l);
		
		/* GetOpaque */
		lFromVH = (String) VH_STRING.getOpaque(h);
		Assert.assertEquals("2", lFromVH);
		
		/* SetOpaque */
		VH_STRING.setOpaque(h, "3");
		Assert.assertEquals("3", h.l);
		
		/* GetVolatile */
		lFromVH = (String) VH_STRING.getVolatile(h);
		Assert.assertEquals("3", lFromVH);
		
		/* SetVolatile */
		VH_STRING.setVolatile(h, "4");
		Assert.assertEquals("4", h.l);
		
		/* GetAcquire */
		lFromVH = (String) VH_STRING.getAcquire(h);
		Assert.assertEquals("4", lFromVH);
		
		/* SetRelease */
		VH_STRING.setRelease(h, "5");
		Assert.assertEquals("5", h.l);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper("1");
		boolean casResult = (boolean)VH_STRING.compareAndSet(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.compareAndSet(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper("1");
		String caeResult = (String)VH_STRING.compareAndExchange(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertEquals("1", caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (String)VH_STRING.compareAndExchange(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper("1");
		caeResult = (String)VH_STRING.compareAndExchangeAcquire(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeAcquire(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper("1");
		caeResult = (String)VH_STRING.compareAndExchangeRelease(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeRelease(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertEquals("1", caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper("1");
		casResult = (boolean)VH_STRING.weakCompareAndSet(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSet(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper("1");
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper("1");
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper("1");
		casResult = (boolean)VH_STRING.weakCompareAndSetPlain(h, "2", "3");
		Assert.assertEquals("1", h.l);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetPlain(h, "1", "2");
		Assert.assertEquals("2", h.l);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper("1");
		lFromVH = (String)VH_STRING.getAndSet(h, "2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", h.l);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper("1");
		lFromVH = (String)VH_STRING.getAndSetAcquire(h, "2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", h.l);
		
		/* GetAndSetRelease */
		h = new InstanceHelper("1");
		lFromVH = (String)VH_STRING.getAndSetRelease(h, "2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", h.l);
		
		/* GetAndAdd */
		h = new InstanceHelper("1");
		try {
			lFromVH = (String)VH_STRING.getAndAdd(h, "2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddAcquire */
		h = new InstanceHelper("1");
		try {
			lFromVH = (String)VH_STRING.getAndAddAcquire(h, "2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddRelease */
		h = new InstanceHelper("1");
		try {
			lFromVH = (String)VH_STRING.getAndAddRelease(h, "2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* getAndBitwiseAnd */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAnd(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAndAcquire(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAndRelease(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOr(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOrAcquire(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOrRelease(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXor(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXorAcquire(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXorRelease(h, "3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link short} field.
	 */
	@Test
	public void testShort() {
		/* Setup */
		InstanceHelper h = new InstanceHelper((short)1);
		
		/* Get */
		short sFromVH = (short)VH_SHORT.get(h);
		Assert.assertEquals((short)1, sFromVH);
		
		/* Set */
		VH_SHORT.set(h, (short)2);
		Assert.assertEquals((short)2, h.s);
		
		/* GetOpaque */
		sFromVH = (short) VH_SHORT.getOpaque(h);
		Assert.assertEquals((short)2, sFromVH);
		
		/* SetOpaque */
		VH_SHORT.setOpaque(h, (short)3);
		Assert.assertEquals((short)3, h.s);
		
		/* GetVolatile */
		sFromVH = (short) VH_SHORT.getVolatile(h);
		Assert.assertEquals((short)3, sFromVH);
		
		/* SetVolatile */
		VH_SHORT.setVolatile(h, (short)4);
		Assert.assertEquals((short)4, h.s);
		
		/* GetAcquire */
		sFromVH = (short) VH_SHORT.getAcquire(h);
		Assert.assertEquals((short)4, sFromVH);
		
		/* SetRelease */
		VH_SHORT.setRelease(h, (short)5);
		Assert.assertEquals((short)5, h.s);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper((short)1);
		boolean casResult = (boolean)VH_SHORT.compareAndSet(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.compareAndSet(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper((short)1);
		short caeResult = (short)VH_SHORT.compareAndExchange(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertEquals((short)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchange(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper((short)1);
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper((short)1);
		caeResult = (short)VH_SHORT.compareAndExchangeRelease(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeRelease(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper((short)1);
		casResult = (boolean)VH_SHORT.weakCompareAndSet(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSet(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper((short)1);
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper((short)1);
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper((short)1);
		casResult = (boolean)VH_SHORT.weakCompareAndSetPlain(h, (short)2, (short)3);
		Assert.assertEquals((short)1, h.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetPlain(h, (short)1, (short)2);
		Assert.assertEquals((short)2, h.s);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndSet(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndSetAcquire(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
		
		/* GetAndSetRelease */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndSetRelease(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
		
		/* GetAndAdd */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndAdd(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* GetAndAddAcquire */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndAddAcquire(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* GetAndAddRelease */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndAddRelease(h, (short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseAnd(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, h.s);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseAndAcquire(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, h.s);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseAndRelease(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, h.s);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseOr(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseOrAcquire(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseOrRelease(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, h.s);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseXor(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseXorAcquire(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper((short)1);
		sFromVH = (short)VH_SHORT.getAndBitwiseXorRelease(h, (short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, h.s);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a {@link boolean} field.
	 */
	@Test
	public void testBoolean() {
		/* Setup */
		InstanceHelper h = new InstanceHelper(true);
		
		/* Get */
		boolean zFromVH = (boolean)VH_BOOLEAN.get(h);
		Assert.assertEquals(true, zFromVH);
		
		/* Set */
		VH_BOOLEAN.set(h, false);
		Assert.assertEquals(false, h.z);
		
		/* GetOpaque */
		zFromVH = (boolean) VH_BOOLEAN.getOpaque(h);
		Assert.assertEquals(false, zFromVH);
		
		/* SetOpaque */
		VH_BOOLEAN.setOpaque(h, true);
		Assert.assertEquals(true, h.z);
		
		/* GetVolatile */
		zFromVH = (boolean) VH_BOOLEAN.getVolatile(h);
		Assert.assertEquals(true, zFromVH);
		
		/* SetVolatile */
		VH_BOOLEAN.setVolatile(h, false);
		Assert.assertEquals(false, h.z);
		
		/* GetAcquire */
		zFromVH = (boolean) VH_BOOLEAN.getAcquire(h);
		Assert.assertEquals(false, zFromVH);
		
		/* SetRelease */
		VH_BOOLEAN.setRelease(h, true);
		Assert.assertEquals(true, h.z);
		
		/* CompareAndSet - Fail */
		h = new InstanceHelper(true);
		boolean casResult = (boolean)VH_BOOLEAN.compareAndSet(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.compareAndSet(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		h = new InstanceHelper(true);
		boolean caeResult = (boolean)VH_BOOLEAN.compareAndExchange(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertEquals(true, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchange(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		h = new InstanceHelper(true);
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		h = new InstanceHelper(true);
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertEquals(true, caeResult);
		
		/* WeakCompareAndSet - Fail */
		h = new InstanceHelper(true);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		h = new InstanceHelper(true);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		h = new InstanceHelper(true);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		h = new InstanceHelper(true);
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetPlain(h, false, false);
		Assert.assertEquals(true, h.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetPlain(h, true, false);
		Assert.assertEquals(false, h.z);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndSet(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* GetAndSetAcquire */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndSetAcquire(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* GetAndSetRelease */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndSetRelease(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);

		try {
			/* GetAndAdd */
			zFromVH = (boolean)VH_BOOLEAN.getAndAdd(h, false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		try {
			/* GetAndAddAcquire */
			zFromVH = (boolean)VH_BOOLEAN.getAndAddAcquire(h, false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		try {
			/* GetAndAddRelease */
			zFromVH = (boolean)VH_BOOLEAN.getAndAddRelease(h, false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAnd */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAnd(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* getAndBitwiseAndAcquire */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAndAcquire(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* getAndBitwiseAndRelease */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAndRelease(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* getAndBitwiseOr */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOr(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, h.z);
		
		/* getAndBitwiseOrAcquire */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOrAcquire(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, h.z);
		
		/* getAndBitwiseOrRelease */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOrRelease(h, false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, h.z);
		
		/* getAndBitwiseXor */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXor(h, true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* getAndBitwiseXorAcquire */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXorAcquire(h, true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
		
		/* getAndBitwiseXorRelease */
		h = new InstanceHelper(true);
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXorRelease(h, true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, h.z);
	}
	
	/**
	 * Perform all the operations available on a InstanceFieldViewVarHandle referencing a final {@link int} field.
	 */
	@Test
	public void testFinalField() {
		InstanceHelper h = new InstanceHelper();
		
		// Getting value of final static field
		int result = (int) VH_FINAL_INT.get(h);
		Assert.assertEquals(result, h.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.set(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getVolatile(h);
		Assert.assertEquals(result, h.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setVolatile(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getOpaque(h);
		Assert.assertEquals(result, h.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setOpaque(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getAcquire(h);
		Assert.assertEquals(result, h.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setRelease(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndSet(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchange(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchangeAcquire(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchangeRelease(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSet(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetAcquire(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetRelease(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetPlain(h, 5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSet(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSetAcquire(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSetRelease(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAdd(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAddAcquire(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAddRelease(h, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAnd */
		try {
			VH_FINAL_INT.getAndBitwiseAnd(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseAndAcquire(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			VH_FINAL_INT.getAndBitwiseAndRelease(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			VH_FINAL_INT.getAndBitwiseOr(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseOrAcquire(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			VH_FINAL_INT.getAndBitwiseOrRelease(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			VH_FINAL_INT.getAndBitwiseXor(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseXorAcquire(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			VH_FINAL_INT.getAndBitwiseXorRelease(h, 3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Pass {@link null}, cast to {@link InstanceHelper}, as the receiver object.
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullReceiver() {
		VH_STRING.set((InstanceHelper)null, "foo");
	}
	
	/**
	 * Pass a wider primitive ({@link long}) than the type referenced by the {@link VarHandle} ({@link int}).
	 */
	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testIncorrectArgTypePrimitiveWider() {
		VH_INT.set(new InstanceHelper(1), Long.MAX_VALUE);
	}
	
	/**
	 * Pass a wider primitive ({@link short}) than the type referenced by the {@link VarHandle} ({@link int}).
	 */
	@Test
	public void testIncorrectArgTypePrimitiveNarrower() {
		InstanceHelper h = new InstanceHelper(1);
		VH_INT.set(h, Short.MAX_VALUE);
		Assert.assertEquals(Short.MAX_VALUE, h.i);
	}
	
	/**
	 * Pass an invalid reference type ({@link String}) to a VarHandle referencing an {@link int} field.
	 */
	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testIncorrectArgTypePrimitiveReference() {
		VH_INT.set(new InstanceHelper(1), "foo");
	}
	
	/**
	 * Pass an {@link int} to a VarHandle referencing an {@link String} field.
	 */
	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testIncorrectArgTypeReferencePrimitive() {
		VH_STRING.set(new InstanceHelper("foo"), (int) 1);
	}
	
	/**
	 * Pass an invalid reference type ({@link InstanceHelper}) to a VarHandle referencing a {@link String} field.
	 */
	@Test(expectedExceptions=ClassCastException.class)
	public void testIncorrectArgTypeReferenceReference() {
		VH_STRING.set(new InstanceHelper("foo"), new InstanceHelper());
	}
	
	/**
	 * Pass a supertype ({@link Parent}) to a VarHandle referencing a {@link Child} field.
	 */
	@Test(expectedExceptions=ClassCastException.class)
	public void testIncorrectArgTypeSupertype() {
		VH_CHILD.set(new InstanceHelper(), new Parent());
	}
	
	/**
	 * Pass a subtype ({@link Child}) to a VarHandle referencing a {@link Parent} field.
	 */
	@Test
	public void testIncorrectArgTypeSubtype() {
		VH_PARENT.set(new InstanceHelper(), new Child());
	}
	
	/**
	 * Pass a boxed {@link Integer} to a VarHandle referencing an {@link int} field.
	 */
	@Test
	public void testBoxedArgs() {
		InstanceHelper h = new InstanceHelper(1);
		VH_INT.set(h, Integer.valueOf(2));
		Assert.assertEquals(h.i, 2);
	}
	
	/**
	 * Box the return value of {@link VarHandle#get(Object...) VarHandle.get} 
	 * where the {@link VarHandle} references an {@link int} field.
	 */
	@Test
	public void testBoxedReturnValue() {
		InstanceHelper h = new InstanceHelper(1);
		Integer result = (Integer)VH_INT.get(h);
		Assert.assertEquals(h.i, result.intValue());
	}
	
	/**
	 * Pass an {@link int} to a {@link VarHandle} referencing an {@link Integer} field.
	 */
	@Test
	public void testUnboxedArgs() {
		InstanceHelper h = new InstanceHelper(1);
		VH_INTEGER.set(h, 2);
		Assert.assertEquals(2, h.integer.intValue());
	}
	
	/**
	 * Unbox the return value of {@link VarHandle#get(Object...) VarHandle.get} 
	 * where the {@link VarHandle} references an {@link Integer} field.
	 */
	@Test
	public void testUnboxedReturnValue() {
		InstanceHelper h = new InstanceHelper(Integer.valueOf(1));
		int result = (int)VH_INTEGER.get(h);
		Assert.assertEquals(1, result);
	}
	
	/**
	 * Pass too many arguments (+2) to a {@link VarHandle} operation.
	 */
	@Test(expectedExceptions=WrongMethodTypeException.class)
	public void testTooManyArgs() {
		VH_INT.set(new InstanceHelper(1), 1, 2, 3);
	}
	
	/**
	 * Pass too few arguments (-1) to a {@link VarHandle} operation.
	 */
	@Test(expectedExceptions=WrongMethodTypeException.class)
	public void testTooFewArgs() {
		VH_INT.set(new InstanceHelper(1));
	}
	
	/**
	 * Invoke a {@link VarHandle} getter without storing its return value.
	 */
	@Test
	public void testNotStoringReturnValue() {
		VH_INT.get(new InstanceHelper(1));
	}
	
	/**
	 * Box the return value of {@link VarHandle#get(Object...) VarHandle.get} 
	 * where the {@link VarHandle} references an {@link int} field, to {@link Object}.
	 */
	@Test
	public void testOtherReturnType() {
		Object s = (Object)VH_INT.get(new InstanceHelper(1));
		Assert.assertTrue(s instanceof Integer);
	}
	
	/**
	 * Force the return type of {@link VarHandle#get(Object...) VarHandle.get} to {@link String}, 
	 * where the {@link VarHandle} references an {@link int} field.
	 */
	@Test(expectedExceptions=WrongMethodTypeException.class)
	public void testIncompatibleReturnType() {
		@SuppressWarnings("unused")
		String s = (String)VH_INT.get(new InstanceHelper(1));
	}
}
