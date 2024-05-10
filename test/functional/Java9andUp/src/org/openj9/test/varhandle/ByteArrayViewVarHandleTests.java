/*
 * Copyright IBM Corp. and others 2016
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
 */
package org.openj9.test.varhandle;

import org.testng.annotations.*;
import org.testng.*;

import java.lang.invoke.*;
import java.nio.*;

/**
 * Test ByteArrayViewVarHandle operations
 *
 * @author Bjorn Vardal
 */
@Test(groups = { "level.extended" })
public class ByteArrayViewVarHandleTests extends ViewVarHandleTests {
	/**
	 * Initialize the expected values based on the provided {@code byteOrder}.
	 * Initialize the {@link VarHandle VarHandles} based on the provided {@code byteOrder}.
	 *
	 * @param byteOrder "bigEndian" or "littleEndian". Determines the {@link ByteOrder} to use when reading and writing.
	 */
	@Parameters({ "byteOrder" })
	public ByteArrayViewVarHandleTests(String byteOrder) {
		super(byteOrder);

		// Wrap the backing byte[] in a ByteBuffer in order to share behaviour with ByteBufferViewVarHandle tests
		_buffer = ByteBuffer.wrap(ByteArrayViewHelper.b);

		vhChar = MethodHandles.byteArrayViewVarHandle(char[].class, _byteOrder);
		vhDouble = MethodHandles.byteArrayViewVarHandle(double[].class, _byteOrder);
		vhFloat = MethodHandles.byteArrayViewVarHandle(float[].class, _byteOrder);
		vhInt = MethodHandles.byteArrayViewVarHandle(int[].class, _byteOrder);
		vhLong = MethodHandles.byteArrayViewVarHandle(long[].class, _byteOrder);
		vhShort = MethodHandles.byteArrayViewVarHandle(short[].class, _byteOrder);
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as char elements.
	 */
	@Test
	public void testChar() {

		ByteArrayViewHelper.reset();
		/* Get */
		char cFromVH = (char)vhChar.get(ByteArrayViewHelper.b, 0);
		Assert.assertEquals(FIRST_CHAR, cFromVH);

		/* Set */
		vhChar.set(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
		checkUpdated2(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			cFromVH = (char) vhChar.getOpaque(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_CHAR, cFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhChar.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			cFromVH = (char) vhChar.getVolatile(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_CHAR, cFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhChar.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			cFromVH = (char) vhChar.getAcquire(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_CHAR, cFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhChar.setRelease(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		@SuppressWarnings("unused")
		boolean casResult;
		@SuppressWarnings("unused")
		char caeResult;

		/* CompareAndSet */
		try {
			casResult = vhChar.compareAndSet(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchange */
		try {
			caeResult = (char)vhChar.compareAndExchange(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchangeAcquire */
		try {
			caeResult = (char)vhChar.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchangeRelease */
		try {
			caeResult = (char)vhChar.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSet */
		try {
			casResult = vhChar.weakCompareAndSet(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetAcquire */
		try {
			casResult = vhChar.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetRelease */
		try {
			casResult = vhChar.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetPlain */
		try {
			casResult = vhChar.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, '1', '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSet */
		try {
			cFromVH = (char)vhChar.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSetAcquire */
		try {
			cFromVH = (char)vhChar.getAndSetAcquire(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSetRelease */
		try {
			cFromVH = (char)vhChar.getAndSetRelease(ByteArrayViewHelper.b, 0, CHANGED_CHAR);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAdd */
		try {
			cFromVH = (char)vhChar.getAndAdd(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddAcquire */
		try {
			cFromVH = (char)vhChar.getAndAddAcquire(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddRelease */
		try {
			cFromVH = (char)vhChar.getAndAddRelease(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAnd */
		try {
			cFromVH = (char)vhChar.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndAcquire */
		try {
			cFromVH = (char)vhChar.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndRelease */
		try {
			cFromVH = (char)vhChar.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOr */
		try {
			cFromVH = (char)vhChar.getAndBitwiseOr(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrAcquire */
		try {
			cFromVH = (char)vhChar.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrRelease */
		try {
			cFromVH = (char)vhChar.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXor */
		try {
			cFromVH = (char)vhChar.getAndBitwiseXor(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorAcquire */
		try {
			cFromVH = (char)vhChar.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorRelease */
		try {
			cFromVH = (char)vhChar.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, '2');
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as char elements.
	 */
	@Test
	public void testCharLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Character.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		char cFromVH = (char)vhChar.get(ByteArrayViewHelper.b, lastCompleteIndex);
		Assert.assertEquals(LAST_CHAR, cFromVH);

		/* Set */
		vhChar.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_CHAR);
		checkUpdated2(lastCompleteIndex);
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as double elements.
	 */
	@Test
	public void testDouble() {
		ByteArrayViewHelper.reset();
		/* Get */
		double dFromVH = (double)vhDouble.get(ByteArrayViewHelper.b, 0);
		assertEquals(FIRST_DOUBLE, dFromVH);

		/* Set */
		vhDouble.set(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
		checkUpdated8(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			dFromVH = (double)vhDouble.getOpaque(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhDouble.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			dFromVH = (double)vhDouble.getVolatile(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhDouble.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			dFromVH = (double)vhDouble.getAcquire(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhDouble.setRelease(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		boolean casResult = false;
		try {
			casResult = vhDouble.compareAndSet(ByteArrayViewHelper.b, 0, 2.0, 3.0);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.compareAndSet(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Fail */
		ByteArrayViewHelper.reset();
		double caeResult = 0;
		try {
			caeResult = (double)vhDouble.compareAndExchange(ByteArrayViewHelper.b, 0, 1.0, 2.0);
			checkNotUpdated8();
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (double)vhDouble.compareAndExchange(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (double)vhDouble.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, 1.0, 2.0);
			checkNotUpdated8();
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeAcquire - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (double)vhDouble.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (double)vhDouble.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, 1.0, 2.0);
			checkNotUpdated8();
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeRelease - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (double)vhDouble.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSet(ByteArrayViewHelper.b, 0, 2.0, 3.0);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSet(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, 2.0, 3.0);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, 2.0, 3.0);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetPlain - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, 2.0, 3.0);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetPlain - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhDouble.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, FIRST_DOUBLE, CHANGED_DOUBLE);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			dFromVH = (double)vhDouble.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			dFromVH = (double)vhDouble.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			dFromVH = (double)vhDouble.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_DOUBLE);
			checkUpdated8(0);
			assertEquals(FIRST_DOUBLE, dFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAdd */
		try {
			dFromVH = (double)vhDouble.getAndAdd(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddAcquire */
		try {
			dFromVH = (double)vhDouble.getAndAddAcquire(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddRelease */
		try {
			dFromVH = (double)vhDouble.getAndAddRelease(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAnd */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndAcquire */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndRelease */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOr */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseOr(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrAcquire */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrRelease */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXor */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseXor(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorAcquire */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorRelease */
		try {
			dFromVH = (double)vhDouble.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, 2.0);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as double elements.
	 */
	@Test
	public void testDoubleLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Double.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		double dFromVH = (double)vhDouble.get(ByteArrayViewHelper.b, lastCompleteIndex);
		assertEquals(LAST_DOUBLE, dFromVH);

		/* Set */
		vhDouble.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_DOUBLE);
		checkUpdated8(lastCompleteIndex);
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as float elements.
	 */
	@Test
	public void testFloat() {
		ByteArrayViewHelper.reset();
		/* Get */
		float fFromVH = (float)vhFloat.get(ByteArrayViewHelper.b, 0);
		assertEquals(FIRST_FLOAT, fFromVH);

		/* Set */
		vhFloat.set(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
		checkUpdated4(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			fFromVH = (float)vhFloat.getOpaque(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhFloat.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			fFromVH = (float)vhFloat.getVolatile(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhFloat.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			fFromVH = (float)vhFloat.getAcquire(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhFloat.setRelease(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		boolean casResult = false;
		try {
			casResult = vhFloat.compareAndSet(ByteArrayViewHelper.b, 0, 2.0f, 3.0f);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.compareAndSet(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Fail */
		ByteArrayViewHelper.reset();
		float caeResult = 0;
		try {
			caeResult = (float)vhFloat.compareAndExchange(ByteArrayViewHelper.b, 0, 1.0f, 2.0f);
			checkNotUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (float)vhFloat.compareAndExchange(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (float)vhFloat.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, 1.0f, 2.0f);
			checkNotUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeAcquire - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (float)vhFloat.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (float)vhFloat.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, 1.0f, 2.0f);
			checkNotUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchangeRelease - Succeed */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (float)vhFloat.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSet(ByteArrayViewHelper.b, 0, 2.0f, 3.0f);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSet(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, 2.0f, 3.0f);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, 2.0f, 3.0f);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetPlain - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, 2.0f, 3.0f);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetPlain - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = vhFloat.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, FIRST_FLOAT, CHANGED_FLOAT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			fFromVH = (float)vhFloat.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			fFromVH = (float)vhFloat.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			fFromVH = (float)vhFloat.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_FLOAT);
			checkUpdated4(0);
			assertEquals(FIRST_FLOAT, fFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAdd */
		try {
			fFromVH = (float)vhFloat.getAndAdd(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddAcquire */
		try {
			fFromVH = (float)vhFloat.getAndAddAcquire(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddRelease */
		try {
			fFromVH = (float)vhFloat.getAndAddRelease(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAnd */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndAcquire */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndRelease */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOr */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseOr(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrAcquire */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrRelease */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXor */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseXor(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorAcquire */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorRelease */
		try {
			fFromVH = (float)vhFloat.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, 2.0f);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as float elements.
	 */
	@Test
	public void testFloatLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Float.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		float fFromVH = (float)vhFloat.get(ByteArrayViewHelper.b, lastCompleteIndex);
		assertEquals(LAST_FLOAT, fFromVH);

		/* Set */
		vhFloat.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_FLOAT);
		checkUpdated4(lastCompleteIndex);
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as int elements.
	 */
	@Test
	public void testInt() {
		ByteArrayViewHelper.reset();
		/* Get */
		int iFromVH = (int)vhInt.get(ByteArrayViewHelper.b, 0);
		assertEquals(FIRST_INT, iFromVH);

		/* Set */
		vhInt.set(ByteArrayViewHelper.b, 0, CHANGED_INT);
		checkUpdated4(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			iFromVH = (int) vhInt.getOpaque(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_INT, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhInt.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			iFromVH = (int) vhInt.getVolatile(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_INT, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhInt.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			iFromVH = (int) vhInt.getAcquire(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_INT, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhInt.setRelease(ByteArrayViewHelper.b, 0, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		boolean casResult = false;
		try {
			casResult = (boolean)vhInt.compareAndSet(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.compareAndSet(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Fail */
		ByteArrayViewHelper.reset();
		int caeResult = 0;
		try {
			caeResult = (int)vhInt.compareAndExchange(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchange(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (int)vhInt.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (int)vhInt.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			assertEquals(FIRST_INT, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSet(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSet(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, 2, 3);
			checkNotUpdated4(0);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, FIRST_INT, CHANGED_INT);
			checkUpdated4(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_INT);
			assertEquals(FIRST_INT, iFromVH);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSetAcquire(ByteArrayViewHelper.b, 0, CHANGED_INT);
			assertEquals(FIRST_INT, iFromVH);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSetRelease(ByteArrayViewHelper.b, 0, CHANGED_INT);
			assertEquals(FIRST_INT, iFromVH);
			checkUpdated4(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAdd */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAdd(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_INT, iFromVH);
			checkIntAddition((byte)2, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAddAcquire(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_INT, iFromVH);
			checkIntAddition((byte)2, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAddRelease(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_INT, iFromVH);
			checkIntAddition((byte)2, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAnd*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, 7);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseAnd(7, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, 7);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseAnd(7, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, 7);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseAnd(7, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOr*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOr(ByteArrayViewHelper.b, 0, 1);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseOr(1, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, 1);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseOr(1, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, 1);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseOr(1, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXor*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXor(ByteArrayViewHelper.b, 0, 6);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseXor(6, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, 6);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseXor(6, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, 6);
			assertEquals(FIRST_INT, iFromVH);
			checkIntBitwiseXor(6, 0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as int elements.
	 */
	@Test
	public void testIntLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Integer.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		int fFromVH = (int)vhInt.get(ByteArrayViewHelper.b, lastCompleteIndex);
		assertEquals(LAST_INT, fFromVH);

		/* Set */
		vhInt.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_INT);
		checkUpdated4(lastCompleteIndex);
	}

	/**
	 * Tests all VarHandle access modes on a non-zero index in a byte[], viewed as int.
	 */
	@Test
	public void testIntNonZeroIndex() {
		ByteArrayViewHelper.reset();
		/* Get */
		int iFromVH = (int)vhInt.get(ByteArrayViewHelper.b, 4);
		assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);

		/* Set */
		vhInt.set(ByteArrayViewHelper.b, 4, CHANGED_INT);
		checkUpdated4(4);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			iFromVH = (int) vhInt.getOpaque(ByteArrayViewHelper.b, 4);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhInt.setOpaque(ByteArrayViewHelper.b, 4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			iFromVH = (int) vhInt.getVolatile(ByteArrayViewHelper.b, 4);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhInt.setVolatile(ByteArrayViewHelper.b, 4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			iFromVH = (int) vhInt.getAcquire(ByteArrayViewHelper.b, 4);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhInt.setRelease(ByteArrayViewHelper.b, 4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		boolean casResult = false;
		try {
			casResult = (boolean)vhInt.compareAndSet(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.compareAndSet(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Fail */
		ByteArrayViewHelper.reset();
		int caeResult = 0;
		try {
			caeResult = (int)vhInt.compareAndExchange(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchange(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (int)vhInt.compareAndExchangeAcquire(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchangeAcquire(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (int)vhInt.compareAndExchangeRelease(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Succeed */
		try {
			caeResult = (int)vhInt.compareAndExchangeRelease(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			assertEquals(INITIAL_INT_AT_INDEX_4, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSet(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSet(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhInt.weakCompareAndSetRelease(ByteArrayViewHelper.b, 4, 2, 3);
			checkNotUpdated4(4);
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Succeed */
		try {
			casResult = (boolean)vhInt.weakCompareAndSetRelease(ByteArrayViewHelper.b, 4, INITIAL_INT_AT_INDEX_4, CHANGED_INT);
			checkUpdated4(4);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSet(ByteArrayViewHelper.b, 4, CHANGED_INT);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSetAcquire(ByteArrayViewHelper.b, 4, CHANGED_INT);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndSetRelease(ByteArrayViewHelper.b, 4, CHANGED_INT);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkUpdated4(4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAdd */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAdd(ByteArrayViewHelper.b, 4, 2);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntAddition((byte)2, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAddAcquire(ByteArrayViewHelper.b, 4, 2);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntAddition((byte)2, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndAddRelease(ByteArrayViewHelper.b, 4, 2);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntAddition((byte)2, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAnd*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAnd(ByteArrayViewHelper.b, 4, 7);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseAnd(7, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 4, 7);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseAnd(7, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 4, 7);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseAnd(7, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOr*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOr(ByteArrayViewHelper.b, 4, 1);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseOr(1, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 4, 1);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseOr(1, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 4, 1);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseOr(1, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXor*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXor(ByteArrayViewHelper.b, 4, 6);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseXor(6, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 4, 6);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseXor(6, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (int)vhInt.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 4, 6);
			assertEquals(INITIAL_INT_AT_INDEX_4, iFromVH);
			checkIntBitwiseXor(6, 4);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as long elements.
	 */
	@Test
	public void testLong() {
		ByteArrayViewHelper.reset();
		/* Get */
		long iFromVH = (long)vhLong.get(ByteArrayViewHelper.b, 0);
		assertEquals(FIRST_LONG, iFromVH);

		/* Set */
		vhLong.set(ByteArrayViewHelper.b, 0, CHANGED_LONG);
		checkUpdated8(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			iFromVH = (long) vhLong.getOpaque(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_LONG, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhLong.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			iFromVH = (long) vhLong.getVolatile(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_LONG, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhLong.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			iFromVH = (long) vhLong.getAcquire(ByteArrayViewHelper.b, 0);
			assertEquals(FIRST_LONG, iFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhLong.setRelease(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		boolean casResult = false;
		try {
			casResult = (boolean)vhLong.compareAndSet(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndSet - Succeed */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhLong.compareAndSet(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Fail */
		ByteArrayViewHelper.reset();
		long caeResult = 0;
		try {
			caeResult = (long)vhLong.compareAndExchange(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* compareAndExchange - Succeed */
		try {
			caeResult = (long)vhLong.compareAndExchange(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (long)vhLong.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeAcquire - Succeed */
		try {
			caeResult = (long)vhLong.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			caeResult = (long)vhLong.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* CompareAndExchangeRelease - Succeed */
		try {
			caeResult = (long)vhLong.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			assertEquals(FIRST_LONG, caeResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhLong.weakCompareAndSet(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSet - Succeed */
		try {
			casResult = (boolean)vhLong.weakCompareAndSet(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhLong.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetAcquire - Succeed */
		try {
			casResult = (boolean)vhLong.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Fail */
		ByteArrayViewHelper.reset();
		try {
			casResult = (boolean)vhLong.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, 2L, 3L);
			checkNotUpdated8();
			Assert.assertFalse(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* WeakCompareAndSetRelease - Succeed */
		try {
			casResult = (boolean)vhLong.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, FIRST_LONG, CHANGED_LONG);
			checkUpdated8(0);
			Assert.assertTrue(casResult);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSet */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			assertEquals(FIRST_LONG, iFromVH);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndSetAcquire(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			assertEquals(FIRST_LONG, iFromVH);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndSetRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndSetRelease(ByteArrayViewHelper.b, 0, CHANGED_LONG);
			assertEquals(FIRST_LONG, iFromVH);
			checkUpdated8(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAdd */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndAdd(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongAddition();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddAcquire */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndAddAcquire(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongAddition();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndAddRelease */
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndAddRelease(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongAddition();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAnd*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, 15);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseAnd();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, 15);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseAnd();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseAndRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, 15);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseAnd();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOr*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseOr(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseOr();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseOr();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseOrRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, 2);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseOr();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXor*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseXor(ByteArrayViewHelper.b, 0, 10);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseXor();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorAcquire*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, 10);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseXor();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* GetAndBitwiseXorRelease*/
		ByteArrayViewHelper.reset();
		try {
			iFromVH = (long)vhLong.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, 10);
			assertEquals(FIRST_LONG, iFromVH);
			checkLongBitwiseXor();
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as long elements.
	 */
	@Test
	public void testLongLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Long.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		long fFromVH = (long)vhLong.get(ByteArrayViewHelper.b, lastCompleteIndex);
		assertEquals(LAST_LONG, fFromVH);

		/* Set */
		vhLong.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_LONG);
		checkUpdated8(lastCompleteIndex);
	}

	/**
	 * Perform all the operations available on a ByteArrayViewVarHandle viewed as long elements.
	 */
	@Test
	public void testShort() {

		ByteArrayViewHelper.reset();
		/* Get */
		short sFromVH = (short)vhShort.get(ByteArrayViewHelper.b, 0);
		Assert.assertEquals(FIRST_SHORT, sFromVH);

		/* Set */
		vhShort.set(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
		checkUpdated2(0);

		ByteArrayViewHelper.reset();
		/* GetOpaque */
		try {
			sFromVH = (short) vhShort.getOpaque(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_SHORT, sFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetOpaque */
		try {
			vhShort.setOpaque(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetVolatile */
		try {
			sFromVH = (short) vhShort.getVolatile(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_SHORT, sFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetVolatile */
		try {
			vhShort.setVolatile(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		ByteArrayViewHelper.reset();
		/* GetAcquire */
		try {
			sFromVH = (short) vhShort.getAcquire(ByteArrayViewHelper.b, 0);
			Assert.assertEquals(FIRST_SHORT, sFromVH);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		/* SetRelease */
		try {
			vhShort.setRelease(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			checkUpdated2(0);
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		@SuppressWarnings("unused")
		boolean casResult;
		@SuppressWarnings("unused")
		short caeResult;

		/* CompareAndSet */
		try {
			casResult = vhShort.compareAndSet(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchange */
		try {
			caeResult = (short)vhShort.compareAndExchange(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchangeAcquire */
		try {
			caeResult = (short)vhShort.compareAndExchangeAcquire(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* compareAndExchangeRelease */
		try {
			caeResult = (short)vhShort.compareAndExchangeRelease(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSet */
		try {
			casResult = vhShort.weakCompareAndSet(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetAcquire */
		try {
			casResult = vhShort.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetRelease */
		try {
			casResult = vhShort.weakCompareAndSetRelease(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* WeakCompareAndSetPlain */
		try {
			casResult = vhShort.weakCompareAndSetPlain(ByteArrayViewHelper.b, 0, (short)1, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSet */
		try {
			sFromVH = (short)vhShort.getAndSet(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSetAcquire */
		try {
			sFromVH = (short)vhShort.getAndSetAcquire(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndSetRelease */
		try {
			sFromVH = (short)vhShort.getAndSetRelease(ByteArrayViewHelper.b, 0, CHANGED_SHORT);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAdd */
		try {
			sFromVH = (short)vhShort.getAndAdd(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddAcquire */
		try {
			sFromVH = (short)vhShort.getAndAddAcquire(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndAddRelease */
		try {
			sFromVH = (short)vhShort.getAndAddRelease(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAnd */
		try {
			sFromVH = (short)vhShort.getAndBitwiseAnd(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndAcquire */
		try {
			sFromVH = (short)vhShort.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseAndRelease */
		try {
			sFromVH = (short)vhShort.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOr */
		try {
			sFromVH = (short)vhShort.getAndBitwiseOr(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrAcquire */
		try {
			sFromVH = (short)vhShort.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseOrRelease */
		try {
			sFromVH = (short)vhShort.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXor */
		try {
			sFromVH = (short)vhShort.getAndBitwiseXor(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorAcquire */
		try {
			sFromVH = (short)vhShort.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}

		/* GetAndBitwiseXorRelease */
		try {
			sFromVH = (short)vhShort.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 0, (short)2);
			Assert.fail("Expected UnsupportedOperationException");
		} catch (UnsupportedOperationException e) {}
	}

	/**
	 * Get and set the last element of a ByteArrayViewVarHandle viewed as short elements.
	 */
	@Test
	public void testShortLastElement() {
		int lastCompleteIndex = lastCompleteIndex(Short.BYTES);
		ByteArrayViewHelper.reset();
		/* Get */
		short fFromVH = (short)vhShort.get(ByteArrayViewHelper.b, lastCompleteIndex);
		Assert.assertEquals(LAST_SHORT, fFromVH);

		/* Set */
		vhShort.set(ByteArrayViewHelper.b, lastCompleteIndex, CHANGED_SHORT);
		checkUpdated2(lastCompleteIndex);
	}

	/**
	 * Attempts to get a ByteArrayViewVarHandle with byte[] as the view type.
	 */
	@Test(expectedExceptions = UnsupportedOperationException.class)
	public void testByte() {
		MethodHandles.byteArrayViewVarHandle(byte[].class, _byteOrder);
	}

	/**
	 * Attempts to get a ByteArrayViewVarHandle with Object[] as the view type.
	 */
	@Test(expectedExceptions = UnsupportedOperationException.class)
	public void testReference() {
		MethodHandles.byteArrayViewVarHandle(Object[].class, _byteOrder);
	}

	/**
	 * Attempts to get a ByteArrayViewVarHandle with boolean[] as the view type.
	 */
	@Test(expectedExceptions = UnsupportedOperationException.class)
	public void testBoolean() {
		MethodHandles.byteArrayViewVarHandle(boolean[].class, _byteOrder);
	}

	/**
	 * Attempt to get an element outside the bounds of the {@code byte[]}.
	 */
	@Test(expectedExceptions = IndexOutOfBoundsException.class)
	public void testInvalidIndex() {
		vhLong.get(ByteArrayViewHelper.b, ByteArrayViewHelper.b.length);
	}

	/**
	 * Attempt to get an element that crosses the end of the {@code byte[]}.
	 */
	@Test(expectedExceptions = IndexOutOfBoundsException.class)
	public void testIndexOverflow() {
		vhLong.get(ByteArrayViewHelper.b, lastCompleteIndex(Long.BYTES) + 1);
	}

	/**
	 * Cast the {@code byte[]} receiver object to {@link Object}.
	 */
	@Test
	public void testObjectReceiver() {
		vhInt.set((Object)ByteArrayViewHelper.b, 0, 2);
	}

	/**
	 * Pass an invalid type, cast to {@link Object}, as the {@code byte[]} receiver object.
	 */
	@Test(expectedExceptions = ClassCastException.class)
	public void testInvalidObjectReceiver() {
		vhInt.set((Object)"InvalidReceiver", 0, 2);
	}

	/**
	 * Invoke the ByteArrayViewVarHandle with an int[] receiver.
	 */
	@Test(expectedExceptions = ClassCastException.class)
	public void testIntArrayReceiver() {
		int[] array = null;
		array = ArrayHelper.reset(array);
		vhInt.set(array, 0, 2);
	}

	/**
	 * Invoke the ByteArrayViewVarHandle with a boolean[] receiver.
	 */
	@Test(expectedExceptions = ClassCastException.class)
	public void testBooleanArrayReceiver() {
		boolean[] array = null;
		array = ArrayHelper.reset(array);
		vhInt.set(array, 0, 2);
	}

	/**
	 * Perform all the operations with unaligned access.
	 * Use different indices to test whether crossing an 8-boundary matters.
	 */
	@Test
	public void testUnalignedAccess() {
		ByteArrayViewHelper.reset();
		/* Get */
		int iFromVH = (int)vhInt.get(ByteArrayViewHelper.b, 3);
		assertEquals(INITIAL_INT_AT_INDEX_3, iFromVH);

		/* Set */
		vhInt.set(ByteArrayViewHelper.b, 6, CHANGED_INT);
		checkUpdated4(6);

		try {
			iFromVH = (int) vhInt.getOpaque(ByteArrayViewHelper.b, 1);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.setOpaque(ByteArrayViewHelper.b, 2, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			iFromVH = (int) vhInt.getVolatile(ByteArrayViewHelper.b, 3);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.setVolatile(ByteArrayViewHelper.b, 5, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			iFromVH = (int) vhInt.getAcquire(ByteArrayViewHelper.b, 6);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.setRelease(ByteArrayViewHelper.b, 7, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.compareAndSet(ByteArrayViewHelper.b, 9, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.compareAndExchange(ByteArrayViewHelper.b, 10, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.compareAndExchangeAcquire(ByteArrayViewHelper.b, 11, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.compareAndExchangeRelease(ByteArrayViewHelper.b, 1, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.weakCompareAndSet(ByteArrayViewHelper.b, 2, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.weakCompareAndSetAcquire(ByteArrayViewHelper.b, 3, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.weakCompareAndSetRelease(ByteArrayViewHelper.b, 5, FIRST_INT, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndSet(ByteArrayViewHelper.b, 6, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndSetAcquire(ByteArrayViewHelper.b, 7, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndSetRelease(ByteArrayViewHelper.b, 9, CHANGED_INT);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndAdd(ByteArrayViewHelper.b, 10, 2);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndAddAcquire(ByteArrayViewHelper.b, 11, 2);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndAddRelease(ByteArrayViewHelper.b, 1, 2);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseAnd(ByteArrayViewHelper.b, 2, 7);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseAndAcquire(ByteArrayViewHelper.b, 3, 7);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseAndRelease(ByteArrayViewHelper.b, 5, 7);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseOr(ByteArrayViewHelper.b, 6, 1);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseOrAcquire(ByteArrayViewHelper.b, 7, 1);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseOrRelease(ByteArrayViewHelper.b, 9, 1);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseXor(ByteArrayViewHelper.b, 10, 6);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseXorAcquire(ByteArrayViewHelper.b, 11, 6);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}

		try {
			vhInt.getAndBitwiseXorRelease(ByteArrayViewHelper.b, 1, 6);
			failUnalignedAccess();
		} catch (IllegalStateException iae) {
			Assert.assertTrue(versionMajor < 23);
		} catch (UnsupportedOperationException ex) {
			Assert.assertTrue(versionMajor >= 23);
		}
	}
}
