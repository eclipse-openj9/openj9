/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
 * Test StaticFieldVarHandle operations
 * 
 * @author Bjorn Vardal
 *
 */
@Test(groups = { "level.extended" })
public class StaticFieldVarHandleTests {
	private final static VarHandle VH_BYTE;
	private final static VarHandle VH_CHAR;
	private final static VarHandle VH_DOUBLE;
	private final static VarHandle VH_FLOAT;
	private final static VarHandle VH_INT;
	private final static VarHandle VH_LONG;
	private final static VarHandle VH_SHORT;
	private final static VarHandle VH_BOOLEAN;
	private final static VarHandle VH_STRING;
	private final static VarHandle VH_CLASS;
	private final static VarHandle VH_FINAL_INT;
	private final static VarHandle VH_FINAL_NOINIT_INT;
	
	static {
		StaticHelper.reset();
		
		try {
			VH_BYTE = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "b", byte.class);
			VH_CHAR = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "c", char.class);
			VH_DOUBLE = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "d", double.class);
			VH_FLOAT = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "f", float.class);
			VH_INT = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "i", int.class);
			VH_LONG = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "j", long.class);
			VH_SHORT = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "s", short.class);;
			VH_BOOLEAN = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "z", boolean.class);
			VH_STRING = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "l1", String.class);
			VH_CLASS = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "l2", Class.class);
			VH_FINAL_INT = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "finalI", int.class);
			VH_FINAL_NOINIT_INT = MethodHandles.lookup().findStaticVarHandle(StaticHelper.StaticNoInitializationHelper.class, "finalI", int.class);
		} catch (Throwable t) {
			throw new Error(t);
		}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link byte} field.
	 */
	@Test
	public void testByte() {
		StaticHelper.reset();
		
		/* Get */
		byte bFromVH = (byte)VH_BYTE.get();
		Assert.assertEquals((byte)1, bFromVH);
		
		/* Set */
		VH_BYTE.set((byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* GetOpaque */
		bFromVH = (byte) VH_BYTE.getOpaque();
		Assert.assertEquals((byte)2, bFromVH);
		
		/* SetOpaque */
		VH_BYTE.setOpaque((byte)3);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* GetVolatile */
		bFromVH = (byte) VH_BYTE.getVolatile();
		Assert.assertEquals((byte)3, bFromVH);
		
		/* SetVolatile */
		VH_BYTE.setVolatile((byte)4);
		Assert.assertEquals((byte)4, StaticHelper.b);
		
		/* GetAcquire */
		bFromVH = (byte) VH_BYTE.getAcquire();
		Assert.assertEquals((byte)4, bFromVH);
		
		/* SetRelease */
		VH_BYTE.setRelease((byte)5);
		Assert.assertEquals((byte)5, StaticHelper.b);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_BYTE.compareAndSet((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.compareAndSet((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		byte caeResult = (byte)VH_BYTE.compareAndExchange((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchange((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeAcquire((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (byte)VH_BYTE.compareAndExchangeRelease((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertEquals((byte)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BYTE.weakCompareAndSet((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSet((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetAcquire((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetRelease((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BYTE.weakCompareAndSetPlain((byte)2, (byte)3);
		Assert.assertEquals((byte)1, StaticHelper.b);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_BYTE.weakCompareAndSetPlain((byte)1, (byte)2);
		Assert.assertEquals((byte)2, StaticHelper.b);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndSet((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndSetAcquire((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndSetRelease((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* GetAndAdd */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndAdd((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndAddAcquire((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndAddRelease((byte)2);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseAnd((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, StaticHelper.b);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseAndAcquire((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, StaticHelper.b);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseAndRelease((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)1, StaticHelper.b);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseOr((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseOrAcquire((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseOrRelease((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)3, StaticHelper.b);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseXor((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseXorAcquire((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		bFromVH = (byte)VH_BYTE.getAndBitwiseXorRelease((byte)3);
		Assert.assertEquals((byte)1, bFromVH);
		Assert.assertEquals((byte)2, StaticHelper.b);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link char} field.
	 */
	@Test
	public void testChar() {
		StaticHelper.reset();
		
		/* Get */
		char cFromVH = (char)VH_CHAR.get();
		Assert.assertEquals('1', cFromVH);
		
		/* Set */
		VH_CHAR.set('2');
		Assert.assertEquals('2', StaticHelper.c);
		
		/* GetOpaque */
		cFromVH = (char) VH_CHAR.getOpaque();
		Assert.assertEquals('2', cFromVH);
		
		/* SetOpaque */
		VH_CHAR.setOpaque('3');
		Assert.assertEquals('3', StaticHelper.c);
		
		/* GetVolatile */
		cFromVH = (char) VH_CHAR.getVolatile();
		Assert.assertEquals('3', cFromVH);
		
		/* SetVolatile */
		VH_CHAR.setVolatile('4');
		Assert.assertEquals('4', StaticHelper.c);
		
		/* GetAcquire */
		cFromVH = (char) VH_CHAR.getAcquire();
		Assert.assertEquals('4', cFromVH);
		
		/* SetRelease */
		VH_CHAR.setRelease('5');
		Assert.assertEquals('5', StaticHelper.c);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_CHAR.compareAndSet('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.compareAndSet('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		char caeResult = (char)VH_CHAR.compareAndExchange('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertEquals('1', caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchange('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeAcquire('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertEquals('1', caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (char)VH_CHAR.compareAndExchangeRelease('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertEquals('1', caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (char)VH_CHAR.compareAndExchangeRelease('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertEquals('1', caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_CHAR.weakCompareAndSet('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSet('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetAcquire('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetRelease('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_CHAR.weakCompareAndSetPlain('2', '3');
		Assert.assertEquals('1', StaticHelper.c);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_CHAR.weakCompareAndSetPlain('1', '2');
		Assert.assertEquals('2', StaticHelper.c);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndSet('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', StaticHelper.c);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndSetAcquire('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', StaticHelper.c);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndSetRelease('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('2', StaticHelper.c);
		
		/* GetAndAdd */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndAdd('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', StaticHelper.c);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndAddAcquire('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', StaticHelper.c);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndAddRelease('2');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1' + '2', StaticHelper.c);
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseAnd('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', StaticHelper.c);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseAndAcquire('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', StaticHelper.c);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseAndRelease('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('1', StaticHelper.c);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseOr('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', StaticHelper.c);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseOrAcquire('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', StaticHelper.c);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseOrRelease('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals('3', StaticHelper.c);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseXor('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, StaticHelper.c);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseXorAcquire('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, StaticHelper.c);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		cFromVH = (char)VH_CHAR.getAndBitwiseXorRelease('3');
		Assert.assertEquals('1', cFromVH);
		Assert.assertEquals((char)2, StaticHelper.c);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link double} field.
	 */
	@Test
	public void testDouble() {
		StaticHelper.reset();
		
		/* Get */
		double dFromVH = (double)VH_DOUBLE.get();
		Assert.assertEquals(1.0, dFromVH);
		
		/* Set */
		VH_DOUBLE.set(2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		
		/* GetOpaque */
		dFromVH = (double) VH_DOUBLE.getOpaque();
		Assert.assertEquals(2.0, dFromVH);
		
		/* SetOpaque */
		VH_DOUBLE.setOpaque(3.0);
		Assert.assertEquals(3.0, StaticHelper.d);
		
		/* GetVolatile */
		dFromVH = (double) VH_DOUBLE.getVolatile();
		Assert.assertEquals(3.0, dFromVH);
		
		/* SetVolatile */
		VH_DOUBLE.setVolatile(4.0);
		Assert.assertEquals(4.0, StaticHelper.d);
		
		/* GetAcquire */
		dFromVH = (double) VH_DOUBLE.getAcquire();
		Assert.assertEquals(4.0, dFromVH);
		
		/* SetRelease */
		VH_DOUBLE.setRelease(5.0);
		Assert.assertEquals(5.0, StaticHelper.d);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_DOUBLE.compareAndSet(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.compareAndSet(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		double caeResult = (double)VH_DOUBLE.compareAndExchange(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchange(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeAcquire(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (double)VH_DOUBLE.compareAndExchangeRelease(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertEquals(1.0, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSet(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetAcquire(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetRelease(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetPlain(2.0, 3.0);
		Assert.assertEquals(1.0, StaticHelper.d);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_DOUBLE.weakCompareAndSetPlain(1.0, 2.0);
		Assert.assertEquals(2.0, StaticHelper.d);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndSet(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, StaticHelper.d);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndSetAcquire(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, StaticHelper.d);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndSetRelease(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(2.0, StaticHelper.d);
		
		/* GetAndAdd */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndAdd(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, StaticHelper.d);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndAddAcquire(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, StaticHelper.d);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		dFromVH = (double)VH_DOUBLE.getAndAddRelease(2.0);
		Assert.assertEquals(1.0, dFromVH);
		Assert.assertEquals(3.0, StaticHelper.d);
		
		/* getAndBitwiseAnd */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAnd(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAndAcquire(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseAndRelease(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOr(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOrAcquire(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseOrRelease(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXor(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXorAcquire(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			dFromVH = (double)VH_DOUBLE.getAndBitwiseXorRelease(3.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link float} field.
	 */
	@Test
	public void testFloat() {
		StaticHelper.reset();
		
		/* Get */
		float fFromVH = (float)VH_FLOAT.get();
		Assert.assertEquals(1.0f, fFromVH);
		
		/* Set */
		VH_FLOAT.set(2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		
		/* GetOpaque */
		fFromVH = (float) VH_FLOAT.getOpaque();
		Assert.assertEquals(2.0f, fFromVH);
		
		/* SetOpaque */
		VH_FLOAT.setOpaque(3.0f);
		Assert.assertEquals(3.0f, StaticHelper.f);
		
		/* GetVolatile */
		fFromVH = (float) VH_FLOAT.getVolatile();
		Assert.assertEquals(3.0f, fFromVH);
		
		/* SetVolatile */
		VH_FLOAT.setVolatile(4.0f);
		Assert.assertEquals(4.0f, StaticHelper.f);
		
		/* GetAcquire */
		fFromVH = (float) VH_FLOAT.getAcquire();
		Assert.assertEquals(4.0f, fFromVH);
		
		/* SetRelease */
		VH_FLOAT.setRelease(5.0f);
		Assert.assertEquals(5.0f, StaticHelper.f);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_FLOAT.compareAndSet(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.compareAndSet(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		float caeResult = (float)VH_FLOAT.compareAndExchange(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchange(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeAcquire(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (float)VH_FLOAT.compareAndExchangeRelease(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertEquals(1.0f, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSet(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetAcquire(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetRelease(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_FLOAT.weakCompareAndSetPlain(2.0f, 3.0f);
		Assert.assertEquals(1.0f, StaticHelper.f);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_FLOAT.weakCompareAndSetPlain(1.0f, 2.0f);
		Assert.assertEquals(2.0f, StaticHelper.f);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndSet(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, StaticHelper.f);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndSetAcquire(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, StaticHelper.f);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndSetRelease(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(2.0f, StaticHelper.f);
		
		/* GetAndAdd */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndAdd(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, StaticHelper.f);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndAddAcquire(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, StaticHelper.f);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		fFromVH = (float)VH_FLOAT.getAndAddRelease(2.0f);
		Assert.assertEquals(1.0f, fFromVH);
		Assert.assertEquals(3.0f, StaticHelper.f);
		
		/* getAndBitwiseAnd */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAnd(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAndAcquire(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseAndRelease(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOr(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOrAcquire(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseOrRelease(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXor(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXorAcquire(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			fFromVH = (float)VH_FLOAT.getAndBitwiseXorRelease(3.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link int} field.
	 */
	@Test
	public void testInt() {
		StaticHelper.reset();
		
		/* Get */
		int iFromVH = (int)VH_INT.get();
		Assert.assertEquals(1, iFromVH);
		
		/* Set */
		VH_INT.set(2);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* GetOpaque */
		iFromVH = (int) VH_INT.getOpaque();
		Assert.assertEquals(2, iFromVH);
		
		/* SetOpaque */
		VH_INT.setOpaque(3);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* GetVolatile */
		iFromVH = (int) VH_INT.getVolatile();
		Assert.assertEquals(3, iFromVH);
		
		/* SetVolatile */
		VH_INT.setVolatile(4);
		Assert.assertEquals(4, StaticHelper.i);
		
		/* GetAcquire */
		iFromVH = (int) VH_INT.getAcquire();
		Assert.assertEquals(4, iFromVH);
		
		/* SetRelease */
		VH_INT.setRelease(5);
		Assert.assertEquals(5, StaticHelper.i);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_INT.compareAndSet(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_INT.compareAndSet(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		int caeResult = (int)VH_INT.compareAndExchange(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertEquals(1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (int)VH_INT.compareAndExchange(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (int)VH_INT.compareAndExchangeAcquire(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeAcquire(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertEquals(1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (int)VH_INT.compareAndExchangeRelease(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertEquals(1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (int)VH_INT.compareAndExchangeRelease(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertEquals(1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_INT.weakCompareAndSet(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSet(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetAcquire(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetRelease(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_INT.weakCompareAndSetPlain(2, 3);
		Assert.assertEquals(1, StaticHelper.i);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_INT.weakCompareAndSetPlain(1, 2);
		Assert.assertEquals(2, StaticHelper.i);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndSet(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndSetAcquire(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndSetRelease(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* GetAndAdd */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndAdd(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndAddAcquire(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndAddRelease(2);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseAnd(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, StaticHelper.i);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseAndAcquire(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, StaticHelper.i);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseAndRelease(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(1, StaticHelper.i);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseOr(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseOrAcquire(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseOrRelease(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(3, StaticHelper.i);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseXor(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseXorAcquire(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		iFromVH = (int)VH_INT.getAndBitwiseXorRelease(3);
		Assert.assertEquals(1, iFromVH);
		Assert.assertEquals(2, StaticHelper.i);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link int} field.
	 */
	@Test
	public void testLong() {
		StaticHelper.reset();
		
		/* Get */
		long jFromVH = (long)VH_LONG.get();
		Assert.assertEquals(1L, jFromVH);
		
		/* Set */
		VH_LONG.set(2L);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* GetOpaque */
		jFromVH = (long) VH_LONG.getOpaque();
		Assert.assertEquals(2L, jFromVH);
		
		/* SetOpaque */
		VH_LONG.setOpaque(3L);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* GetVolatile */
		jFromVH = (long) VH_LONG.getVolatile();
		Assert.assertEquals(3L, jFromVH);
		
		/* SetVolatile */
		VH_LONG.setVolatile(4L);
		Assert.assertEquals(4L, StaticHelper.j);
		
		/* GetAcquire */
		jFromVH = (long) VH_LONG.getAcquire();
		Assert.assertEquals(4L, jFromVH);
		
		/* SetRelease */
		VH_LONG.setRelease(5L);
		Assert.assertEquals(5L, StaticHelper.j);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_LONG.compareAndSet(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.compareAndSet(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		long caeResult = (long)VH_LONG.compareAndExchange(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (long)VH_LONG.compareAndExchange(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeAcquire(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (long)VH_LONG.compareAndExchangeRelease(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (long)VH_LONG.compareAndExchangeRelease(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertEquals(1L, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_LONG.weakCompareAndSet(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSet(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetAcquire(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetRelease(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_LONG.weakCompareAndSetPlain(2L, 3L);
		Assert.assertEquals(1L, StaticHelper.j);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_LONG.weakCompareAndSetPlain(1L, 2L);
		Assert.assertEquals(2L, StaticHelper.j);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndSet(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndSetAcquire(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndSetRelease(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* GetAndAdd */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndAdd(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndAddAcquire(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndAddRelease(2L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseAnd(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, StaticHelper.j);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseAndAcquire(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, StaticHelper.j);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseAndRelease(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(1L, StaticHelper.j);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseOr(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseOrAcquire(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseOrRelease(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(3L, StaticHelper.j);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseXor(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseXorAcquire(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		jFromVH = (long)VH_LONG.getAndBitwiseXorRelease(3L);
		Assert.assertEquals(1L, jFromVH);
		Assert.assertEquals(2L, StaticHelper.j);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link String} field.
	 */
	@Test
	public void testReference() {
		StaticHelper.reset();
		
		/* Get */
		String lFromVH = (String)VH_STRING.get();
		Assert.assertEquals("1", lFromVH);
		
		/* Set */
		VH_STRING.set("2");
		Assert.assertEquals("2", StaticHelper.l1);
		
		/* GetOpaque */
		lFromVH = (String) VH_STRING.getOpaque();
		Assert.assertEquals("2", lFromVH);
		
		/* SetOpaque */
		VH_STRING.setOpaque("3");
		Assert.assertEquals("3", StaticHelper.l1);
		
		/* GetVolatile */
		lFromVH = (String) VH_STRING.getVolatile();
		Assert.assertEquals("3", lFromVH);
		
		/* SetVolatile */
		VH_STRING.setVolatile("4");
		Assert.assertEquals("4", StaticHelper.l1);
		
		/* GetAcquire */
		lFromVH = (String) VH_STRING.getAcquire();
		Assert.assertEquals("4", lFromVH);
		
		/* SetRelease */
		VH_STRING.setRelease("5");
		Assert.assertEquals("5", StaticHelper.l1);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_STRING.compareAndSet("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.compareAndSet("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		String caeResult = (String)VH_STRING.compareAndExchange("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (String)VH_STRING.compareAndExchange("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (String)VH_STRING.compareAndExchangeAcquire("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeAcquire("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (String)VH_STRING.compareAndExchangeRelease("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (String)VH_STRING.compareAndExchangeRelease("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertEquals("1", caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_STRING.weakCompareAndSet("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSet("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetAcquire("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetRelease("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_STRING.weakCompareAndSetPlain("2", "3");
		Assert.assertEquals("1", StaticHelper.l1);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_STRING.weakCompareAndSetPlain("1", "2");
		Assert.assertEquals("2", StaticHelper.l1);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		lFromVH = (String)VH_STRING.getAndSet("2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", StaticHelper.l1);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		lFromVH = (String)VH_STRING.getAndSetAcquire("2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", StaticHelper.l1);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		lFromVH = (String)VH_STRING.getAndSetRelease("2");
		Assert.assertEquals("1", lFromVH);
		Assert.assertEquals("2", StaticHelper.l1);
		
		/* GetAndAdd */
		StaticHelper.reset();
		try {
			lFromVH = (String)VH_STRING.getAndAdd("2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		try {
			lFromVH = (String)VH_STRING.getAndAddAcquire("2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		try {
			lFromVH = (String)VH_STRING.getAndAddRelease("2");
			Assert.fail("Successfully added reference types (String)");
		} catch (UnsupportedOperationException e) {	}
		
		/* getAndBitwiseAnd */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAnd("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAndAcquire("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseAndRelease("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOr("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOrAcquire("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseOrRelease("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXor("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXorAcquire("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			lFromVH = (String)VH_STRING.getAndBitwiseXorRelease("3");
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link Class} field.
	 */
	@Test
	public void testReferenceOtherType() {
		StaticHelper.reset();
		
		/* Get */
		Class<?> lFromVH = (Class<?>)VH_CLASS.get();
		Assert.assertEquals(String.class, lFromVH);
		
		/* Set */
		VH_CLASS.set(Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);

		StaticHelper.reset();
		/* GetOpaque */
		lFromVH = (Class<?>) VH_CLASS.getOpaque();
		Assert.assertEquals(String.class, lFromVH);
		
		/* SetOpaque */
		VH_CLASS.setOpaque(Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);

		StaticHelper.reset();
		/* GetVolatile */
		lFromVH = (Class<?>) VH_CLASS.getVolatile();
		Assert.assertEquals(String.class, lFromVH);
		
		/* SetVolatile */
		VH_CLASS.setVolatile(Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);

		StaticHelper.reset();
		/* GetAcquire */
		lFromVH = (Class<?>) VH_CLASS.getAcquire();
		Assert.assertEquals(String.class, lFromVH);
		
		/* SetRelease */
		VH_CLASS.setRelease(Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);

		StaticHelper.reset();
		/* CompareAndSet - Fail */
		StaticHelper.l2 = String.class;
		boolean casResult = (boolean)VH_CLASS.compareAndSet(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_CLASS.compareAndSet(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertTrue(casResult);

		StaticHelper.reset();
		/* compareAndExchange - Fail */
		StaticHelper.l2 = String.class;
		Class<?> caeResult = (Class<?>)VH_CLASS.compareAndExchange(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchange(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		StaticHelper.reset();
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.l2 = String.class;
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeAcquire(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeAcquire(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		StaticHelper.reset();
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.l2 = String.class;
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeRelease(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (Class<?>)VH_CLASS.compareAndExchangeRelease(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertEquals(String.class, caeResult);

		StaticHelper.reset();
		/* WeakCompareAndSet - Fail */
		StaticHelper.l2 = String.class;
		casResult = (boolean)VH_CLASS.weakCompareAndSet(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSet(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertTrue(casResult);

		StaticHelper.reset();
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.l2 = String.class;
		casResult = (boolean)VH_CLASS.weakCompareAndSetAcquire(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSetAcquire(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertTrue(casResult);

		StaticHelper.reset();
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.l2 = String.class;
		casResult = (boolean)VH_CLASS.weakCompareAndSetRelease(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSetRelease(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertTrue(casResult);

		StaticHelper.reset();
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.l2 = String.class;
		casResult = (boolean)VH_CLASS.weakCompareAndSetPlain(Integer.class, Object.class);
		Assert.assertEquals(String.class, StaticHelper.l2);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_CLASS.weakCompareAndSetPlain(String.class, Integer.class);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		Assert.assertTrue(casResult);

		StaticHelper.reset();
		/* GetAndSet */
		StaticHelper.l2 = String.class;
		lFromVH = (Class<?>)VH_CLASS.getAndSet(Integer.class);
		Assert.assertEquals(String.class, lFromVH);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		
		/* GetAndSetAcquire */
		StaticHelper.l2 = String.class;
		lFromVH = (Class<?>)VH_CLASS.getAndSetAcquire(Integer.class);
		Assert.assertEquals(String.class, lFromVH);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		
		/* GetAndSetRelease */
		StaticHelper.l2 = String.class;
		lFromVH = (Class<?>)VH_CLASS.getAndSetRelease(Integer.class);
		Assert.assertEquals(String.class, lFromVH);
		Assert.assertEquals(Integer.class, StaticHelper.l2);
		
		/* GetAndAdd */
		StaticHelper.l2 = String.class;
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndAdd(Integer.class);
			Assert.fail("Successfully added reference types (Class<?>)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddAcquire */
		StaticHelper.l2 = String.class;
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndAddAcquire(Integer.class);
			Assert.fail("Successfully added reference types (Class<?>)");
		} catch (UnsupportedOperationException e) {	}
		
		/* GetAndAddRelease */
		StaticHelper.l2 = String.class;
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndAddRelease(Integer.class);
			Assert.fail("Successfully added reference types (Class<?>)");
		} catch (UnsupportedOperationException e) {	}
		
		/* getAndBitwiseAnd */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseAnd(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseAndAcquire(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseAndRelease(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseOr(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseOrAcquire(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseOrRelease(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseXor(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseXorAcquire(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			lFromVH = (Class<?>)VH_CLASS.getAndBitwiseXorRelease(Integer.class);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link short} field.
	 */
	@Test
	public void testShort() {
		StaticHelper.reset();
		
		/* Get */
		short sFromVH = (short)VH_SHORT.get();
		Assert.assertEquals((short)1, sFromVH);
		
		/* Set */
		VH_SHORT.set((short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* GetOpaque */
		sFromVH = (short) VH_SHORT.getOpaque();
		Assert.assertEquals((short)2, sFromVH);
		
		/* SetOpaque */
		VH_SHORT.setOpaque((short)3);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* GetVolatile */
		sFromVH = (short) VH_SHORT.getVolatile();
		Assert.assertEquals((short)3, sFromVH);
		
		/* SetVolatile */
		VH_SHORT.setVolatile((short)4);
		Assert.assertEquals((short)4, StaticHelper.s);
		
		/* GetAcquire */
		sFromVH = (short) VH_SHORT.getAcquire();
		Assert.assertEquals((short)4, sFromVH);
		
		/* SetRelease */
		VH_SHORT.setRelease((short)5);
		Assert.assertEquals((short)5, StaticHelper.s);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_SHORT.compareAndSet((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.compareAndSet((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		short caeResult = (short)VH_SHORT.compareAndExchange((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchange((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeAcquire((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (short)VH_SHORT.compareAndExchangeRelease((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (short)VH_SHORT.compareAndExchangeRelease((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertEquals((short)1, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_SHORT.weakCompareAndSet((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSet((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetAcquire((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetRelease((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_SHORT.weakCompareAndSetPlain((short)2, (short)3);
		Assert.assertEquals((short)1, StaticHelper.s);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_SHORT.weakCompareAndSetPlain((short)1, (short)2);
		Assert.assertEquals((short)2, StaticHelper.s);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndSet((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndSetAcquire((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndSetRelease((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* GetAndAdd */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndAdd((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* GetAndAddAcquire */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndAddAcquire((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* GetAndAddRelease */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndAddRelease((short)2);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseAnd((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, StaticHelper.s);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseAndAcquire((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, StaticHelper.s);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseAndRelease((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)1, StaticHelper.s);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseOr((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseOrAcquire((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseOrRelease((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)3, StaticHelper.s);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseXor((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseXorAcquire((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		sFromVH = (short)VH_SHORT.getAndBitwiseXorRelease((short)3);
		Assert.assertEquals((short)1, sFromVH);
		Assert.assertEquals((short)2, StaticHelper.s);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a {@link boolean} field.
	 */
	@Test
	public void testBoolean() {
		StaticHelper.reset();
		
		/* Get */
		boolean zFromVH = (boolean)VH_BOOLEAN.get();
		Assert.assertEquals(true, zFromVH);
		
		/* Set */
		VH_BOOLEAN.set(false);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* GetOpaque */
		zFromVH = (boolean) VH_BOOLEAN.getOpaque();
		Assert.assertEquals(false, zFromVH);
		
		/* SetOpaque */
		VH_BOOLEAN.setOpaque(true);
		Assert.assertEquals(true, StaticHelper.z);
		
		/* GetVolatile */
		zFromVH = (boolean) VH_BOOLEAN.getVolatile();
		Assert.assertEquals(true, zFromVH);
		
		/* SetVolatile */
		VH_BOOLEAN.setVolatile(false);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* GetAcquire */
		zFromVH = (boolean) VH_BOOLEAN.getAcquire();
		Assert.assertEquals(false, zFromVH);
		
		/* SetRelease */
		VH_BOOLEAN.setRelease(true);
		Assert.assertEquals(true, StaticHelper.z);
		
		/* CompareAndSet - Fail */
		StaticHelper.reset();
		boolean casResult = (boolean)VH_BOOLEAN.compareAndSet(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertFalse(casResult);

		/* CompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.compareAndSet(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertTrue(casResult);
		
		/* compareAndExchange - Fail */
		StaticHelper.reset();
		boolean caeResult = (boolean)VH_BOOLEAN.compareAndExchange(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertEquals(true, caeResult);

		/* compareAndExchange - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchange(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeAcquire - Fail */
		StaticHelper.reset();
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeAcquire - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeAcquire(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertEquals(true, caeResult);
		
		/* CompareAndExchangeRelease - Fail */
		StaticHelper.reset();
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertEquals(true, caeResult);

		/* CompareAndExchangeRelease - Succeed */
		caeResult = (boolean)VH_BOOLEAN.compareAndExchangeRelease(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertEquals(true, caeResult);
		
		/* WeakCompareAndSet - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSet - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSet(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetAcquire - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetAcquire - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetAcquire(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetRelease - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetRelease - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetRelease(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertTrue(casResult);
		
		/* WeakCompareAndSetPlain - Fail */
		StaticHelper.reset();
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetPlain(false, false);
		Assert.assertEquals(true, StaticHelper.z);
		Assert.assertFalse(casResult);

		/* WeakCompareAndSetPlain - Succeed */
		casResult = (boolean)VH_BOOLEAN.weakCompareAndSetPlain(true, false);
		Assert.assertEquals(false, StaticHelper.z);
		Assert.assertTrue(casResult);
		
		/* GetAndSet */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndSet(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* GetAndSetAcquire */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndSetAcquire(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* GetAndSetRelease */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndSetRelease(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);

		try {
			/* GetAndAdd */
			zFromVH = (boolean)VH_BOOLEAN.getAndAdd(false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		try {
			/* GetAndAddAcquire */
			zFromVH = (boolean)VH_BOOLEAN.getAndAddAcquire(false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		try {
			/* GetAndAddRelease */
			zFromVH = (boolean)VH_BOOLEAN.getAndAddRelease(false);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAnd */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAnd(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* getAndBitwiseAndAcquire */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAndAcquire(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* getAndBitwiseAndRelease */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseAndRelease(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* getAndBitwiseOr */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOr(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, StaticHelper.z);
		
		/* getAndBitwiseOrAcquire */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOrAcquire(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, StaticHelper.z);
		
		/* getAndBitwiseOrRelease */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseOrRelease(false);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(true, StaticHelper.z);
		
		/* getAndBitwiseXor */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXor(true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* getAndBitwiseXorAcquire */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXorAcquire(true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
		
		/* getAndBitwiseXorRelease */
		StaticHelper.reset();
		zFromVH = (boolean)VH_BOOLEAN.getAndBitwiseXorRelease(true);
		Assert.assertEquals(true, zFromVH);
		Assert.assertEquals(false, StaticHelper.z);
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a <b>final</b> {@link int} field.
	 */
	@Test
	public void testFinalField() {
		StaticHelper.reset();
		
		// Getting value of final static field
		int result = (int) VH_FINAL_INT.get();
		Assert.assertEquals(result, StaticHelper.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.set(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getVolatile();
		Assert.assertEquals(result, StaticHelper.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setVolatile(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getOpaque();
		Assert.assertEquals(result, StaticHelper.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setOpaque(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Getting value of final static field
		result = (int) VH_FINAL_INT.getAcquire();
		Assert.assertEquals(result, StaticHelper.finalI);
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.setRelease(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndSet(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchange(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchangeAcquire(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.compareAndExchangeRelease(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSet(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetAcquire(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetRelease(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.weakCompareAndSetPlain(5, 10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSet(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSetAcquire(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndSetRelease(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAdd(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAddAcquire(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		// Setting value of final static field
		try {
			VH_FINAL_INT.getAndAddRelease(10);
			Assert.fail("Successfully set the value of a final field.");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAnd */
		try {
			VH_FINAL_INT.getAndBitwiseAnd(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseAndAcquire(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseAndRelease */
		try {
			VH_FINAL_INT.getAndBitwiseAndRelease(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOr */
		try {
			VH_FINAL_INT.getAndBitwiseOr(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseOrAcquire(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseOrRelease */
		try {
			VH_FINAL_INT.getAndBitwiseOrRelease(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXor */
		try {
			VH_FINAL_INT.getAndBitwiseXor(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorAcquire */
		try {
			VH_FINAL_INT.getAndBitwiseXorAcquire(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
		
		/* getAndBitwiseXorRelease */
		try {
			VH_FINAL_INT.getAndBitwiseXorRelease(3);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}
	
	/**
	 * Perform all the operations available on a StaticFieldViewVarHandle referencing a <b>final</b> {@link int} field.
	 * Note: The static field StaticHelper.StaticNoInitializationHelper.finalI hasn't been initialized implicitly
	 * by StaticHelper.reset() before VarHandle.get() is invoked.
	 */
	@Test
	public void testFinalFieldInClassNotInitialized() {
		// Getting value of final static field
		int result = (int) VH_FINAL_NOINIT_INT.get();
		Assert.assertEquals(result, StaticHelper.StaticNoInitializationHelper.finalI);
	}
	
	/**
	 * Call {@link StaticFieldVarHandleTests#commonVarHandleCallSite(VarHandle, Object) commonVarHandleCallSite}
	 * using different VarHandles:
	 * <ul>
	 * <li>{@link byte} field</li>
	 * <li>{@link short} field</li>
	 * <li>{@link int} field</li>
	 * <li>{@link long} field</li>
	 * </ul>
	 */
	@Test
	public void testCommonCallSite() {
		commonVarHandleCallSite(VH_BYTE, StaticHelper.b);
		commonVarHandleCallSite(VH_SHORT, StaticHelper.s);
		commonVarHandleCallSite(VH_INT, StaticHelper.i);
		commonVarHandleCallSite(VH_LONG, StaticHelper.j);
	}
	
	private long commonVarHandleCallSite(VarHandle vh, long value) {
		return (long)vh.get();
	}
}
