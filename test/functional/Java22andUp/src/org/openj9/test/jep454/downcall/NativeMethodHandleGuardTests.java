/*
 * Copyright IBM Corp. and others 2026
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
package org.openj9.test.jep454.downcall;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicReference;
import static java.lang.foreign.ValueLayout.*;
import org.testng.Assert;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;

/* Run with -Xffiproto and the --add-opens options in playlist.xml. */
@Test(groups = { "level.sanity" })
public class NativeMethodHandleGuardTests {
	static {
		System.loadLibrary("clinkerffitests");
	}

	private static MethodHandle pointerCall(Linker.Option... options) {
		MemorySegment symbol = SymbolLookup.loaderLookup()
			.find("addIntAndIntFromPointer")
			.orElseThrow();
		return Linker.nativeLinker().downcallHandle(
			symbol,
			FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS),
			options);
	}

	@Test
	public void guardSelectsExactlyOneBranch() throws Throwable {
		Method guard = Class.forName("openj9.internal.foreign.abi.InternalDowncallHandler")
			.getDeclaredMethod(
				"guardNativeSegments",
				MethodHandle.class,
				MethodHandle.class);
		guard.setAccessible(true);
		MethodType type = MethodType.methodType(
			int.class,
			MemorySegment.class,
			SegmentAllocator.class,
			long.class,
			MemorySegment.class,
			MemorySegment.class);
		MethodHandle nativeBranch = MethodHandles.dropArguments(
			MethodHandles.constant(int.class, 1),
			0,
			type.parameterList());
		MethodHandle fallback = MethodHandles.dropArguments(
			MethodHandles.constant(int.class, 2),
			0,
			type.parameterList());
		MethodHandle guarded = (MethodHandle)guard.invoke(null, nativeBranch, fallback);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment nativeSegment = arena.allocate(8);
			MemorySegment heapSegment = MemorySegment.ofArray(new byte[8]);
			Object[] arguments = { nativeSegment, arena, 42L, nativeSegment, nativeSegment };
			Assert.assertEquals((int)guarded.invokeWithArguments(arguments), 1);

			for (int index : new int[] { 0, 3, 4 }) {
				for (MemorySegment unsupported : new MemorySegment[] { heapSegment, null }) {
					arguments[index] = unsupported;
					Assert.assertEquals(
						(int)guarded.invokeWithArguments(arguments),
						2,
						"Guard missed segment argument " + index);
				}
				arguments[index] = nativeSegment;
			}

			MethodHandle bound = MethodHandles.insertArguments(guarded, 0, nativeSegment);
			Assert.assertEquals(
				(int)bound.invokeExact(
					(SegmentAllocator)arena,
					42L,
					nativeSegment,
					MemorySegment.NULL),
				1);
			Assert.assertEquals(
				(int)bound.invokeExact((SegmentAllocator)arena, 42L, heapSegment, nativeSegment),
				2);
		}
	}

	@DataProvider(name = "scalarDowncalls")
	public static Object[][] scalarDowncalls() {
		return new Object[][] {
			{ "add2BoolsWithOr", JAVA_BOOLEAN, true, false, true },
			{ "add2Bytes", JAVA_BYTE, (byte)6, (byte)3, (byte)9 },
			{ "add2Bytes", JAVA_BYTE, (byte)-6, (byte)-3, (byte)-9 },
			{ "createNewCharFrom2Chars", JAVA_CHAR, 'B', 'D', 'C' },
			{ "add2Shorts", JAVA_SHORT, (short)24, (short)32, (short)56 },
			{ "add2Shorts", JAVA_SHORT, (short)-24, (short)-32, (short)-56 },
			{ "add2Ints", JAVA_INT, 112, 123, 235 },
			{ "add2Longs", JAVA_LONG, 57424L, 698235L, 755659L },
			{ "add2Floats", JAVA_FLOAT, 1.25f, 2.5f, 3.75f },
			{ "add2Doubles", JAVA_DOUBLE, 1.25d, 2.5d, 3.75d }
		};
	}

	@Test(dataProvider = "scalarDowncalls")
	public void downcallsAcceptAllScalarCarriers(
		String name,
		ValueLayout layout,
		Object first,
		Object second,
		Object expected) throws Throwable {
		Linker linker = Linker.nativeLinker();
		MemorySegment symbol = SymbolLookup.loaderLookup().find(name).orElseThrow();
		FunctionDescriptor descriptor = FunctionDescriptor.of(layout, layout, layout);
		MethodHandle bound = linker.downcallHandle(symbol, descriptor);
		MethodHandle unbound = linker.downcallHandle(descriptor);
		Assert.assertEquals(bound.type(), descriptor.toMethodType());
		Assert.assertEquals(
			unbound.type(),
			descriptor.toMethodType().insertParameterTypes(0, MemorySegment.class));
		Assert.assertEquals(bound.invokeWithArguments(first, second), expected);
		Assert.assertEquals(unbound.invokeWithArguments(symbol, first, second), expected);
	}

	@Test
	public void voidAndMixedCarriersRemainSupported() throws Throwable {
		Linker linker = Linker.nativeLinker();
		SymbolLookup symbols = SymbolLookup.loaderLookup();
		MemorySegment voidSymbol = symbols.find("add2IntsReturnVoid").orElseThrow();
		FunctionDescriptor voidDescriptor = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		linker.downcallHandle(voidSymbol, voidDescriptor).invokeExact(454, 398);
		linker.downcallHandle(voidDescriptor).invokeExact(voidSymbol, 454, 398);

		MemorySegment mixedSymbol = symbols.find("addIntAndChar").orElseThrow();
		FunctionDescriptor mixedDescriptor = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR);
		int boundResult = (int)linker.downcallHandle(mixedSymbol, mixedDescriptor)
			.invokeExact(
				58,
				'A');
		int unboundResult = (int)linker.downcallHandle(mixedDescriptor)
			.invokeExact(mixedSymbol, 58, 'A');
		Assert.assertEquals(boundResult, 123);
		Assert.assertEquals(unboundResult, 123);
	}

	@Test
	public void carrierEligibilityRetainsFallback() throws Throwable {
		for (ValueLayout layout : new ValueLayout[] {
			JAVA_INT,
			JAVA_LONG,
			JAVA_FLOAT,
			JAVA_DOUBLE
		}) {
			assertEligibility(FunctionDescriptor.of(layout, layout), true);
			assertEligibility(FunctionDescriptor.of(layout, ADDRESS), true);
		}
		assertEligibility(FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS), true);

		for (ValueLayout layout : new ValueLayout[] {
			JAVA_BOOLEAN,
			JAVA_BYTE,
			JAVA_CHAR,
			JAVA_SHORT
		}) {
			assertEligibility(FunctionDescriptor.of(layout, JAVA_INT), false);
			assertEligibility(FunctionDescriptor.of(JAVA_INT, JAVA_INT, layout), false);
			assertEligibility(FunctionDescriptor.ofVoid(layout), false);
		}
		assertEligibility(FunctionDescriptor.of(ADDRESS, ADDRESS), false);
		assertEligibility(
			FunctionDescriptor.of(ADDRESS.withTargetLayout(JAVA_BYTE), ADDRESS, JAVA_INT),
			false);
	}

	@Test(expectedExceptions = IllegalArgumentException.class)
	public void ordinaryHeapPointerRetainsValidation() throws Throwable {
		int ignored = (int)pointerCall().invokeExact(5, MemorySegment.ofArray(new int[] { 7 }));
	}

	@Test
	public void unsupportedLayoutsAndOptionsUseFallback() throws Throwable {
		FunctionDescriptor scalar = FunctionDescriptor.of(JAVA_INT, JAVA_INT);
		assertEligibility(scalar, true);
		assertEligibility(FunctionDescriptor.of(JAVA_INT, ADDRESS), true);
		assertEligibility(scalar, false, Linker.Option.firstVariadicArg(0));

		// Even with no actual variadic arguments, this is a variadic call.
		assertEligibility(scalar, false, Linker.Option.firstVariadicArg(1));
		assertEligibility(scalar, false, Linker.Option.critical(false));
		assertEligibility(scalar, false, Linker.Option.critical(true));
		assertEligibility(scalar, false, Linker.Option.captureCallState("errno"));

		MemoryLayout struct = MemoryLayout.structLayout(JAVA_INT, JAVA_INT);
		assertEligibility(FunctionDescriptor.of(JAVA_INT, struct), false);
		assertEligibility(FunctionDescriptor.of(struct, JAVA_INT), false);
	}

	private static void assertEligibility(
		FunctionDescriptor descriptor,
		boolean expected,
		Linker.Option... options) throws Throwable {
		Class<?> optionsClass = Class.forName("jdk.internal.foreign.abi.LinkerOptions");
		Method factory = optionsClass.getMethod(
			"forDowncall",
			FunctionDescriptor.class,
			Linker.Option[].class);
		factory.setAccessible(true);
		Object linkerOptions = factory.invoke(null, descriptor, options);
		Class<?> handlerClass = Class.forName(
			"openj9.internal.foreign.abi.InternalDowncallHandler");
		Object handler = handlerClass.getConstructor(
			MethodType.class,
			FunctionDescriptor.class,
			optionsClass)
			.newInstance(descriptor.toMethodType(), descriptor, linkerOptions);
		Method eligible = handlerClass.getDeclaredMethod("isSimpleNativeCall");
		eligible.setAccessible(true);
		Assert.assertEquals((boolean)eligible.invoke(handler), expected, descriptor.toString());
	}

	@Test
	public void criticalHeapPointerRetainsFallback() throws Throwable {
		int result = (int)pointerCall(Linker.Option.critical(true))
			.invokeExact(5, MemorySegment.ofArray(new int[] { 7 }));
		Assert.assertEquals(result, 12);
	}

	@Test(expectedExceptions = IllegalStateException.class)
	public void closedNativePointerRetainsValidation() throws Throwable {
		MemorySegment pointer;
		try (Arena arena = Arena.ofConfined()) {
			pointer = arena.allocateFrom(JAVA_INT, 7);
		}
		int ignored = (int)pointerCall().invokeExact(5, pointer);
	}

	@Test
	public void confinedNativePointerRetainsThreadCheck() throws Throwable {
		try (Arena arena = Arena.ofConfined()) {
			MemorySegment pointer = arena.allocateFrom(JAVA_INT, 7);
			MethodHandle call = pointerCall();
			AtomicReference<Throwable> failure = new AtomicReference<>();
			Thread thread = new Thread(() -> {
				try {
					int ignored = (int)call.invokeExact(5, pointer);
				} catch (Throwable t) {
					failure.set(t);
				}
			});
			thread.start();
			thread.join(30_000);
			Assert.assertFalse(thread.isAlive(), "Downcall did not finish");
			Assert.assertTrue(
				failure.get() instanceof WrongThreadException,
				"Expected WrongThreadException, got " + failure.get());
		}
	}

	@Test
	public void pointerReturnRetainsSegmentConversion() throws Throwable {
		Linker linker = Linker.nativeLinker();
		MemorySegment symbol = linker.defaultLookup().find("strchr").orElseThrow();
		FunctionDescriptor descriptor = FunctionDescriptor.of(
			ADDRESS.withTargetLayout(JAVA_BYTE),
			ADDRESS,
			JAVA_INT);
		MethodHandle bound = linker.downcallHandle(symbol, descriptor);
		MethodHandle unbound = linker.downcallHandle(descriptor);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment input = arena.allocateFrom("abc");
			for (MethodHandle strchr : new MethodHandle[] { bound, unbound.bindTo(symbol) }) {
				MemorySegment result = (MemorySegment)strchr.invokeExact(input, (int)'b');
				Assert.assertEquals(result.address(), input.address() + 1);
				Assert.assertEquals(result.byteSize(), 1L);
				Assert.assertEquals(result.get(JAVA_BYTE, 0), (byte)'b');
				MemorySegment absent = (MemorySegment)strchr.invokeExact(input, (int)'z');
				Assert.assertEquals(absent.address(), 0L);
			}
		}
	}
}
