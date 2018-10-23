package org.openj9.test.java.security;

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

import java.lang.invoke.MethodHandles;
import org.testng.Assert;

// This class is loaded by ClassLoaderOne.
public class ChildMHLookupField extends Parent {
	private final ClassLoader clOne = ChildMHLookupField.class.getClassLoader();
	private final Object objDummyOne = clOne.loadClass("org.openj9.test.java.security.Dummy").newInstance();
	private final Class<?> clzDummyOne = objDummyOne.getClass();
	private final ClassLoader clTwo = Parent.class.getClassLoader();
	private final Object objDummyTwo = clTwo.loadClass("org.openj9.test.java.security.Dummy").newInstance();
	private final Class<?> clzDummyTwo = objDummyTwo.getClass();
	private final MethodHandles.Lookup mhLookup = MethodHandles.lookup();
	
	public ChildMHLookupField() throws Exception {
		// Verify that the classes are loaded by appropriate class loaders
		Assert.assertEquals(clOne.getClass(), ClassLoaderOne.class);
		Assert.assertEquals(clzDummyOne.getClassLoader().getClass(), ClassLoaderOne.class);

		Assert.assertEquals(clTwo.getClass(), ClassLoaderTwo.class);
		Assert.assertEquals(clzDummyTwo.getClassLoader().getClass(), ClassLoaderTwo.class);
	}

	// Loading constraints are violated when reference and type classes (instance field getter) are loaded by different class loaders.
	public void instanceFieldGetter1() throws Throwable {
		mhLookup.findGetter(Parent.class, "instanceField", clzDummyOne);
	}
	
	// Loading constraints are violated when reference/type(instance field getter) and access classes are loaded by different class loaders.
	public void instanceFieldGetter2() throws Throwable {
		mhLookup.findGetter(Parent.class, "instanceField", clzDummyTwo);
	}

	// Loading constraints are violated when reference and type classes (static field getter) are loaded by different class loaders.
	public void staticFieldGetter1() throws Throwable {
		mhLookup.findStaticGetter(Parent.class, "staticField", clzDummyOne);
	}
	
	// Loading constraints are violated when reference/type(static field getter) and access classes are loaded by different class loaders.
	public void staticFieldGetter2() throws Throwable {
		mhLookup.findStaticGetter(Parent.class, "staticField", clzDummyTwo);
	}

	// Loading constraints are violated when reference and type classes (instance field setter) are loaded by different class loaders.
	public void instanceFieldSetter1() throws Throwable {
		mhLookup.findSetter(Parent.class, "instanceField", clzDummyOne);
	}
	
	// Loading constraints are violated when reference/type(instance field setter) and access classes are loaded by different class loaders.
	public void instanceFieldSetter2() throws Throwable {
		mhLookup.findSetter(Parent.class, "instanceField", clzDummyTwo);
	}

	// Loading constraints are violated when reference and type classes (static field setter) are loaded by different class loaders.
	public void staticFieldSetter1() throws Throwable {
		mhLookup.findStaticSetter(Parent.class, "staticField", clzDummyOne);
	}
	
	// Loading constraints are violated when reference/type(static field setter) and access classes are loaded by different class loaders.
	public void staticFieldSetter2() throws Throwable {
		mhLookup.findStaticSetter(Parent.class, "staticField", clzDummyTwo);
	}
}
