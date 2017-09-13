/*[INCLUDE-IF Valhalla-MVT]*/
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package java.lang.invoke;

/**
 * MethodHandle subclass for constructing default value objects for Derived Value Types (DVT).
 */
final class DefaultValueHandle extends MethodHandle {
	private static final ThunkTable _thunkTable = new ThunkTable();
	
	/**
	 * Constructs a MethodHandle that, when invoked, will return a default value object for the specified value class.
	 * 
	 * @param type A MethodType who's return type is the value type we're generating default value objects for.
	 */
	DefaultValueHandle(Class<?> type) {
		super(MethodType.methodType(type), KIND_DEFAULTVALUE, null);
	}
	
	/*
	 * This is a single shared thunkTable across all ConstantHandle subclasses
	 * as each subclass only deals with a single signature.
	 */
	@Override
	protected final ThunkTable thunkTable() {
		return _thunkTable;
	}
}
