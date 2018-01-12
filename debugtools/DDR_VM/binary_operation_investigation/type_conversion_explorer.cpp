/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

/* Application for experimentally determining type conversions from
 * binary operations.
 *
 * Andrew Hall
 */

#include <iostream>

#include "arith_template.hpp"

#include "j9_types.hpp"

using namespace std; 


static void printBasicTypes(void)
{
	cout << "Type codes:" << endl;
	cout << "U_8: " << typeid(U_8).name() << endl;
	cout << "U_16: " << typeid(U_16).name() << endl;
	cout << "U_32: " << typeid(U_32).name() << endl;
	cout << "U_64: " << typeid(U_64).name() << endl;
	cout << "I_8: " << typeid(I_8).name() << endl;
	cout << "I_16: " << typeid(I_16).name() << endl;
	cout << "I_32: " << typeid(I_32).name() << endl;
	cout << "I_64: " << typeid(I_64).name() << endl;
	cout << "int: " << typeid(int).name() << endl;
	cout << "unsigned int: " << typeid(unsigned int).name() << endl;
	cout << "U_8*: " << typeid(U_8 *).name() << endl;
	cout << "U_16*: " << typeid(U_16 *).name() << endl;
	cout << "U_32*: " << typeid(U_32 *).name() << endl;
	cout << "U_64*: " << typeid(U_64 *).name() << endl;
	cout << "I_8*: " << typeid(I_8 *).name() << endl;
	cout << "I_16*: " << typeid(I_16 *).name() << endl;
	cout << "I_32*: " << typeid(I_32 *).name() << endl;
	cout << "I_64*: " << typeid(I_64 *).name() << endl;

}

static void printCombinations(void)
{
	cout << "Combinations:" << endl;
	ArithTemplate<U_8> a;
	ArithTemplate<U_16> b;
	ArithTemplate<U_32> c;
	ArithTemplate<U_64> d;
	ArithTemplate<I_8> e;
	ArithTemplate<I_16> f;
	ArithTemplate<I_32> g;
	ArithTemplate<I_64> h;

	U_8 * ptr1, *ptr2;

	cout << typeid(ptr1).name() << " - " << typeid(ptr2).name() << ": " << typeid(ptr1 - ptr2).name() << endl;
}

int main()
{
	printBasicTypes();
	printCombinations();
}
