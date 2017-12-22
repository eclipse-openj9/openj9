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

import com.ibm.j9ddr.corereaders.memory.BaseMemoryRange;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

/**
 * @author andhall
 * 
 */
public class TestComparableMemoryRange
{

	@Test
	public void testContains()
	{
		BaseMemoryRange sut = new DummyComparableMemoryRange(0, 0x10000,
				0x1000);

		assertTrue(sut.contains(0x10000));
		assertTrue(sut.contains(0x10001));
		assertTrue(sut.contains(0x10FFF));
		assertTrue(sut.contains(sut.getTopAddress()));
		assertTrue(sut.contains(sut.getBaseAddress()));
		assertFalse(sut.contains(0x11000));
		assertFalse(sut.contains(0x1));
		assertFalse(sut.contains(0xFFFF));
		assertFalse(sut.contains(0x11000));
	}

	@Test
	public void testCompare()
	{
		BaseMemoryRange one = new DummyComparableMemoryRange(0, 0x10000,
				0x10000);
		BaseMemoryRange two = new DummyComparableMemoryRange(1, 0x10000,
				0x10000);
		BaseMemoryRange three = new DummyComparableMemoryRange(0,
				0x20000, 0x10000);
		BaseMemoryRange four = new DummyComparableMemoryRange(0, 0x10000,
				0x20000);
		BaseMemoryRange five = new DummyComparableMemoryRange(0,
				0xFFFFFFFFFFFFF000L, 0x1000);

		assertEquals(0, one.compareTo(one));
		assertTrue(one.compareTo(two) < 0);
		assertTrue(two.compareTo(one) > 0);
		assertTrue(one.compareTo(three) < 0);
		assertTrue(three.compareTo(one) > 0);
		assertTrue(one.compareTo(five) < 0);
		assertTrue(five.compareTo(one) > 0);
		assertTrue(one.compareTo(four) < 0);
		assertTrue(four.compareTo(one) > 0);
	}

	/**
	 * Mock object for testing ComparableMemoryRange
	 * 
	 * @author andhall
	 * 
	 */
	private static class DummyComparableMemoryRange extends
			BaseMockMemoryRange implements IMemorySource
	{

		public DummyComparableMemoryRange(int asid, long baseAddress, long size)
		{
			super(asid,baseAddress,size);
		}

		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			throw new UnsupportedOperationException("Not implemented");
		}

		public String getName()
		{
			throw new UnsupportedOperationException("Not implemented");
		}

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
