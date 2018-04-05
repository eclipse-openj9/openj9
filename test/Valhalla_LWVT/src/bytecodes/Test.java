/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package bytecodes;

public class Test {

	public static void main(String[] args) {
		testdefaultvalueAndwithfield();
		testdefaultvalueAbstractInterface();
		testnewValue();
		testdefaultvalueNotValue();
		testwithfieldNotValue();
		testwithfieldStatic();
		testwithfieldNonFinal();
		testwithfieldOutsideClass();
		testwithfieldNullPtr();
		testputfieldValue();
	}

	public static void testdefaultvalueAndwithfield() {
		Value a2 = Value.makeValue(4.0, null, 40);
		Value v = Value.makeValue(2.8, a2, 95);
		assert a2.theDouble == 4.0;
		assert a2.theObject == null;
		assert a2.theSingle == 40;
		assert v.theDouble == 2.8;
		assert v.theObject == a2;
		assert v.theSingle == 95;
	}

	public static void testdefaultvalueAbstractInterface() {
		try {
			Abstract.defaultvalue();
			assert false; /* UNREACHABLE */
		} catch (InstantiationError e) {}

		try {
			Interface.defaultvalue();
			assert false; /* UNREACHABLE */
		} catch (InstantiationError e) {}
	}

	public static void testnewValue() {
		try {
			Value v = new Value();
		} catch (InstantiationError e) {}
	}

	public static void testdefaultvalueNotValue() {
		try {
			NotValue nv = NotValue.defaultvalue();
			assert false; /* UNREACHABLE */
		} catch (Throwable e) {
			assert e instanceof IncompatibleClassChangeError;
		}
	}

	public static void testwithfieldNotValue() {
		NotValue nv = new NotValue();
		try {
			nv = nv.withfield();
			assert false; /* UNREACHABLE */
		} catch (Throwable e) {
			assert e instanceof IncompatibleClassChangeError;
		}
	}

	public static void testwithfieldStatic() {
		try {
			Value.witherStatic();
			assert false; /* UNREACHABLE */
		} catch (IncompatibleClassChangeError e) {}
	}

	public static void testwithfieldNonFinal() {
		try {
			AlmostValue av = AlmostValue.makeAlmostValue(0);
			assert false; /* UNREACHABLE */
		} catch (IllegalAccessError e) {}
	}

	public static void testwithfieldOutsideClass() {
		Value v = Value.makeValue(0.0, null, 0);
		try {
			v.theSingle = 40;
			assert false; /* UNREACHABLE */
		} catch (IllegalAccessError e) {}
	}

	public static void testwithfieldNullPtr() {
		Value v = null;
		try {
			v.theSingle = 40;
			assert false; /* UNREACHABLE */
		} catch (NullPointerException e) {}
	}

	public static void testputfieldValue() {
		Value v = Value.makeValue(0.0, null, 0);
		try {
			v.theSingle = 3;
			assert false; /* UNREACHABLE */
		} catch (IncompatibleClassChangeError e) {}
	}


}
