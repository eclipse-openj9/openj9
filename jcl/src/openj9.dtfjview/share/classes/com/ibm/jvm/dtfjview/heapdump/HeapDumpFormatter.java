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
package com.ibm.jvm.dtfjview.heapdump;

import java.io.IOException;

/**
 * Abstract class to decouple the formatting of heapdumps from the walking of
 * the internal data structures that hold the source data.
 * 
 * @author andhall
 * 
 */
public abstract class HeapDumpFormatter
{
	protected final String _version;   
	protected final boolean _is64Bit;

	/**
	 * Constructor
	 * 
	 * @param version JVM version information to be included in heapdump header
	 * @param is64Bit True if we are dumping a 64 bit image, false otherwise
	 */
	protected HeapDumpFormatter(String version,boolean is64Bit)
	{
		this._version = version;
		this._is64Bit = is64Bit;
	}

	/**
	 * Adds a class to the heapdump.
	 * 
	 * @param address
	 *            Address of class
	 * @param name
	 *            Name of class
	 * @param superClassAddress
	 *            Address of superclass
	 * @param size
	 *            Size of class, bytes
	 * @param instanceSize
	 *            Size of objects of this class
	 * @param hashCode
	 *            Hashcode
	 * @param references
	 *            Iterator of Long addresses (some of which may be null)
	 * @throws IOException
	 */
	public abstract void addClass(long address, String name,
			long superClassAddress, int size, long instanceSize, int hashCode, ReferenceIterator references)
			throws IOException;

	/**
	 * Adds an object to the heapdump.
	 * 
	 * @param address
	 *            Address of object
	 * @param classAddress
	 *            Address of class of object
	 * @param className
	 *            Name of class of object
	 * @param size
	 *            Size of object, bytes
	 * @param hashCode
	 *            Hashcode of object
	 * @param references
	 *            Iterator of Long addresses (some of which may be null)
	 * @throws IOException
	 */
	public abstract void addObject(long address, long classAddress,
			String className, int size, int hashCode, ReferenceIterator references)
			throws IOException;

	/**
	 * Adds a primitive array object to the heapdump
	 * 
	 * Type code is: 0 - bool, 1 - char, 2 - float, 3 - double, 4 - byte, 5 - short, 6 -
	 * int, 7 - long
	 * 
	 * @param address
	 *            Address of object
	 * @param arrayClassAddress
	 *           Address of primitive array class
	 * @param type
	 *            Type code - see above
	 * @param size
	 *            Size of object, bytes.
	 *            Since version 6 PHD, instance size is in the PHD and can be larger than max signed int so pass as a long
	 * @param hashCode
	 *            Hashcode of object
	 * @param numberOfElements
	 *            Number of elements in the array
	 * @throws IOException
	 * @throws IllegalArgumentException
	 *             if invalid type is supplied
	 */
	public abstract void addPrimitiveArray(long address, long arrayClassAddress, int type, long size,
			int hashCode, int numberOfElements) throws IOException,
			IllegalArgumentException;

	/**
	 * Adds an object array to the heapdump.
	 * 
	 * @param address
	 *            Address of object
	 * @param arrayClassAddress
	 *            Address of class of array object
	 * @param arrayClassName
	 *            Name of class of array object
	 * @param elementClassAddress
	 *            Address of class of elements
	 * @param elementClassName
	 *            Name of class of elements
	 * @param size
	 *            Size of object, bytes
	 *            Since version 6 PHD, instance size is in the PHD and can be larger than max signed int so pass as a long
	 * @param numberOfElements
	 *            number of elements in the array
	 * @param hashCode
	 *            Hashcode of object
	 * @param references
	 *            Iterator of Long addresses (some of which may be null)
	 * @throws IOException
	 */
	public abstract void addObjectArray(long address, long arrayClassAddress,
			String arrayClassName, long elementClassAddress,
			String elementClassName, long size, int numberOfElements,
			int hashCode, ReferenceIterator references) throws IOException;
	
	/**
	 * Closes the heapdump
	 * 
	 * @throws IOException
	 */
	public abstract void close() throws IOException;
}
