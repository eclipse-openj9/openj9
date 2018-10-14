/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump.classic;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.Writer;

import com.ibm.jvm.dtfjview.heapdump.HeapDumpFormatter;
import com.ibm.jvm.dtfjview.heapdump.LongArrayReferenceIterator;
import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;

/**
 * A formatter for classic heap dumps.
 * 
 * Format specification is in the diagnostic guide.
 * 
 * @author andhall
 *
 */
public class ClassicHeapDumpFormatter extends HeapDumpFormatter
{
	private static final String EOF_HEADER = "// EOF: Total 'Objects',Refs(null) :";
	private static final String BREAKDOWN_HEADER = "// Breakdown - Classes:";
	private static final String VERSION_HEADER = "// Version: ";
	
	// Using a BufferedWriter rather than a PrintWriter because a BufferedWriter preserves IOExceptions while providing newLine().
	private final BufferedWriter _out;
	private int _classCount = 0;
	//This includes objects + objectArrays + primitiveArrays + classes
	private int _allObjectCount = 0;
	private int _objectArraysCount = 0;
	private int _primitiveArraysCount = 0;
	private int _referenceCount = 0;
	private int _nullReferenceCount = 0;
	private boolean _closed = false;
	
	private boolean is64bit = false;
	
	public ClassicHeapDumpFormatter(Writer out,String version,boolean is64bit) throws IOException
	{
		super(version,is64bit);
		_out = new BufferedWriter(out);
		this.is64bit = is64bit;
		
		writeHeader();
	}
	
	private void writeHeader() throws IOException
	{
		_out.write(VERSION_HEADER + _version);
		_out.newLine();
	}

	private void writeReferences(ReferenceIterator references) throws IOException
	{
		if(references == null) {
			return;
		}
		references.reset();
		if(! references.hasNext()) {
			_out.write("\t");
			_out.newLine();
			return;
		}
		
		StringBuffer referenceString = new StringBuffer();
		referenceString.append("\t");
		
		boolean first = true;
		
		while(references.hasNext()) {
			Long thisRef = references.next();
			
			if(first) {
				first = false;
			} else {
				referenceString.append(" ");
			}
			
			referenceString.append(toHexString(thisRef.longValue()));
			
			_referenceCount++;
			if(thisRef.longValue() == 0) {
				_nullReferenceCount++;
			}
		}
		
		referenceString.append(" "); // the classic heapdump from the JVM has a blank on the end of each line 
		_out.write(referenceString.toString());
		_out.newLine();
	}
	
	private String toHexString(long value)
	{
		//Fast-path for 0
		if(value == 0) {
			return "0x0";
		}
		
		if (is64bit) {			
			return "0x" + String.format("%016X",value);
		} else {			
			return "0x" + String.format("%08X",value);
		}
	}
	
	public void addClass(long address, String name, long superClassAddress,
			int size, long instanceSize, int hashCode, ReferenceIterator references)
			throws IOException
	{
		checkClosed();
		
		_out.write(toHexString(address) + " [" + size + "] CLS " + name);
		_out.newLine();
		
		writeReferences(references);
		
		_classCount++;
		_allObjectCount++;
	}

	private void checkClosed()
	{
		if(_closed) {
			throw new UnsupportedOperationException("This dump has been closed");
		}
	}

	private void writeEntry(long address, long size,String className,ReferenceIterator references) throws IOException
	{
		_out.write(toHexString(address) + " [" + size + "] OBJ " + className);
		_out.newLine();

		writeReferences(references);
		
		_allObjectCount++;
	}
	
	public void addObject(long address, long classAddress, String className,
			int size, int hashCode, ReferenceIterator references)
			throws IOException
	{
		checkClosed();
				
		writeEntry(address,size,className,references);
	}

	/**
	 * Wraps the original reference iterator to add an artificial reference to the list.
	 */
	private static ReferenceIterator addReference(final ReferenceIterator original,
			final long extra)
	{   
		return new ReferenceIterator(){

			private boolean _returnedExtra = false;
			
			public boolean hasNext()
			{
				if(_returnedExtra) {
					return original.hasNext();
				} else {
					return true;
				}
			}

			public Long next()
			{
				if(_returnedExtra) {
					return original.next();
				} else {
					_returnedExtra = true;
					return Long.valueOf(extra);
				}
			}

			public void reset()
			{
				original.reset();
				_returnedExtra = false;
			}};
	}

	public void addObjectArray(long address, long arrayClassAddress,
			String arrayClassName, long elementClassAddress,
			String elementClassName, long size, int numberOfElements,
			int hashCode, ReferenceIterator references) throws IOException
	{
		checkClosed();
		
		writeEntry(address,size,arrayClassName,references);
		
		_objectArraysCount++;
	}

	public void addPrimitiveArray(long address, long arrayClassAddress, int type, long size,
			int hashCode, int numberOfElements) throws IOException,
			IllegalArgumentException
	{
		checkClosed();
		
		ReferenceIterator references = new LongArrayReferenceIterator(new long[]{});
		
		writeEntry(address,size,getPrimitiveArrayName(type),references);
		
		_primitiveArraysCount++;
	}

	private String getPrimitiveArrayName(int type)
	{
		switch(type) {
		case 0:
			return "[Z";
		case 1:
			return "[C";
		case 2:
			return "[F";
		case 3:
			return "[D";
		case 4:
			return "[B";
		case 5:
			return "[S";
		case 6:
			return "[I";
		case 7:
			return "[J";
		default:
			throw new IllegalArgumentException("Unknown primitive type code: " + type);
		}
	}

	public void close() throws IOException
	{        
		printBreakdown();
		
		printEOFSummary();
		
		_out.close();
		
		_closed = true;
	}

	private void printEOFSummary() throws IOException
	{
		StringBuffer buffer = new StringBuffer(EOF_HEADER);
		
		buffer.append(_allObjectCount);
		buffer.append(",");
		buffer.append(_referenceCount);
		buffer.append("(");
		buffer.append(_nullReferenceCount);
		buffer.append(")");
		
		_out.write(buffer.toString());
		_out.newLine();
	}

	private void printBreakdown() throws IOException
	{
		int objectsCount = ((_allObjectCount - _primitiveArraysCount) - _objectArraysCount) - _classCount;
		
		StringBuffer buffer = new StringBuffer(BREAKDOWN_HEADER);
		buffer.append(_classCount);
		
		buffer.append(", Objects:");
		buffer.append(objectsCount);
		
		buffer.append(", ObjectArrays:");
		buffer.append(_objectArraysCount);
		
		buffer.append(", PrimitiveArrays:");
		buffer.append(_primitiveArraysCount);
		
		_out.write(buffer.toString());
		_out.newLine();
	}

}
