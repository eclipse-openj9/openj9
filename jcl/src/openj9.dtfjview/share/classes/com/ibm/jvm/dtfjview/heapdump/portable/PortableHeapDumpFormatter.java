/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump.portable;

import java.io.DataOutputStream;
import java.io.IOException;

import com.ibm.jvm.dtfjview.heapdump.HeapDumpFormatter;
import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;

/**
 * A formatter for Portable heap dumps (PHDs).
 * 
 * Format documented here: http://w3.hursley.ibm.com/~dgriff/newphd.html
 * 
 * @author andhall
 *
 */
public class PortableHeapDumpFormatter extends HeapDumpFormatter
{
	/*Magic numbers at the head of a PHD stream*/
	public static final String MAGIC_STRING = "portable heap dump";
	public static final int PHD_VERSION_NUMBER = 6;
	/*Structural/outline tags*/
	public static final byte START_OF_HEADER_TAG = 1;
	public static final byte END_OF_HEADER_TAG = 2;
	public static final byte START_OF_DUMP_TAG = 2;
	public static final byte END_OF_DUMP_TAG = 3;
	/*Contents of header flags*/
	public static final byte HASHCODE_RANDOM_TAG = 3;
	public static final byte FULL_VERSION_TAG = 4;
	/* PHD Header flags word fields*/
	public static final int IS_64_BIT_HEADER_FLAG = 1;
	public static final int ALL_OBJECTS_HASHED_FLAG = 2;
	public static final int IS_J9_HEADER_FLAG = 4;
	/*Record tags*/
	public static final byte CLASS_RECORD_TAG = 6;
	public static final byte SHORT_OBJECT_RECORD_TAG = (byte) 0x80;
	public static final byte MEDIUM_OBJECT_RECORD_TAG = 0x40;
	public static final byte LONG_OBJECT_RECORD_TAG = 4;
	public static final byte OBJECT_ARRAY_RECORD_TAG = 8;
	public static final byte PRIMITIVE_ARRAY_RECORD_TAG = (byte) 0x20;
	public static final byte LONG_PRIMITIVE_ARRAY_RECORD_TAG = 7;
	
	private final DataOutputStream _out;
	private final PortableHeapDumpClassCache _classCache = new PortableHeapDumpClassCache();
	private final boolean _is32BitHash;
	/**
	 * The last address of anything written through the dump (used for calculating offsets)
	 */
	private long _lastAddress = 0;
	private boolean _closed = false;
	
	public PortableHeapDumpFormatter(DataOutputStream output,String version,boolean is64Bit, boolean is32BitHash) throws IOException
	{
		super(version,is64Bit);
		
		_out = output;
		_is32BitHash = is32BitHash;
		
		writeHeader();
	}

	private void writeHeader() throws IOException
	{
		PortableHeapDumpHeader header = new PortableHeapDumpHeader(_version,_is64Bit,_is32BitHash);
		
		header.writeHeapDump(_out);
	}
	
	private void checkClosed()
	{   
		if(_closed) {
			throw new UnsupportedOperationException("Dump is closed");
		}
	}

	public void addClass(long address, String name, long superClassAddress,
			int size, long instanceSize, int hashCode, ReferenceIterator references) throws IOException
	{
		checkClosed();
		
		ClassRecord record = new ClassRecord(address,_lastAddress, name,superClassAddress,instanceSize, hashCode, _is64Bit, filterNullReferences(references),_is32BitHash);
		
		record.writeHeapDump(_out);
		
		_lastAddress = address;
	}

	public void addObject(long address, long classAddress, String className,
			int size, int hashCode, ReferenceIterator references) throws IOException
	{
		ObjectRecord record = ObjectRecord.getObjectRecord(address, _lastAddress, classAddress, hashCode, filterNullReferences(references), _classCache, _is64Bit,_is32BitHash);

		record.writeHeapDump(_out);
		
		_lastAddress = address;
	}

	public void addObjectArray(long address, long arrayClassAddress,
			String arrayClassName, long elementClassAddress,
			String elementClassName, long size, int numberOfElements,
			int hashCode, ReferenceIterator references) throws IOException
	{
		ObjectArrayRecord record = new ObjectArrayRecord(address,_lastAddress,elementClassAddress,hashCode,numberOfElements,filterNullReferences(references),_is64Bit,size,_is32BitHash);
		
		record.writeHeapDump(_out);
		
		_lastAddress = address;
	}

	public void addPrimitiveArray(long address, long arrayClassAddress, int type, long size,
			int hashCode, int numberOfElements) throws IOException,
			IllegalArgumentException
	{
		PortableHeapDumpRecord record;
		if (_is32BitHash) { // JVM 2.6 and later, optional hashcode, write a primitive array record if no hashcode set
			if (hashCode == 0) {
				record = new PrimitiveArrayRecord(address,_lastAddress,type,numberOfElements,hashCode,_is64Bit, size, _is32BitHash);
			} else { // write a long primitive array record
				record = new LongPrimitiveArrayRecord(address,_lastAddress,type,numberOfElements,hashCode,_is64Bit, size, _is32BitHash);
			}
			record = new PrimitiveArrayRecord(address,_lastAddress,type,numberOfElements,hashCode,_is64Bit, size, _is32BitHash);
		} else { // JVM prior to 2.6, always write a primitive array record
			record = new PrimitiveArrayRecord(address,_lastAddress,type,numberOfElements,hashCode,_is64Bit, size, _is32BitHash);
		}
		record.writeHeapDump(_out);
		
		_lastAddress = address;
	}

	public void close() throws IOException
	{
		_closed = true;
		
		_out.writeByte(END_OF_DUMP_TAG);
		
		_out.close();
	}
	
	/**
	 * Removes null references from the reference iterator - as required by PHD spec.
	 * 
	 * Returned value wraps input, so operations on the returned value affect the state of input.
	 */
	private static ReferenceIterator filterNullReferences(final ReferenceIterator input) 
	{
		return new ReferenceIterator()
		{

			private Long next;
			
			public boolean hasNext()
			{
				while(next == null && input.hasNext()) {
					Long potential = input.next();
					
					if(potential.longValue() != 0) {
						next = potential;
					}
				}
				
				return next != null;
			}
			
			public Long next()
			{
				if(! hasNext()) {
					return null;
				}
				
				Long toReturn = next;
				next = null;
				
				return toReturn;
			}

			public void reset()
			{
				input.reset();
				next = null;
			}
			
		};
	}
}
