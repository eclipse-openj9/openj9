/*[INCLUDE-IF Panama]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
package jdk.internal.nicl;

import java.lang.invoke.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.nicl.*;
import java.nicl.metadata.NativeType;
import java.nicl.types.*;
import java.nicl.types.Pointer;
import java.lang.reflect.Type;

import com.ibm.oti.vm.VMLangInvokeAccess;

import jdk.internal.nicl.types.*;

class NativeInvoker {
	private static final PointerToken TOKEN = new PointerTokenImpl();

	private static final Lookup LOOKUP = MethodHandles.lookup();

	private final MethodType methodType;

	private final boolean isVarArgs;

	private final String debugMethodString;

	private final Type genericReturnType;

	private final LibrarySymbol symbol;

	private final String methodName;

	private static MethodHandle getPtrAddr;

	private static MethodHandle getPtrReturn;

	private static MethodHandle getStructAddr;

	private static MethodHandle getStructReturn;

	private String[] layoutStrings = null;

	static {
		try {
			getPtrAddr = LOOKUP.findStatic(NativeInvoker.class, "getPointerAddress", MethodType.methodType(long.class, Pointer.class)); //$NON-NLS-1$
			getPtrReturn = LOOKUP.findStatic(NativeInvoker.class, "getPointerReturn", MethodType.methodType(Pointer.class, Type.class, long.class)); //$NON-NLS-1$
			getStructAddr = LOOKUP.findStatic(NativeInvoker.class, "getStructAddress", MethodType.methodType(long.class, Reference.class)); //$NON-NLS-1$
			getStructReturn = LOOKUP.findStatic(NativeInvoker.class, "getStructReturn", MethodType.methodType(Reference.class, LayoutType.class, long.class)); //$NON-NLS-1$
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new RuntimeException(e);
		}
	}

	NativeInvoker(MethodType type, Boolean isVarArgs, SymbolLookup symbolLookup, String name, String debug, java.lang.reflect.Type returnType) throws NoSuchMethodException, IllegalAccessException{
		methodType = type;
		this.isVarArgs = isVarArgs;
		symbol = symbolLookup.lookup(name);
		debugMethodString = debug;
		genericReturnType = returnType;
		methodName = name;
	}

	NativeInvoker(MethodType type, Boolean isVarArgs, SymbolLookup symbolLookup, String name) throws NoSuchMethodException, IllegalAccessException {
		this(type, isVarArgs, symbolLookup, name, "<unknown>", null); //$NON-NLS-1$
	}

	MethodHandle getBoundMethodHandle() throws ClassCastException, Throwable {
		int len = methodType.parameterCount();

		Class<?> rtype = methodType.returnType();
		MethodHandle returnFilter = null;
		boolean isStructReturn = false;
		boolean isPointerReturn = false;
		if (Pointer.class.isAssignableFrom(rtype)) {
			rtype = long.class;
			returnFilter = MethodHandles.insertArguments(getPtrReturn, 0, genericReturnType);
			isPointerReturn = true;
		} else if (Util.isCStruct(rtype)) {
			NativeType nativeType = rtype.getAnnotation(java.nicl.metadata.NativeType.class);
			String retLayoutString = Util.sizeof(rtype) + nativeType.layout();
			addLayoutString(retLayoutString, 0);
			returnFilter = MethodHandles.insertArguments(getStructReturn, 0, NativeLibrary.createLayout(rtype));
			rtype = long.class;
			isStructReturn = true;
		}

		Class<?> ptypes[] = new Class<?>[len];
		MethodHandle filters[] = new MethodHandle[len];
		boolean isFilteredArg = false;
		for (int i=0; i<len; i++) {
			Class<?> ptype = methodType.parameterType(i);
			if (Pointer.class.isAssignableFrom(ptype)) {
				ptypes[i] = long.class;
				filters[i] = getPtrAddr;
				isFilteredArg = true;
			} else if (Util.isCStruct(ptype)) {
				ptypes[i] = long.class;
				filters[i] = getStructAddr;
				isFilteredArg = true;
				NativeType nativeType = ptype.getAnnotation(java.nicl.metadata.NativeType.class);
				String argLayoutString = Util.sizeof(ptype) + nativeType.layout();
				addLayoutString(argLayoutString, i+1);
			} else {
				ptypes[i] = ptype;
				filters[i] = null;
			}
		}

		MethodHandle mh;
		MethodType mt = MethodType.methodType(rtype, ptypes);
		VMLangInvokeAccess access = com.ibm.oti.vm.VM.getVMLangInvokeAccess();
		long nativeAddress = symbol.getAddress().addr(new PointerTokenImpl());
		if (null != returnFilter) {
			mh = MethodHandles.filterReturnValue(access.generateNativeMethodHandle(methodName, mt, nativeAddress, layoutStrings), returnFilter);
		} else {
			mh = access.generateNativeMethodHandle(methodName, mt, nativeAddress, layoutStrings);
		}

		if (isFilteredArg) {
			mh = MethodHandles.filterArguments(mh, 0, filters);
		}

		return mh;
	}
	
	private void addLayoutString(String str, int pos) {
		if (null == layoutStrings) {
			/** 
			 * layoutStrings[0] stores the layout string of the return argument of the method.
			 * The remaining layoutStrings array contains the layout string of the input arguments.
			 */
			layoutStrings = new String[methodType.parameterCount() + 1];
		}
		layoutStrings[pos] = str;
	}

	private static long getPointerAddress(Pointer<?> ptr) throws IllegalAccessException {
		return ptr.addr(TOKEN);
	}

	private static Pointer<?> getPointerReturn(Type returnType, long addr) {
		/* TODO This is pointing to native memory, need to think about how to free it */
		LayoutType<?> innerType = Util.createLayoutType(returnType).getInnerType();
		Pointer<?> p = RuntimeSupport.createPtr(addr, innerType);
		return p;
	}

	private static long getStructAddress(Reference<?> arg) throws IllegalAccessException {
		return arg.ptr().addr(TOKEN);
	}

	private static Reference<?> getStructReturn(LayoutType<? extends Reference<?>> returnLayoutType, long addr) {
		/* TODO This is pointing to j9mem_allocate_memory, need to think about how to free it */
		Pointer<?> ptr = RuntimeSupport.createPtr(addr, returnLayoutType);
		return (Reference<?>) ptr.deref();
	}
}

