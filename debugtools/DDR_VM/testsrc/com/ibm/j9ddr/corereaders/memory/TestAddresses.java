/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import static org.junit.Assert.*;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;

import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.j9ddr.corereaders.memory.Addresses;

/**
 * Checks behaviour of unsigned 64 bit maths in Addresses class
 * 
 * @author andhall
 * 
 */
public class TestAddresses
{

	/**
	 * Test method for
	 * {@link com.ibm.j9ddr.corereaders.memory.Addresses#greaterThan(long, long)}
	 * .
	 */
	@Test
	public void testGreaterThan()
	{
		assertTrue(Addresses.greaterThan(1, 0));
		assertTrue(Addresses.greaterThan(Long.MAX_VALUE, Long.MAX_VALUE - 1));
		assertFalse(Addresses.greaterThan(7, 8));
		assertTrue(Addresses.greaterThan(0xFFFFFFFFFFFFFFFFL, 0));
		assertTrue(Addresses.greaterThan(0xFFFFFFFFFFFFFFFFL,
				0xFFFFFFFFFFFFFFFEL));
		assertTrue(Addresses.greaterThan(0xFFFFFFFFFFFFFFFFL, 47));
		assertFalse(Addresses.greaterThan(0xFFFFFFFFFFFFFFFFL,
				0xFFFFFFFFFFFFFFFFL));
		assertFalse(Addresses.greaterThan(0xFFFFFFFFFFFFFFFEL,
				0xFFFFFFFFFFFFFFFFL));
		assertFalse(Addresses.greaterThan(0, 0xFFFFFFFFFFFFFFFFL));
		assertFalse(Addresses.greaterThan(1, 1));
		assertTrue(Addresses.greaterThan(0xFFFFFFFFFFFFFFFFL, Long.MAX_VALUE));
	}

	/**
	 * Test method for
	 * {@link com.ibm.j9ddr.corereaders.memory.Addresses#lessThan(long, long)}.
	 */
	@Test
	public void testLessThan()
	{
		assertFalse(Addresses.lessThan(1, 0));
		assertFalse(Addresses.lessThan(Long.MAX_VALUE, Long.MAX_VALUE - 1));
		assertTrue(Addresses.lessThan(7, 8));
		assertFalse(Addresses.lessThan(0xFFFFFFFFFFFFFFFFL, 0));
		assertFalse(Addresses
				.lessThan(0xFFFFFFFFFFFFFFFFL, 0xFFFFFFFFFFFFFFFEL));
		assertFalse(Addresses.lessThan(0xFFFFFFFFFFFFFFFFL, 47));
		assertFalse(Addresses
				.lessThan(0xFFFFFFFFFFFFFFFFL, 0xFFFFFFFFFFFFFFFFL));
		assertTrue(Addresses.lessThan(0xFFFFFFFFFFFFFFFEL, 0xFFFFFFFFFFFFFFFFL));
		assertTrue(Addresses.lessThan(0, 0xFFFFFFFFFFFFFFFFL));
		assertFalse(Addresses.lessThan(1, 1));
		assertFalse(Addresses.lessThan(0xFFFFFFFFFFFFFFFFL, Long.MAX_VALUE));
	}
	
	@SuppressWarnings("unchecked")
	@BeforeClass
	public static void fixClassPath() {
		URL ddrURL = null;
		try {
			ddrURL = new File(System.getProperty("java.home", ""), "lib/ddr/j9ddr.jar").toURI().toURL();
		} catch (MalformedURLException e) {
			// This can only happen with a typo in source code
			e.printStackTrace();
		}
		
		ClassLoader loader = ClassLoader.getSystemClassLoader();
		Class loaderClazz = loader.getClass();

		try {
			while (loaderClazz != null && !loaderClazz.getName().equals("java.net.URLClassLoader")) {
				loaderClazz = loaderClazz.getSuperclass();
			}
			
			if (loaderClazz == null) {
				throw new UnsupportedOperationException("Application class loader is not an instance of URLClassLoader.  Can not initialize J9DDR");
			}
			
			Method addURLMethod = loaderClazz.getDeclaredMethod("addURL", new Class[] {URL.class});
			addURLMethod.setAccessible(true);
			addURLMethod.invoke(loader, new Object[] {ddrURL});
		} catch (SecurityException e) {
			e.printStackTrace();
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
	}

}
