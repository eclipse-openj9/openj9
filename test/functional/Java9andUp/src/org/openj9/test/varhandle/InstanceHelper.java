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

public class InstanceHelper {
	byte b;
	char c;
	double d;
	float f;
	int i;
	long j;
	String l;
	short s;
	boolean z;
	Integer integer;
	Parent parent = new Parent();
	Child child = new Child();
	final int finalI = 1;

	InstanceHelper() {}
	InstanceHelper(byte b) { this.b = b; }
	InstanceHelper(char c) { this.c = c; }
	InstanceHelper(double d) { this.d = d; }
	InstanceHelper(float f) { this.f = f; }
	InstanceHelper(int i) { this.i = i; }
	InstanceHelper(long j) { this.j = j; }
	InstanceHelper(short s) { this.s = s; }
	InstanceHelper(boolean z) { this.z = z; }
	InstanceHelper(String l) { this.l = l; }
	InstanceHelper(Integer i) { this.integer = i; }
}

class Parent {
	int i;
}

class Child extends Parent {}

class SomeType {}
