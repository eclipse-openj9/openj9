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

import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.AbstractMemory;
import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

/**
 * Tests the code for mapping between addresses and memory ranges, and the
 * byte-order correction code.
 * 
 * @author andhall
 * 
 */
public class TestAbstractMemory
{

	@Test(expected = MemoryFault.class)
	public void testMemoryFault() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x10000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x20000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x30000,
				0x10000));
		/* GAP here */
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x50000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x60000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x70000,
				0x10000));

		sut.getByteAt(0x40000);
	}
	
	@Test(expected = MemoryFault.class)
	public void testMemoryFaultTooHigh() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x10000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x20000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x30000,
				0x10000));
		/* GAP here */
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x50000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x60000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x70000,
				0x10000));

		sut.getByteAt(0x100000);
	}
	
	@Test(expected = MemoryFault.class)
	public void testMemoryFaultTooLow() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x10000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x20000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x30000,
				0x10000));
		/* GAP here */
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x50000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x60000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x70000,
				0x10000));

		sut.getByteAt(0x0);
	}

	@Test
	public void testSimpleDereference() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x10000,
				0x10000));

		assertEquals(0xC, sut.getByteAt(0x10000));
		assertEquals(0xC, sut.getByteAt(0x10000));
	}

	@Test
	public void testOutOfOrderMemoryRangeRegistration() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x200000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x30000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x50000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x20000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x10000,
				0x10000));

		assertEquals(0xC, sut.getByteAt(0x10000));
		assertEquals(0xC, sut.getByteAt(0x10000));
	}

	@Test
	public void testTwoRangeOverlappingRead() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x10000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 0x20000,
				0x10000));

		/* Do a sequence of reads over the border of the memory ranges */
		byte[] buffer = new byte[4];
		sut.getBytesAt(0x1FFFC, buffer);

		assertEquals(0xC, buffer[0]);
		assertEquals(0xC, buffer[1]);
		assertEquals(0xC, buffer[2]);
		assertEquals(0xC, buffer[3]);

		sut.getBytesAt(0x1FFFD, buffer);
		assertEquals(0xC, buffer[0]);
		assertEquals(0xC, buffer[1]);
		assertEquals(0xC, buffer[2]);
		assertEquals(0xD, buffer[3]);

		sut.getBytesAt(0x1FFFE, buffer);
		assertEquals(0xC, buffer[0]);
		assertEquals(0xC, buffer[1]);
		assertEquals(0xD, buffer[2]);
		assertEquals(0xD, buffer[3]);

		sut.getBytesAt(0x1FFFF, buffer);
		assertEquals(0xC, buffer[0]);
		assertEquals(0xD, buffer[1]);
		assertEquals(0xD, buffer[2]);
		assertEquals(0xD, buffer[3]);

		sut.getBytesAt(0x20000, buffer);
		assertEquals(0xD, buffer[0]);
		assertEquals(0xD, buffer[1]);
		assertEquals(0xD, buffer[2]);
		assertEquals(0xD, buffer[3]);
	}

	@Test
	public void testMultipleRangeOverlappingRead() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0, 100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xA }, 0, 100,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xB }, 0, 200,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 300,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 400,
						100));

		byte[] buffer = new byte[300];

		sut.getBytesAt(100, buffer);

		for (int i = 0; i != 100; i++) {
			assertEquals("i = " + i, 0xA, buffer[i]);
			assertEquals("i = " + i, 0xB, buffer[i + 100]);
			assertEquals("i = " + i, 0xC, buffer[i + 200]);
		}
	}

	@Test
	public void testBigEndianByteOrdering() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x1, 0x2, 0x3, 0x4,
				0x5, 0x6, 0x7, 0x8 }, 0, 0, 100));

		assertSame(ByteOrder.BIG_ENDIAN, sut.getByteOrder());
		assertEquals(0xFF & 0x01, sut.getByteAt(0x0));
		assertEquals(0xFFFF & 0x0102, sut.getShortAt(0x0));
		assertEquals(0x01020304, sut.getIntAt(0x0));
		assertEquals(0x0102030405060708L, sut.getLongAt(0x0));
	}

	@Test
	public void testLittleEndianByteOrdering() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.LITTLE_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x1, 0x2, 0x3, 0x4,
				0x5, 0x6, 0x7, 0x8 }, 0, 0, 100));

		assertSame(ByteOrder.LITTLE_ENDIAN, sut.getByteOrder());
		assertEquals(0xFF & 0x01, sut.getByteAt(0x0));
		assertEquals(0xFFFF & 0x0201, sut.getShortAt(0x0));
		assertEquals(0x04030201, sut.getIntAt(0x0));
		assertEquals(0x0807060504030201L, sut.getLongAt(0x0));
	}

	@Test
	public void testOverlappingMemoryRanges() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0, 0x100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xA }, 0, 0x100,
						0x100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xB }, 0, 0x200,
						0x100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xC }, 0, 0x200,
						0x150));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 0x400,
						0x100));

		/*
		 * If we have an overlap with conflicting values, we can't resolve that.
		 * Returning either or will be sufficient.
		 */
		assertTrue(sut.getByteAt(0x200) == 0xB);
		assertEquals(0xC, sut.getByteAt(0x300));
		
		//Read across the overlap
		byte[] buffer = new byte[5];
		sut.getBytesAt(0x2fe, buffer);
		assertEquals(0xB,buffer[0]); //0x2fe
		assertEquals(0xB,buffer[1]); //0x2ff
		assertEquals(0xC,buffer[2]); //0x300
		assertEquals(0xC,buffer[3]); //0x301
		assertEquals(0xC,buffer[4]); //0x302
	}
	
	@Test
	public void testLargeMemoryRangeRead() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 8192, 450560));
		
		byte[] buffer = new byte[450560];
		
		sut.getBytesAt(8192, buffer);
	}
	
	@Test
	public void testGetByteAtOffsetAddress() throws Exception
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 8192, 450560));
		
		sut.getByteAt(9000);
	}

	@Test
	public void testFindPattern()
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0, 100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xA }, 0, 100,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xB }, 0, 200,
						100));
		/* deliberate break */
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 400,
						100));

		byte[] patternBuffer = new byte[100];

		patternBuffer[66] = 0xD;
		patternBuffer[67] = 0xE;
		patternBuffer[68] = 0xA;
		patternBuffer[69] = 0xD;
		patternBuffer[70] = 0xB;
		patternBuffer[71] = 0xE;
		patternBuffer[72] = 0xE;
		patternBuffer[73] = 0xF;

		sut.addMemorySource(new BufferMemorySource(0, 500, patternBuffer));

		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 600,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 700,
						100));
		/* deliberate break */
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 900,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0,
						1000, 100));

		long address = sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 0);
		assertEquals(566, address);

		/* Now check that the start address works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 580));

		/* Now check that the alignment works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 8, 0));
	}

	@Test
	public void testFindPatternAcrossRanges()
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0, 100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xA }, 0, 100,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xB }, 0, 200,
						100));
		/* deliberate break */
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 400,
						100));

		byte[] patternBuffer1 = new byte[100];
		byte[] patternBuffer2 = new byte[100];

		patternBuffer1[97] = 0xD;
		patternBuffer1[98] = 0xE;
		patternBuffer1[99] = 0xA;
		patternBuffer2[0] = 0xD;
		patternBuffer2[1] = 0xB;
		patternBuffer2[2] = 0xE;
		patternBuffer2[3] = 0xE;
		patternBuffer2[4] = 0xF;

		sut.addMemorySource(new BufferMemorySource(0, 500, patternBuffer1));
		sut.addMemorySource(new BufferMemorySource(0, 600, patternBuffer2));

		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 700,
						100));
		/* deliberate break */
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 900,
						100));
		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0,
						1000, 100));

		long address = sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 0);
		assertEquals(597, address);

		/* Now check that the start address works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 598));

		/* Now check that the alignment works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 8, 0));
	}

	@Test
	public void testFindPatternWithDeadSpace()
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut
				.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0,
						0x1000));
		sut
				.addMemorySource(new UnbackedMemorySource(0x1000, 0x2000,
						"Dead space"));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xB }, 0, 0x3000,
				0x1000));
		/* deliberate break */
		sut
				.addMemorySource(new UnbackedMemorySource(0x4000, 0x1000,
						"Dead space"));

		byte[] patternBuffer1 = new byte[0x1000];

		patternBuffer1[97] = 0xD;
		patternBuffer1[98] = 0xE;
		patternBuffer1[99] = 0xA;
		patternBuffer1[100] = 0xD;
		patternBuffer1[101] = 0xB;
		patternBuffer1[102] = 0xE;
		patternBuffer1[103] = 0xE;
		patternBuffer1[104] = 0xF;

		sut.addMemorySource(new BufferMemorySource(0, 0x5000, patternBuffer1));

		sut.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 0x7000,
				0x1000));
		/* deliberate break */
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 0x9000,
				0x1000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0xD }, 0, 0x10000,
				0x1000));

		long address = sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 0);
		assertEquals(0x5061, address);

		/* Now check that the start address works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 1, 0x5062));

		/* Now check that the alignment works */
		assertEquals(-1, sut.findPattern(new byte[] { 0xD, 0xE, 0xA, 0xD, 0xB,
				0xE, 0xE, 0xF }, 8, 0));
	}

	@Test
	public void testMergeRanges()
	{
		AbstractMemory sut = new MockMemory(ByteOrder.BIG_ENDIAN);

		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x10000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x60000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x50000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x30000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x20000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x70000,
				0x10000));
		sut.addMemorySource(new MockMemorySource(new byte[] { 0x0 }, 0, 0x80000,
				0x10000));

		/* Should return base/size data in order of ascending base address */
		long[][] result = sut.buildRangeTable();

		assertEquals(7, result.length);

		int mergedRecords = sut.mergeRangeTable(result);
		assertEquals(2, mergedRecords);
		assertEquals(0x10000, result[0][0]);
		assertEquals(0x30000, result[0][1]);
		assertEquals(0x50000, result[1][0]);
		assertEquals(0x40000, result[1][1]);
	}

	/* Some tests to check the mock objects will do what we need them to */
	@Test(expected = IllegalArgumentException.class)
	public void testMockMemoryErrorHandlingTooSmall() throws Exception
	{
		IMemorySource sut = new MockMemorySource(new byte[] { 0 }, 0, 0x10000,
				100);

		sut.getBytes(0x50, new byte[10], 0, 10);
	}

	@Test(expected = IllegalArgumentException.class)
	public void testMockMemoryErrorHandlingTooLarge() throws Exception
	{
		IMemorySource sut = new MockMemorySource(new byte[] { 0 }, 0, 0x10000,
				0x10);

		sut.getBytes(0x10010, new byte[10], 0, 10);
	}
	
	@Test
	public void testAlignment()
	{
		assertEquals(0,MemorySourceTable.getAlignment(0xF));
		assertEquals(1,MemorySourceTable.getAlignment(0xE));
		assertEquals(2,MemorySourceTable.getAlignment(0xC));
		assertEquals(3,MemorySourceTable.getAlignment(0x8));		
		assertEquals(4,MemorySourceTable.getAlignment(0xF0));
		assertEquals(5,MemorySourceTable.getAlignment(0xE0));
		assertEquals(6,MemorySourceTable.getAlignment(0xC0));
		assertEquals(7,MemorySourceTable.getAlignment(0x80));
		assertEquals(8,MemorySourceTable.getAlignment(0xF00));
		assertEquals(64,MemorySourceTable.getAlignment(0x0));
	}

	@Test
	public void testMockMemoryErrorHandlingSingleBytePattern() throws Exception
	{
		IMemorySource sut = new MockMemorySource(new byte[] { 0xF }, 0, 0x10000,
				0x10);

		byte[] buffer = new byte[10];

		sut.getBytes(0x10000, buffer, 0, buffer.length);

		for (int i = 0; i != 10; i++) {
			assertEquals("Tested index " + i, 0xF, buffer[i]);
		}

		sut.getBytes(0x10006, buffer, 0, buffer.length);

		for (int i = 0; i != 10; i++) {
			assertEquals("Tested index " + i, 0xF, buffer[i]);
		}
	}

	@Test
	public void testMockMemoryErrorHandlingMultiBytePattern() throws Exception
	{
		IMemorySource sut = new MockMemorySource(new byte[] { 0xD, 0xE, 0xA, 0xD,
				0xB, 0xE, 0xE, 0xF }, 0, 0x10000, 0x10);

		byte[] buffer = new byte[10];

		sut.getBytes(0x10000, buffer, 0, buffer.length);

		assertEquals(0xD, buffer[0]);
		assertEquals(0xE, buffer[1]);
		assertEquals(0xA, buffer[2]);
		assertEquals(0xD, buffer[3]);
		assertEquals(0xB, buffer[4]);
		assertEquals(0xE, buffer[5]);
		assertEquals(0xE, buffer[6]);
		assertEquals(0xF, buffer[7]);
		assertEquals(0xD, buffer[8]);
		assertEquals(0xE, buffer[9]);

		buffer = new byte[10];

		sut.getBytes(0x10006, buffer, 0, buffer.length);

		assertEquals(0xE, buffer[0]);
		assertEquals(0xF, buffer[1]);
		assertEquals(0xD, buffer[2]);
		assertEquals(0xE, buffer[3]);
		assertEquals(0xA, buffer[4]);
		assertEquals(0xD, buffer[5]);
		assertEquals(0xB, buffer[6]);
		assertEquals(0xE, buffer[7]);
		assertEquals(0xE, buffer[8]);
		assertEquals(0xF, buffer[9]);
	}
	
	@Test
	public void testOverlap()
	{
		IMemoryRange a = null;
		IMemoryRange b = null;
		
		//A & B don't overlap
		a = new UnbackedMemorySource(0x100, 0x100,"");
		b = new UnbackedMemorySource(0x200, 0x100,"");
		
		assertFalse(a.overlaps(b));
		assertFalse(b.overlaps(a));
		
		//End of A overlaps start of B
		a = new UnbackedMemorySource(0x100,0x101,"");
		b = new UnbackedMemorySource(0x200,0x100,"");
		assertTrue(a.overlaps(b));
		
		//Start of B overlaps end of A
		assertTrue(b.overlaps(a));
		
		a = new UnbackedMemorySource(0x100,0x300,"");
		b = new UnbackedMemorySource(0x200,0x100,"");
		//B is a sub range of A
		assertTrue(a.overlaps(b));
		assertTrue(b.overlaps(a));
		
		a = new MockMemorySource(new byte[] { 0xB }, 0, 0x200,0x100);
		b = new MockMemorySource(new byte[] { 0xC }, 0, 0x200,0x150);
		
		assertTrue(a.overlaps(b));
		assertTrue(b.overlaps(a));
	}
	
	@Test
	public void testSubRange()
	{
		IMemoryRange a = null;
		IMemoryRange b = null;
		
		a = new UnbackedMemorySource(0x100, 0x300,"");
		b = new UnbackedMemorySource(0x300, 0x100,"");
		
		assertTrue(a.isSubRange(b));
		assertFalse(b.isSubRange(a));
		
		b = new UnbackedMemorySource(0x300, 0x101,"");
		assertFalse(a.isSubRange(b));
		assertFalse(b.isSubRange(a));
		
		a = new MockMemorySource(new byte[] { 0xB }, 0, 0x200,0x100);
		b = new MockMemorySource(new byte[] { 0xC }, 0, 0x200,0x150);
		
		assertFalse(a.isSubRange(b));
		assertTrue(b.isSubRange(a));
	}

	static class BufferMemorySource extends BaseMockMemoryRange implements IMemorySource
	{
		private final byte[] buffer;

		public BufferMemorySource(int asid, long baseAddress, byte[] buffer)
		{
			super(asid, baseAddress, buffer.length);
			this.buffer = buffer;
		}

		public int getBytes(long address, byte[] destBuffer, int destOffset,
				int length) throws MemoryFault
		{
			int bufferOffset = (int) (address - getBaseAddress());

			System.arraycopy(buffer, bufferOffset, destBuffer, destOffset,
					length);
			return length;
		}

		public String getName()
		{
			return null;
		}

	}

	/**
	 * Mock memory range that models a region of memory with a particular byte
	 * pattern repeating over and over.
	 * 
	 * @author andhall
	 * 
	 */
	static class MockMemorySource extends BaseMockMemoryRange implements IMemorySource
	{
		protected final byte[] pattern;

		public MockMemorySource(byte[] pattern, int asid, long baseAddress,
				long size)
		{
			super(asid, baseAddress, size);
			this.pattern = pattern;
		}

		private byte getByte(long address) throws MemoryFault
		{
			if (Addresses.greaterThan(address, getTopAddress())) {
				throw new IllegalArgumentException("Address "
						+ Long.toHexString(address) + " too big");
			}

			if (Addresses.lessThan(address, baseAddress)) {
				throw new IllegalArgumentException("Address "
						+ Long.toHexString(address) + " too small");
			}

			int index = (int) ((address - baseAddress) % pattern.length);

			return pattern[index];
		}

		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			for (int count = 0; count != length; count++) {
				buffer[offset + count] = getByte(address + count);
			}
			return length;
		}

		public String getName()
		{
			return null;
		}
	}

	static class MockMemory extends AbstractMemory
	{
		protected MockMemory(ByteOrder byteOrder)
		{
			super(byteOrder);
		}

		public Platform getPlatform()
		{
			return null;
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
