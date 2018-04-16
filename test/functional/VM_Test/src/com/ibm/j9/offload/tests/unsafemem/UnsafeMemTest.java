package com.ibm.j9.offload.tests.unsafemem;

/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp. and others
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

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.nio.ByteBuffer;


public class UnsafeMemTest extends TestCase {
	
	public static void main (String[] args) {
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(UnsafeMemTest.class);
	}
	
	/* The following are not supported for direct byte buffers so we don't test them
	 * 
	 * array()
	 * arrayOffset()
	 * 
	 * 
	 */
	
	
	/**
	 * tests Get and Put Byte
	 */
	public void testGetPutByte(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutByte(buffer);
	}
	
	/**
	 * tests Get and Put Short
	 */
	public void testGetPutShort(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutShort(buffer);
	}
	
	/**
	 * tests Get and Put Char
	 */
	public void testGetPutChar(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutChar(buffer);
	}
	
	/**
	 * tests Get and Put Int
	 */
	public void testGetPutInt(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutInt(buffer);
	}
	
	/**
	 * tests Get and Put Long
	 */
	public void testGetPutLong(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutLong(buffer);
	}
	
	/**
	 * tests Get and Put Float
	 */
	public void testGetPutFloat(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutFloat(buffer);
	}
	
	/**
	 * tests Get and Put Double
	 */
	public void testGetPutDouble(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetPutDouble(buffer);
	}
	
	/**
	 * test duplicate
	 */
	public void testDuplicate(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestDuplicate(buffer);
	}
	
	/**
	 * doTests Get bytes from a buffer into a byte array
	 */
	public void testGetIntoByteArray(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestGetIntoByteArray(buffer);
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void testPutBytesFromByteArray(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestPutBytesFromByteArray(buffer);
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void testPutBytesFromByteBuffer(){
		ByteBuffer buffer1 = ByteBuffer.allocateDirect(16);
		ByteBuffer buffer2 = ByteBuffer.allocateDirect(16);
		doTestPutBytesFromByteBuffer(buffer1,buffer2);
	}
	
	/**
	 * test slice 
	 */
	public void testSlice(){
		ByteBuffer buffer = ByteBuffer.allocateDirect(16);
		doTestSlice(buffer);
	}
	
	/* now repeat for the cases when a direct byte buffer was allocated by a native */
	/**
	 * tests Get and Put Byte
	 */
	public void testGetPutByteNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutByte(buffer);
	}
	
	/**
	 * tests Get and Put Short
	 */
	public void testGetPutShortNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutShort(buffer);
	}
	
	/**
	 * tests Get and Put Char
	 */
	public void testGetPutCharNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutChar(buffer);
	}
	
	/**
	 * tests Get and Put Int
	 */
	public void testGetPutIntNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutInt(buffer);
	}
	
	/**
	 * tests Get and Put Long
	 */
	public void testGetPutLongNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutLong(buffer);
	}
	
	/**
	 * tests Get and Put Float
	 */
	public void testGetPutFloatNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutFloat(buffer);
	}
	
	/**
	 * tests Get and Put Double
	 */
	public void testGetPutDoubleNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetPutDouble(buffer);
	}
	
	/**
	 * test duplicate
	 */
	public void testDuplicateNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestDuplicate(buffer);
	}
	
	/**
	 * doTests Get bytes from a buffer into a byte array
	 */
	public void testGetIntoByteArrayNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestGetIntoByteArray(buffer);
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void testPutBytesFromByteArrayNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestPutBytesFromByteArray(buffer);
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void testPutBytesFromByteBufferNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer1 = natives.getNativeAllocatedByteBuffer(16);
		ByteBuffer buffer2 = natives.getNativeAllocatedByteBuffer(16);
		ByteBuffer buffer3 = ByteBuffer.allocateDirect(16);
		
		doTestPutBytesFromByteBuffer(buffer1,buffer2);

		buffer1.rewind();
		buffer2.rewind();
		buffer3.rewind();
		doTestPutBytesFromByteBuffer(buffer1,buffer3);
		
		buffer1.rewind();
		buffer2.rewind();
		buffer3.rewind();
		doTestPutBytesFromByteBuffer(buffer3,buffer1);
	}
	
	/**
	 * test slice
	 */
	public void testSliceNative(){
		TestNativesUnsafe natives = new TestNativesUnsafe();
		ByteBuffer buffer = natives.getNativeAllocatedByteBuffer(16);
		doTestSlice(buffer);
	}
	
	/*******************************
	 * Actual test implementations re-used by above tests
	 ********************************/
	
	/**
	 * tests Get and Put Byte
	 */
	public void doTestGetPutByte(ByteBuffer buffer){
		buffer.put(0,(byte) 0x10);
		buffer.put(1,(byte) 0x11);
		assertTrue(buffer.get(0) == 0x10);
		assertTrue(buffer.get(1) == 0x11);
	}
	
	/**
	 * doTests Get and Put Short
	 */
	public void doTestGetPutShort(ByteBuffer buffer){
		buffer.putShort(0,(short) 0x1000);
		buffer.putShort(2,(short) 0x1112);
		assertTrue(buffer.getShort(0) == 0x1000);
		assertTrue(buffer.getShort(2) == 0x1112);
	}
	
	/**
	 * doTests Get and Put Char
	 */
	public void doTestGetPutChar(ByteBuffer buffer){
		buffer.putChar(0,(char) 0x1000);
		buffer.putChar(2,(char) 0x1112);
		assertTrue(buffer.getChar(0) == 0x1000);
		assertTrue(buffer.getChar(2) == 0x1112);
	}
	
	/**
	 * doTests Get and Put Int
	 */
	public void doTestGetPutInt(ByteBuffer buffer){
		buffer.putInt(0,(int) 0x10000000);
		buffer.putInt(4,(int) 0x11121314);
		assertTrue(buffer.getInt(0) == 0x10000000);
		assertTrue(buffer.getInt(4) == 0x11121314);
	}
	
	/**
	 * doTests Get and Put Long
	 */
	public void doTestGetPutLong(ByteBuffer buffer){
		buffer.putLong(0,(long) 0x1000000000000000L);
		buffer.putLong(8,(long) 0x1112131415161718L);
		assertTrue(buffer.getLong(0) == 0x1000000000000000L);
		assertTrue(buffer.getLong(8) == 0x1112131415161718L);
	}
	
	/**
	 * doTests Get and Put Float
	 */
	public void doTestGetPutFloat(ByteBuffer buffer){
		buffer.putFloat(0,(float) 1.1111111);
		buffer.putFloat(4,(float) 2.2222222);
		assertTrue(buffer.getFloat(0) == (float)1.1111111);
		assertTrue(buffer.getFloat(4) == (float) 2.2222222);
	}
	
	/**
	 * doTests Get and Put Double
	 */
	public void doTestGetPutDouble(ByteBuffer buffer){
		buffer.putDouble(0,(double) 1.1111111);
		buffer.putDouble(8,(double) 2.2222222);
		assertTrue(buffer.getDouble(0) == (double) 1.1111111);
		assertTrue(buffer.getDouble(8) == (double) 2.2222222);
	}
	
	/**
	 * doTests Get and Put Double
	 */
	public void doTestDuplicate(ByteBuffer buffer){
		ByteBuffer copy = buffer.duplicate();
		buffer.putDouble(0,(double) 1.1111111);
		buffer.putDouble(8,(double) 2.2222222);
		assertTrue(buffer.getDouble(0) == (double) 1.1111111);
		assertTrue(buffer.getDouble(8) == (double) 2.2222222);
		assertTrue(copy.getDouble(0) == (double) 1.1111111);
		assertTrue(copy.getDouble(8) == (double) 2.2222222);
	}
	
	/**
	 * doTests Get bytes from a buffer into a byte array
	 */
	public void doTestGetIntoByteArray(ByteBuffer buffer){
		for (int i=0;i<buffer.capacity();i++){
			buffer.put(i,(byte) i);
		}
		byte copy[] = new byte[buffer.capacity()];
		buffer.get(copy);
		
		for (int i=0;i<buffer.capacity();i++){
			assertTrue(copy[i] == buffer.get(i));
		}
		
		/* now try with a subset */
		buffer.rewind();
		for (int i=0;i<buffer.capacity();i++){
			buffer.put(i,(byte) (i+1));
		}
		byte copy2[] = new byte[buffer.capacity() + 2];
		buffer.get(copy2,2,4);
		
		for (int i=0;i<4;i++){
			assertTrue(copy2[i+2] == (byte) i+1);
		}
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void doTestPutBytesFromByteArray(ByteBuffer buffer){
		byte[] theData = new byte[buffer.capacity()];
		for (int i=0;i<buffer.capacity();i++){
			theData[i] = (byte) i;
		}
		buffer.put(theData);
		
		for (int i=0;i<buffer.capacity();i++){
			assertTrue(buffer.get(i) == i);
		}
		
		/* now try with a subset */
		buffer.rewind();
		for (int i=0;i<buffer.capacity();i++){
			theData[i] = (byte) (i+1);
		}
		buffer.put(theData,2,buffer.capacity() -2);
		
		for (int i=0;i<buffer.capacity() -2;i++){
			assertTrue(buffer.get(i) == i+3);
		}
	}
	
	/**
	 * test put bytes to buffer from byte array 
	 */
	public void doTestPutBytesFromByteBuffer(ByteBuffer buffer1, ByteBuffer buffer2){
		for(int i=0;i<buffer1.capacity();i++){
			buffer1.put(i,(byte) 1);
		}
		
		for(int i=0;i<buffer2.capacity();i++){
			buffer2.put(i,(byte) 2);
		}
		
		buffer2.put(buffer1);
		for(int i=0;i<buffer1.capacity();i++){
			assertTrue("first buffer not equal to second buffer", buffer1.get(i) == buffer2.get(i));
		}
		
	}
	
	/**
	 * test slice
	 */
	public void doTestSlice(ByteBuffer buffer){
		for(int i=0;i<buffer.capacity();i++){
			buffer.put(i,(byte) i);
		}
		
		buffer.get();
		
		ByteBuffer buffer2 = buffer.slice();
		
		for(int i=0;i<buffer2.capacity();i++){
			assertTrue("elements don't line up", buffer.get(i+1) == buffer2.get(i));
		}
		
		for(int i=0;i<buffer.capacity();i++){
			buffer.put(i,(byte) 1);
		}
		
		for(int i=0;i<buffer2.capacity();i++){
			assertTrue("elements don't line up", buffer2.get(i) == 1);
		}
	}
	
}
