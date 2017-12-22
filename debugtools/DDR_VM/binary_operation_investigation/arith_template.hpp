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

#include <iostream>

#include "j9_types.hpp"

using namespace std;


template<typename T1> 
class ArithTemplate
{
public:
	ArithTemplate(void) {
		U_8 u8;
		U_16 u16;
		U_32 u32;
		U_64 u64;
		I_8 i8;
		I_16 i16;
		I_32 i32;
		I_64 i64;
		cout << typeid(base).name() << " + " << typeid(u8).name() << ": " << typeid(base + u8).name() << endl;
		cout << typeid(base).name() << " + " << typeid(u16).name() << ": " << typeid(base + u16).name() << endl;
		cout << typeid(base).name() << " + " << typeid(u32).name() << ": " << typeid(base + u32).name() << endl;
		cout << typeid(base).name() << " + " << typeid(u64).name() << ": " << typeid(base + u64).name() << endl;
		cout << typeid(base).name() << " + " << typeid(i8).name() << ": " << typeid(base + i8).name() << endl;
		cout << typeid(base).name() << " + " << typeid(i16).name() << ": " << typeid(base + i16).name() << endl;
		cout << typeid(base).name() << " + " << typeid(i32).name() << ": " << typeid(base + i32).name() << endl;
		cout << typeid(base).name() << " + " << typeid(i64).name() << ": " << typeid(base + i64).name() << endl;
	}

private:
	T1 base;
};
