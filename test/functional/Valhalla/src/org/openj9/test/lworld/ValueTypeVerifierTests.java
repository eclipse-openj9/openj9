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
package org.openj9.test.lworld;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

/*
 * Instructions to run this test:
 * 
 * 1) recompile the JVM with J9VM_OPT_VALHALLA_VALUE_TYPES flag turned on in j9cfg.h.ftl (or j9cfg.h.in when cmake is turned on)
 * 2) cd [openj9-openjdk-dir]/openj9/test/TestConfig
 * 3) export JAVA_BIN=[openj9-openjdk-dir]/build/linux-x86_64-normal-server-release/images/jdk/bin
 * 4) export PATH=$JAVA_BIN:$PATH
 * 5) export JDK_VERSION=Valhalla
 * 6) export SPEC=linux_x86-64_cmprssptrs
 * 7) export BUILD_LIST=functional/Valhalla
 * 8) make -f run_configure.mk && make compile && make _sanity
 */

@Test(groups = { "level.sanity" })
public class ValueTypeVerifierTests {
	static Lookup lookup = MethodHandles.lookup();
	static Class point2DClass = null;
	static MethodHandle makePoint2D = null;

	
	/*
	 * Create a value type without avoiding bytecode verification
	 * 
	 * value Point2D {
	 * 	int x;
	 * 	int y;
	 * }
	 */
	@Test(priority=1)
	static public void testCreatePoint2DWithVerification() throws Throwable {
		String fields[] = {"x:I", "y:I"};
		point2DClass = ValueTypeGenerator.generateVerifiableValueClass("Point2D", fields);
		makePoint2D = lookup.findStatic(point2DClass, "makeValue", MethodType.methodType(point2DClass, int.class, int.class));
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		Object point2D = makePoint2D.invoke(x, y);
	}

	/*
	 * Create a value type with double slot primative members
	 * that doesn't avoid bytecode verification.
	 * 
	 * value Point2DComplex {
	 * 	double d;
	 * 	long j;
	 * }
	 */
	@Test(priority=1)
	static public void testCreatePoint2DComplexWithVerification() throws Throwable {
		String fields[] = {"d:D", "j:J"};
		Class point2DComplexClass = ValueTypeGenerator.generateVerifiableValueClass("Point2DComplex", fields);
		MethodHandle makePoint2DComplex = lookup.findStatic(point2DComplexClass, "makeValue", MethodType.methodType(point2DComplexClass, double.class, long.class));
		double d = Double.MAX_VALUE;
		long j = Long.MAX_VALUE;
		Object point2D = makePoint2DComplex.invoke(d, j);
	}
}
