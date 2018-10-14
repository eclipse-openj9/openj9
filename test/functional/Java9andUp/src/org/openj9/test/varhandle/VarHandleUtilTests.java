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
import java.lang.invoke.VarHandle.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

@Test(groups = { "level.extended" })
public class VarHandleUtilTests {
	@Test
	public void testVarTypeInt() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
		Assert.assertEquals(int.class, vh.varType());
	}
	
	@Test
	public void testVarTypeRef() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", String.class);
		Assert.assertEquals(String.class, vh.varType());
	}
	
	@Test
	public void testAccessParameterTypesInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
		List<Class<?>> accessParameterTypes = vh.coordinateTypes();
		Assert.assertEquals(1, accessParameterTypes.size(), "Access parameter types should contain one element");
		Assert.assertEquals(InstanceHelper.class, accessParameterTypes.get(0), "Access parameter type at index 0 should be Helper");
	}
	
	@Test
	public void testAccessParameterTypesStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "i", int.class);
		List<Class<?>> accessParameterTypes = vh.coordinateTypes();
		Assert.assertEquals(0, accessParameterTypes.size(), "Access parameter types should contain zero element");
	}
	
	@Test
	public void testAccessParameterTypesArray() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(int[].class);
		List<Class<?>> accessParameterTypes = vh.coordinateTypes();
		Assert.assertEquals(2, accessParameterTypes.size(),"Access parameter types should contain two element");
		Assert.assertEquals(int[].class, accessParameterTypes.get(0), "Access parameter type at index 0 should be int[]");
		Assert.assertEquals(int.class, accessParameterTypes.get(1), "Access parameter type at index 1 should be int for index");
	}
	
	@Test
	public void testAccessModeTypeInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
		MethodType getMT = vh.accessModeType(AccessMode.GET);
		MethodType setMT = vh.accessModeType(AccessMode.SET);
		MethodType getVolatileMT = vh.accessModeType(AccessMode.GET_VOLATILE);
		MethodType setVolatileMT = vh.accessModeType(AccessMode.SET_VOLATILE);
		MethodType getOpaqueMT = vh.accessModeType(AccessMode.GET_OPAQUE);
		MethodType setOpaqueMT = vh.accessModeType(AccessMode.SET_OPAQUE);
		MethodType getAcquireMT = vh.accessModeType(AccessMode.GET_ACQUIRE);
		MethodType setReleaseMT = vh.accessModeType(AccessMode.SET_RELEASE);
		MethodType compareAndSetMT = vh.accessModeType(AccessMode.COMPARE_AND_SET);
		MethodType compareAndExchangeMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE);
		MethodType compareAndExchangeAcquireMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE);
		MethodType compareAndExchangeReleaseMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_RELEASE);
		MethodType weakCompareAndSetMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET);
		MethodType weakCompareAndSetAcquireMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE);
		MethodType weakCompareAndSetReleaseMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_RELEASE);
		MethodType weakCompareAndSetPlainMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_PLAIN);
		MethodType getAndSetMT = vh.accessModeType(AccessMode.GET_AND_SET);
		MethodType getAndSetAcquireMT = vh.accessModeType(AccessMode.GET_AND_SET_ACQUIRE);
		MethodType getAndSetReleaseMT = vh.accessModeType(AccessMode.GET_AND_SET_RELEASE);
		MethodType getAndAddMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddAcquireMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddReleaseMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndBitwiseAndMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND);
		MethodType getAndBitwiseAndAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_ACQUIRE);
		MethodType getAndBitwiseAndReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_RELEASE);
		MethodType getAndBitwiseOrMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR);
		MethodType getAndBitwiseOrAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_ACQUIRE);
		MethodType getAndBitwiseOrReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_RELEASE);
		MethodType getAndBitwiseXorMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR);
		MethodType getAndBitwiseXorAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE);
		MethodType getAndBitwiseXorReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_RELEASE);
		
		MethodType getter = MethodType.methodType(int.class, InstanceHelper.class);
		MethodType setter = MethodType.methodType(void.class, InstanceHelper.class, int.class);
		MethodType compareAndExchange = MethodType.methodType(int.class, InstanceHelper.class, int.class, int.class);
		MethodType compareAndSet = MethodType.methodType(boolean.class, InstanceHelper.class, int.class, int.class);
		MethodType getAndSet = MethodType.methodType(int.class, InstanceHelper.class, int.class);
		
		Assert.assertEquals(getter, getMT);
		Assert.assertEquals(setter, setMT);
		Assert.assertEquals(getter, getVolatileMT);
		Assert.assertEquals(setter, setVolatileMT);
		Assert.assertEquals(getter, getOpaqueMT);
		Assert.assertEquals(setter, setOpaqueMT);
		Assert.assertEquals(getter, getAcquireMT);
		Assert.assertEquals(setter, setReleaseMT);
		Assert.assertEquals(compareAndSet, compareAndSetMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeAcquireMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetAcquireMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetPlainMT);
		Assert.assertEquals(getAndSet, getAndSetMT);
		Assert.assertEquals(getAndSet, getAndSetAcquireMT);
		Assert.assertEquals(getAndSet, getAndSetReleaseMT);
		Assert.assertEquals(getAndSet, getAndAddMT);
		Assert.assertEquals(getAndSet, getAndAddAcquireMT);
		Assert.assertEquals(getAndSet, getAndAddReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorReleaseMT);
	}
	
	@Test
	public void testAccessModeTypeStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "i", int.class);
		MethodType getMT = vh.accessModeType(AccessMode.GET);
		MethodType setMT = vh.accessModeType(AccessMode.SET);
		MethodType getVolatileMT = vh.accessModeType(AccessMode.GET_VOLATILE);
		MethodType setVolatileMT = vh.accessModeType(AccessMode.SET_VOLATILE);
		MethodType getOpaqueMT = vh.accessModeType(AccessMode.GET_OPAQUE);
		MethodType setOpaqueMT = vh.accessModeType(AccessMode.SET_OPAQUE);
		MethodType getAcquireMT = vh.accessModeType(AccessMode.GET_ACQUIRE);
		MethodType setReleaseMT = vh.accessModeType(AccessMode.SET_RELEASE);
		MethodType compareAndSetMT = vh.accessModeType(AccessMode.COMPARE_AND_SET);
		MethodType compareAndExchangeMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE);
		MethodType compareAndExchangeAcquireMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE);
		MethodType compareAndExchangeReleaseMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_RELEASE);
		MethodType weakCompareAndSetMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET);
		MethodType weakCompareAndSetAcquireMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE);
		MethodType weakCompareAndSetReleaseMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_RELEASE);
		MethodType weakCompareAndSetPlainMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_PLAIN);
		MethodType getAndSetMT = vh.accessModeType(AccessMode.GET_AND_SET);
		MethodType getAndSetAcquireMT = vh.accessModeType(AccessMode.GET_AND_SET_ACQUIRE);
		MethodType getAndSetReleaseMT = vh.accessModeType(AccessMode.GET_AND_SET_RELEASE);
		MethodType getAndAddMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddAcquireMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddReleaseMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndBitwiseAndMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND);
		MethodType getAndBitwiseAndAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_ACQUIRE);
		MethodType getAndBitwiseAndReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_RELEASE);
		MethodType getAndBitwiseOrMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR);
		MethodType getAndBitwiseOrAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_ACQUIRE);
		MethodType getAndBitwiseOrReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_RELEASE);
		MethodType getAndBitwiseXorMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR);
		MethodType getAndBitwiseXorAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE);
		MethodType getAndBitwiseXorReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_RELEASE);
		
		MethodType getter = MethodType.methodType(int.class);
		MethodType setter = MethodType.methodType(void.class, int.class);
		MethodType compareAndExchange = MethodType.methodType(int.class, int.class, int.class);
		MethodType compareAndSet = MethodType.methodType(boolean.class, int.class, int.class);
		MethodType getAndSet = MethodType.methodType(int.class, int.class);
		
		Assert.assertEquals(getter, getMT);
		Assert.assertEquals(setter, setMT);
		Assert.assertEquals(getter, getVolatileMT);
		Assert.assertEquals(setter, setVolatileMT);
		Assert.assertEquals(getter, getOpaqueMT);
		Assert.assertEquals(setter, setOpaqueMT);
		Assert.assertEquals(getter, getAcquireMT);
		Assert.assertEquals(setter, setReleaseMT);
		Assert.assertEquals(compareAndSet, compareAndSetMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeAcquireMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetAcquireMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetPlainMT);
		Assert.assertEquals(getAndSet, getAndSetMT);
		Assert.assertEquals(getAndSet, getAndSetAcquireMT);
		Assert.assertEquals(getAndSet, getAndSetReleaseMT);
		Assert.assertEquals(getAndSet, getAndAddMT);
		Assert.assertEquals(getAndSet, getAndAddAcquireMT);
		Assert.assertEquals(getAndSet, getAndAddReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorReleaseMT);
	}
	
	@Test
	public void testAccessModeTypeArray() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(int[].class);
		MethodType getMT = vh.accessModeType(AccessMode.GET);
		MethodType setMT = vh.accessModeType(AccessMode.SET);
		MethodType getVolatileMT = vh.accessModeType(AccessMode.GET_VOLATILE);
		MethodType setVolatileMT = vh.accessModeType(AccessMode.SET_VOLATILE);
		MethodType getOpaqueMT = vh.accessModeType(AccessMode.GET_OPAQUE);
		MethodType setOpaqueMT = vh.accessModeType(AccessMode.SET_OPAQUE);
		MethodType getAcquireMT = vh.accessModeType(AccessMode.GET_ACQUIRE);
		MethodType setReleaseMT = vh.accessModeType(AccessMode.SET_RELEASE);
		MethodType compareAndSetMT = vh.accessModeType(AccessMode.COMPARE_AND_SET);
		MethodType compareAndExchangeMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE);
		MethodType compareAndExchangeAcquireMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE);
		MethodType compareAndExchangeReleaseMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_RELEASE);
		MethodType weakCompareAndSetMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET);
		MethodType weakCompareAndSetAcquireMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE);
		MethodType weakCompareAndSetReleaseMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_RELEASE);
		MethodType weakCompareAndSetPlainMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_PLAIN);
		MethodType getAndSetMT = vh.accessModeType(AccessMode.GET_AND_SET);
		MethodType getAndSetAcquireMT = vh.accessModeType(AccessMode.GET_AND_SET_ACQUIRE);
		MethodType getAndSetReleaseMT = vh.accessModeType(AccessMode.GET_AND_SET_RELEASE);
		MethodType getAndAddMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddAcquireMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddReleaseMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndBitwiseAndMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND);
		MethodType getAndBitwiseAndAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_ACQUIRE);
		MethodType getAndBitwiseAndReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_RELEASE);
		MethodType getAndBitwiseOrMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR);
		MethodType getAndBitwiseOrAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_ACQUIRE);
		MethodType getAndBitwiseOrReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_RELEASE);
		MethodType getAndBitwiseXorMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR);
		MethodType getAndBitwiseXorAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE);
		MethodType getAndBitwiseXorReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_RELEASE);
		
		MethodType getter = MethodType.methodType(int.class, int[].class, int.class);
		MethodType setter = MethodType.methodType(void.class, int[].class, int.class, int.class);
		MethodType compareAndExchange = MethodType.methodType(int.class, int[].class, int.class, int.class, int.class);
		MethodType compareAndSet = MethodType.methodType(boolean.class, int[].class, int.class, int.class, int.class);
		MethodType getAndSet = MethodType.methodType(int.class, int[].class, int.class, int.class);
		
		Assert.assertEquals(getter, getMT);
		Assert.assertEquals(setter, setMT);
		Assert.assertEquals(getter, getVolatileMT);
		Assert.assertEquals(setter, setVolatileMT);
		Assert.assertEquals(getter, getOpaqueMT);
		Assert.assertEquals(setter, setOpaqueMT);
		Assert.assertEquals(getter, getAcquireMT);
		Assert.assertEquals(setter, setReleaseMT);
		Assert.assertEquals(compareAndSet, compareAndSetMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeAcquireMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetAcquireMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetPlainMT);
		Assert.assertEquals(getAndSet, getAndSetMT);
		Assert.assertEquals(getAndSet, getAndSetAcquireMT);
		Assert.assertEquals(getAndSet, getAndSetReleaseMT);
		Assert.assertEquals(getAndSet, getAndAddMT);
		Assert.assertEquals(getAndSet, getAndAddAcquireMT);
		Assert.assertEquals(getAndSet, getAndAddReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorReleaseMT);
	}
	
	@Test
	public void testAccessModeTypeByteArray() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
		MethodType getMT = vh.accessModeType(AccessMode.GET);
		MethodType setMT = vh.accessModeType(AccessMode.SET);
		MethodType getVolatileMT = vh.accessModeType(AccessMode.GET_VOLATILE);
		MethodType setVolatileMT = vh.accessModeType(AccessMode.SET_VOLATILE);
		MethodType getOpaqueMT = vh.accessModeType(AccessMode.GET_OPAQUE);
		MethodType setOpaqueMT = vh.accessModeType(AccessMode.SET_OPAQUE);
		MethodType getAcquireMT = vh.accessModeType(AccessMode.GET_ACQUIRE);
		MethodType setReleaseMT = vh.accessModeType(AccessMode.SET_RELEASE);
		MethodType compareAndSetMT = vh.accessModeType(AccessMode.COMPARE_AND_SET);
		MethodType compareAndExchangeMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE);
		MethodType compareAndExchangeAcquireMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE);
		MethodType compareAndExchangeReleaseMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_RELEASE);
		MethodType weakCompareAndSetMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET);
		MethodType weakCompareAndSetAcquireMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE);
		MethodType weakCompareAndSetReleaseMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_RELEASE);
		MethodType weakCompareAndSetPlainMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_PLAIN);
		MethodType getAndSetMT = vh.accessModeType(AccessMode.GET_AND_SET);
		MethodType getAndSetAcquireMT = vh.accessModeType(AccessMode.GET_AND_SET_ACQUIRE);
		MethodType getAndSetReleaseMT = vh.accessModeType(AccessMode.GET_AND_SET_RELEASE);
		MethodType getAndAddMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddAcquireMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddReleaseMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndBitwiseAndMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND);
		MethodType getAndBitwiseAndAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_ACQUIRE);
		MethodType getAndBitwiseAndReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_RELEASE);
		MethodType getAndBitwiseOrMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR);
		MethodType getAndBitwiseOrAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_ACQUIRE);
		MethodType getAndBitwiseOrReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_RELEASE);
		MethodType getAndBitwiseXorMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR);
		MethodType getAndBitwiseXorAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE);
		MethodType getAndBitwiseXorReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_RELEASE);
		
		MethodType getter = MethodType.methodType(int.class, byte[].class, int.class);
		MethodType setter = MethodType.methodType(void.class, byte[].class, int.class, int.class);
		MethodType compareAndExchange = MethodType.methodType(int.class, byte[].class, int.class, int.class, int.class);
		MethodType compareAndSet = MethodType.methodType(boolean.class, byte[].class, int.class, int.class, int.class);
		MethodType getAndSet = MethodType.methodType(int.class, byte[].class, int.class, int.class);
		
		Assert.assertEquals(getter, getMT);
		Assert.assertEquals(setter, setMT);
		Assert.assertEquals(getter, getVolatileMT);
		Assert.assertEquals(setter, setVolatileMT);
		Assert.assertEquals(getter, getOpaqueMT);
		Assert.assertEquals(setter, setOpaqueMT);
		Assert.assertEquals(getter, getAcquireMT);
		Assert.assertEquals(setter, setReleaseMT);
		Assert.assertEquals(compareAndSet, compareAndSetMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeAcquireMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetAcquireMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetPlainMT);
		Assert.assertEquals(getAndSet, getAndSetMT);
		Assert.assertEquals(getAndSet, getAndSetAcquireMT);
		Assert.assertEquals(getAndSet, getAndSetReleaseMT);
		Assert.assertEquals(getAndSet, getAndAddMT);
		Assert.assertEquals(getAndSet, getAndAddAcquireMT);
		Assert.assertEquals(getAndSet, getAndAddReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorReleaseMT);
	}
	
	@Test
	public void testAccessModeTypeByteBuffer() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
		MethodType getMT = vh.accessModeType(AccessMode.GET);
		MethodType setMT = vh.accessModeType(AccessMode.SET);
		MethodType getVolatileMT = vh.accessModeType(AccessMode.GET_VOLATILE);
		MethodType setVolatileMT = vh.accessModeType(AccessMode.SET_VOLATILE);
		MethodType getOpaqueMT = vh.accessModeType(AccessMode.GET_OPAQUE);
		MethodType setOpaqueMT = vh.accessModeType(AccessMode.SET_OPAQUE);
		MethodType getAcquireMT = vh.accessModeType(AccessMode.GET_ACQUIRE);
		MethodType setReleaseMT = vh.accessModeType(AccessMode.SET_RELEASE);
		MethodType compareAndSetMT = vh.accessModeType(AccessMode.COMPARE_AND_SET);
		MethodType compareAndExchangeMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE);
		MethodType compareAndExchangeAcquireMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE);
		MethodType compareAndExchangeReleaseMT = vh.accessModeType(AccessMode.COMPARE_AND_EXCHANGE_RELEASE);
		MethodType weakCompareAndSetMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET);
		MethodType weakCompareAndSetAcquireMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE);
		MethodType weakCompareAndSetReleaseMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_RELEASE);
		MethodType weakCompareAndSetPlainMT = vh.accessModeType(AccessMode.WEAK_COMPARE_AND_SET_PLAIN);
		MethodType getAndSetMT = vh.accessModeType(AccessMode.GET_AND_SET);
		MethodType getAndSetAcquireMT = vh.accessModeType(AccessMode.GET_AND_SET_ACQUIRE);
		MethodType getAndSetReleaseMT = vh.accessModeType(AccessMode.GET_AND_SET_RELEASE);
		MethodType getAndAddMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddAcquireMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndAddReleaseMT = vh.accessModeType(AccessMode.GET_AND_ADD);
		MethodType getAndBitwiseAndMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND);
		MethodType getAndBitwiseAndAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_ACQUIRE);
		MethodType getAndBitwiseAndReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_AND_RELEASE);
		MethodType getAndBitwiseOrMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR);
		MethodType getAndBitwiseOrAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_ACQUIRE);
		MethodType getAndBitwiseOrReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_OR_RELEASE);
		MethodType getAndBitwiseXorMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR);
		MethodType getAndBitwiseXorAcquireMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE);
		MethodType getAndBitwiseXorReleaseMT = vh.accessModeType(AccessMode.GET_AND_BITWISE_XOR_RELEASE);
		
		MethodType getter = MethodType.methodType(int.class, ByteBuffer.class, int.class);
		MethodType setter = MethodType.methodType(void.class, ByteBuffer.class, int.class, int.class);
		MethodType compareAndExchange = MethodType.methodType(int.class, ByteBuffer.class, int.class, int.class, int.class);
		MethodType compareAndSet = MethodType.methodType(boolean.class, ByteBuffer.class, int.class, int.class, int.class);
		MethodType getAndSet = MethodType.methodType(int.class, ByteBuffer.class, int.class, int.class);
		
		Assert.assertEquals(getter, getMT);
		Assert.assertEquals(setter, setMT);
		Assert.assertEquals(getter, getVolatileMT);
		Assert.assertEquals(setter, setVolatileMT);
		Assert.assertEquals(getter, getOpaqueMT);
		Assert.assertEquals(setter, setOpaqueMT);
		Assert.assertEquals(getter, getAcquireMT);
		Assert.assertEquals(setter, setReleaseMT);
		Assert.assertEquals(compareAndSet, compareAndSetMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeAcquireMT);
		Assert.assertEquals(compareAndExchange, compareAndExchangeReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetAcquireMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetReleaseMT);
		Assert.assertEquals(compareAndSet, weakCompareAndSetPlainMT);
		Assert.assertEquals(getAndSet, getAndSetMT);
		Assert.assertEquals(getAndSet, getAndSetAcquireMT);
		Assert.assertEquals(getAndSet, getAndSetReleaseMT);
		Assert.assertEquals(getAndSet, getAndAddMT);
		Assert.assertEquals(getAndSet, getAndAddAcquireMT);
		Assert.assertEquals(getAndSet, getAndAddReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseAndReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseOrReleaseMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorAcquireMT);
		Assert.assertEquals(getAndSet, getAndBitwiseXorReleaseMT);
	}
	
	private static void isAccessModeSupported_AllSupported(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	private static void isAccessModeSupported_Ref(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	private static void isAccessModeSupported_Boolean(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	private static void isAccessModeSupported_FloatingPoint(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}

	private static void isAccessModeSupported_FloatingPointView(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	private static void isAccessModeSupported_ByteView(VarHandle vh) {
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	@Test
	public void testIsAccessModeSupportedFinalField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "finalI", int.class);
		
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.SET));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_VOLATILE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.SET_VOLATILE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_OPAQUE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.SET_OPAQUE));
		Assert.assertTrue(vh.isAccessModeSupported(AccessMode.GET_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.COMPARE_AND_EXCHANGE_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.WEAK_COMPARE_AND_SET_PLAIN));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_SET_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_ADD_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_AND_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_OR_RELEASE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_ACQUIRE));
		Assert.assertFalse(vh.isAccessModeSupported(AccessMode.GET_AND_BITWISE_XOR_RELEASE));
	}
	
	@Test
	public void testIsAccessModeSupportedByteInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "b", byte.class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedByteStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "b", byte.class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedByteArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(byte[].class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedCharInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "c", char.class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedCharStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "c", char.class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedCharArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(char[].class);
		isAccessModeSupported_AllSupported(vh);
	}

	@Test
	public void testIsAccessModeSupportedCharByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(char[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedCharByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(char[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedCharByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(char[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedCharByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(char[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedDoubleInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "d", double.class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "d", double.class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(double[].class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(double[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(double[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(double[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedDoubleByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(double[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "f", float.class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "f", float.class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(float[].class);
		isAccessModeSupported_FloatingPoint(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(float[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}

	@Test
	public void testIsAccessModeSupportedFloatByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_FloatingPointView(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedIntInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "i", int.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(int[].class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedIntByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}


	@Test
	public void testIsAccessModeSupportedLongInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "j", long.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedLongStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "j", long.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedLongArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(long[].class);
		isAccessModeSupported_AllSupported(vh);
	}


	@Test
	public void testIsAccessModeSupportedLongByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(long[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedLongByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedLongByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(long[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedLongByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_AllSupported(vh);
	}
	
	@Test
	public void testIsAccessModeSupportedRefInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", String.class);
		isAccessModeSupported_Ref(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedRefStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "l1", String.class);
		isAccessModeSupported_Ref(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedRefArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(String[].class);
		isAccessModeSupported_Ref(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "s", short.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "s", short.class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(short[].class);
		isAccessModeSupported_AllSupported(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortByteArrayViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(short[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortByteArrayViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteArrayViewVarHandle(short[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortByteBufferViewBE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(short[].class, ByteOrder.BIG_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedShortByteBufferViewLE() throws Throwable {
		VarHandle vh = MethodHandles.byteBufferViewVarHandle(short[].class, ByteOrder.LITTLE_ENDIAN);
		isAccessModeSupported_ByteView(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedBooleanInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "z", boolean.class);
		isAccessModeSupported_Boolean(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedBooleanStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "z", boolean.class);
		isAccessModeSupported_Boolean(vh);
	}
	

	@Test
	public void testIsAccessModeSupportedBooleanArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(boolean[].class);
		isAccessModeSupported_Boolean(vh);
	}
	
	@Test
	public void testToMethodHandleInstanceField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "i", int.class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.GET);
		InstanceHelper h = new InstanceHelper(1);
		
		int i = (int)mh.invoke(h);
		Assert.assertEquals(h.i, i);
	}
	
	@Test
	public void testToMethodHandleStaticField() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "i", int.class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.GET);
		StaticHelper.reset();
		
		int i = (int)mh.invoke();
		Assert.assertEquals(StaticHelper.i, i);
	}
	
	@Test
	public void testToMethodHandleArrayElement() throws Throwable {
		VarHandle vh = MethodHandles.arrayElementVarHandle(int[].class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.GET);
		int[] array = null;
		array = ArrayHelper.reset(array);
		
		int i = (int)mh.invoke(array, 0);
		Assert.assertEquals(array[0], i);
	}
	
	@Test
	public void testToMethodHandleFinalFieldGetter() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "finalI", int.class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.GET);
		InstanceHelper h = new InstanceHelper();
		int i = (int)mh.invoke(h);
		Assert.assertEquals(h.finalI, i);
	}

	@Test(expectedExceptions=UnsupportedOperationException.class)
	public void testToMethodHandleFinalFieldSetter() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "finalI", int.class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.SET);
		mh.invoke(new InstanceHelper(), 2);
	}
	
	@Test(expectedExceptions=UnsupportedOperationException.class)
	public void testToMethodHandleUnsupportedType() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "z", boolean.class);
		MethodHandle mh = vh.toMethodHandle(AccessMode.GET_AND_ADD);
		InstanceHelper h = new InstanceHelper(false);
		mh.invoke(h, true);
	}
}
