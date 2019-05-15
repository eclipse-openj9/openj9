/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package java.lang.invoke;

abstract class ArgumentConversionHandle extends ConvertHandle {

	ArgumentConversionHandle(MethodHandle handle, MethodType type, byte kind) {
		super(handle, type, kind, handle.type());
		if (type == null) {
			throw new IllegalArgumentException();
		}
		checkConversion(handle.type, type);
	}

	// {{{ JIT support
	protected final ThunkTuple computeThunks(Object nextHandleType) {
		// To get the type casts right, we must inspect the full signatures of
		// both the receiver and nextHandleType, but we can upcast the return
		// type because ArgumentConversionHandles are not used to filter return types.
		MethodType thunkableReceiverType = ThunkKey.computeThunkableType(type(), 0);
		MethodType thunkableNextType     = ThunkKey.computeThunkableType((MethodType)nextHandleType, 0);
		return thunkTable().get(new ThunkKeyWithObject(thunkableReceiverType, thunkableNextType));
	}
	// }}} JIT support
}

