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
import java.nio.ByteOrder;

import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.j9ddr.corereaders.memory.TestAbstractMemory.MockMemory;
import com.ibm.j9ddr.corereaders.memory.TestAbstractMemory.MockMemorySource;

/**
 * @author andhall
 *
 */
public class TestIMemoryImageInputStream
{

	@Test
	public void testByteOrder()
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x10000);
		
		assertSame(ByteOrder.BIG_ENDIAN,sut.getByteOrder());
		
		memory = new MockMemory(ByteOrder.LITTLE_ENDIAN);
		sut = new IMemoryImageInputStream(memory, 0x10000);
		
		assertSame(ByteOrder.LITTLE_ENDIAN,sut.getByteOrder());
	}
	
	@Test
	public void testReadIndividualBytes() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xDE, (byte)0xAD, (byte)0xBE, (byte)0xEF}, 0, 0x10000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x10000);
		
		assertEquals(0xDE,sut.read());
		assertEquals(0xAD,sut.read());
		assertEquals(0xBE,sut.read());
		assertEquals(0xEF,sut.read());
		assertEquals(0xDE,sut.read());
	}
	
	@Test
	public void testReadBlocksOfMemory() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xDE, (byte)0xAD, (byte)0xBE, (byte)0xEF}, 0, 0x10000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x10000);
		
		byte[] buffer = new byte[5];
		
		assertEquals(5,sut.read(buffer));
		
		assertEquals((byte)0xDE,buffer[0]);
		assertEquals((byte)0xAD,buffer[1]);
		assertEquals((byte)0xBE,buffer[2]);
		assertEquals((byte)0xEF,buffer[3]);
		assertEquals((byte)0xDE,buffer[4]);
		
		assertEquals(5,sut.read(buffer));
		
		assertEquals((byte)0xAD,buffer[0]);
		assertEquals((byte)0xBE,buffer[1]);
		assertEquals((byte)0xEF,buffer[2]);
		assertEquals((byte)0xDE,buffer[3]);
		assertEquals((byte)0xAD,buffer[4]);
	}
	
	@Test
	public void testReadBlocksSpanning() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xDE, (byte)0xAD, (byte)0xBE, (byte)0xEF}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xCA, (byte)0xFE, (byte)0xBA, (byte)0xBE}, 0, 0x20000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x1FFFC);
		
		byte[] buffer = new byte[10];
		
		assertEquals(10,sut.read(buffer));
		
		assertEquals((byte)0xDE,buffer[0]);
		assertEquals((byte)0xAD,buffer[1]);
		assertEquals((byte)0xBE,buffer[2]);
		assertEquals((byte)0xEF,buffer[3]);
		assertEquals((byte)0xCA,buffer[4]);
		assertEquals((byte)0xFE,buffer[5]);
		assertEquals((byte)0xBA,buffer[6]);
		assertEquals((byte)0xBE,buffer[7]);
		assertEquals((byte)0xCA,buffer[8]);
		assertEquals((byte)0xFE,buffer[9]);
	}
	
	@Test
	public void testEOFIndividualBytes() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xA0}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new UnbackedMemorySource(0x20000, 0x10000, "Deliberately not backed"));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x1FFFE);
		
		assertEquals(0xA0,sut.read());
		assertEquals(0xA0,sut.read());
		assertEquals(-1,sut.read());
	}
	
	@Test
	public void testGetStreamPosition() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xA0}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xB0}, 0, 0x20000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x10000);
		
		long pos = sut.getStreamPosition();
		assertEquals(0xA0,sut.read());
		assertEquals(0xA0,sut.read());
		assertEquals(pos + 2,sut.getStreamPosition());
	}
	
	@Test
	public void testSeek() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xA0}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xB0}, 0, 0x20000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x10000);
		
		assertEquals(0x10000,sut.getStreamPosition());
		assertEquals(0xA0,sut.read());
		sut.seek(0x20000);
		assertEquals(0x20000,sut.getStreamPosition());
		assertEquals(0xB0,sut.read());
	}
	
	@Test
	public void testSkipBytes() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xA0}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xB0}, 0, 0x20000,
				0x10000));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x1FFFE);
		
		assertEquals(0x1FFFE,sut.getStreamPosition());
		assertEquals(0xA0,sut.read());
		sut.skipBytes(2);
		assertEquals(0x20001,sut.getStreamPosition());
		assertEquals(0xB0,sut.read());
	}	
	
	@Test
	public void testEOFBlocks() throws Exception
	{
		AbstractMemory memory = new MockMemory(ByteOrder.BIG_ENDIAN);

		memory.addMemorySource(new MockMemorySource(new byte[] { (byte)0xDE, (byte)0xAD, (byte)0xBE, (byte)0xEF}, 0, 0x10000,
				0x10000));
		
		memory.addMemorySource(new UnbackedMemorySource(0x20000, 0x10000, "Deliberately not backed"));
		
		IMemoryImageInputStream sut = new IMemoryImageInputStream(memory, 0x1FFFC);
		
		byte[] buffer = new byte[10];
		
		assertEquals(4,sut.read(buffer));
		
		assertEquals((byte)0xDE,buffer[0]);
		assertEquals((byte)0xAD,buffer[1]);
		assertEquals((byte)0xBE,buffer[2]);
		assertEquals((byte)0xEF,buffer[3]);
		
		assertEquals(-1,sut.read(buffer));
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
