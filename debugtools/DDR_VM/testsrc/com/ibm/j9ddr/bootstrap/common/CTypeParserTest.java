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
package com.ibm.j9ddr.bootstrap.common;

import static org.junit.Assert.*;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;

import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.j9ddr.CTypeParser;

/**
 * @author andhall
 *
 */
public class CTypeParserTest
{

	@Test
	public void testSimpleArray()
	{
		CTypeParser sut = new CTypeParser("int[][]");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("[][]",sut.getSuffix());
	}
	
	@Test
	public void testSimpleArray2()
	{
		CTypeParser sut = new CTypeParser("int []");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("[]",sut.getSuffix());
	}
	
	@Test
	public void testSimpleArray3()
	{
		CTypeParser sut = new CTypeParser("int [ ]");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("[]",sut.getSuffix());
	}
	
	
	@Test
	public void testSimplePointer()
	{
		CTypeParser sut = new CTypeParser("int **");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("**",sut.getSuffix());
	}
	
	@Test
	public void testConst()
	{
		CTypeParser sut = new CTypeParser("const int");
		
		assertEquals("const ",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("",sut.getSuffix());
	}
	
	@Test
	public void testSizedArray()
	{
		CTypeParser sut = new CTypeParser("int[4][6][8]");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("[4][6][8]",sut.getSuffix());
	}
	
	@Test
	public void testVariableSizedArray()
	{
		CTypeParser sut = new CTypeParser("int*[banana]");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("*[banana]",sut.getSuffix());
	}
	
	@Test
	public void testConstPointers()
	{
		CTypeParser sut = new CTypeParser("const int * const");
		
		assertEquals("const ",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("* const",sut.getSuffix());
	}
	
	@Test
	public void testVolatilePointers()
	{
		CTypeParser sut = new CTypeParser("const int * volatile");
		
		assertEquals("const ",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("* volatile",sut.getSuffix());
	}
	
	@Test
	public void testFunnySpaces()
	{
		CTypeParser sut = new CTypeParser("   const   int [ ] [] [ ]");
		
		assertEquals("const ",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("[][][]",sut.getSuffix());
	}
	
	@Test
	public void testFunnySpaces2()
	{
		CTypeParser sut = new CTypeParser("   const   int *   const    * volatile      *    *");
		
		assertEquals("const ",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals("* const * volatile **",sut.getSuffix());
	}
	
	@Test
	public void testBitfields()
	{
		CTypeParser sut = new CTypeParser("int : 1");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals(":1",sut.getSuffix());
	}
	
	@Test
	public void testBitfieldsFunnySpaces()
	{
		CTypeParser sut = new CTypeParser("int     :      422");
		
		assertEquals("",sut.getPrefix());
		assertEquals("int",sut.getCoreType());
		assertEquals(":422",sut.getSuffix());
	}
	
	@Test
	public void testTraceExample()
	{
		CTypeParser sut = new CTypeParser("struct message *volatile");
		
		assertEquals("", sut.getPrefix());
		assertEquals("struct message",sut.getCoreType());
		assertEquals("* volatile", sut.getSuffix());
	}
	
	@Test
	public void testUserTypeContainingVolatile()
	{
		CTypeParser sut = new CTypeParser("struct myvolatile");
		
		assertEquals("", sut.getPrefix());
		assertEquals("struct myvolatile",sut.getCoreType());
		assertEquals("", sut.getSuffix());
	}
	
	@Test
	public void testUserTypeContainingConst()
	{
		CTypeParser sut = new CTypeParser("struct myconst");
		
		assertEquals("", sut.getPrefix());
		assertEquals("struct myconst",sut.getCoreType());
		assertEquals("", sut.getSuffix());
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
