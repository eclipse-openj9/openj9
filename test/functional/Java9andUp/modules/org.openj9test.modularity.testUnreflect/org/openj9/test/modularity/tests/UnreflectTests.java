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

package org.openj9.test.modularity.tests;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import org.openj9.test.unnamed.UnnamedReflectObjects;
import org.openj9.test.unnamed.UnnamedDummy;

import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class UnreflectTests {
	private final static MethodHandles.Lookup lookup = MethodHandles.lookup();
	/*
	 * Following tests check if a named module 'org.openj9test.modularity.testUnreflect' can 'unreflectXXX'
	 * an incoming reflect object which contains a reference to a named module 'org.openj9test.modularity.dummy'
	 * that doesn't export the package 'org.openj9.test.modularity.dummy' unconditionally (specifically not export
	 * to current running module 'org.openj9test.modularity.testUnreflect').
	 */

	@Test(groups = { "level.extended" })
	public void testLookupUnreflectGetter() throws Throwable {
		lookup.unreflectGetter(UnnamedReflectObjects.getReflectField());
	}

	@Test(groups = { "level.extended" })
	public void testLookupUnreflectSetter() throws Throwable {
		lookup.unreflectSetter(UnnamedReflectObjects.getReflectField());
	}

	@Test(groups = { "level.extended" })
	public void testLookupUnreflectConstructor() throws Throwable {
		lookup.unreflectConstructor(UnnamedReflectObjects.getReflectConstructor());
	}

	@Test(groups = { "level.extended" })
	public void testLookupUnreflect() throws Throwable {
		lookup.unreflect(UnnamedReflectObjects.getReflectMethod());
	}

	@Test(groups = { "level.extended" })
	public void testLookupUnreflectSpecial() throws Throwable {
		lookup.unreflectSpecial(UnnamedReflectObjects.getReflectMethod(), UnreflectTests.class);
	}
	
	@Test(groups = { "level.extended" })
	public void testLookupUnreflectVarHandle() throws Throwable {
		lookup.unreflectVarHandle(UnnamedReflectObjects.getReflectField());
	}
}
