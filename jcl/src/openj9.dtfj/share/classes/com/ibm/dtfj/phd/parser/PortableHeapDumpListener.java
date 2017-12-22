/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.parser;

import com.ibm.dtfj.phd.util.LongEnumeration;

/**
 *  This interface is used to parse a heapdump in Phd format. It provides info about the
 *  contents of a heapdump in the form of callbacks with one call per object. Note that wherever
 *  an array of references appears that null references are not included.
 *  <p>
 *  Any exceptions thrown by the listener will be propagated back through the parse call.
 *  <p>
 *  <b>Note:</b> This interface was changed on 12 Oct 2005 to provide the ability to enumerate
 *  the references rather than returning an array of longs. This has been done for performance
 *  reasons after it was found that certain heapdumps contain huge object arrays.
 */

public interface PortableHeapDumpListener {
	/**
	 *  This call represents a normal object.
	 *  @param address      the address of the object
	 *  @param classAddress the address of the object's class object (which is dumped via classDump)
	 *  @param flags        flags associated with the object (currently unused)
	 *  @param hashCode     the object's hash code
	 *  @param refs         the enumeration of object references
	 *  @param instanceSize the instance size == PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE
	 */
	void objectDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, long instanceSize) throws Exception;
	/**
	 *  This call represents an array of objects.
	 *  @param address      the address of the array
	 *  @param classAddress the address of the class object for the objects in the array
	 *  @param flags        flags associated with the object (currently unused)
	 *  @param hashCode     the object's hash code
	 *  @param refs         the enumeration of object references
	 *  @param length       the true length of the array in terms of number of references. This includes null refs and so may be greater than refs.length.
	 *  @param instanceSize will be set for all object arrays with a version 6 PHD, otherwise == PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE
	 */
	void objectArrayDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, int length, long instanceSize) throws Exception;
	/**
	 *  This call represents a class object.
	 *  @param address      the address of the class object
	 *  @param superAddress the address of the superclass object
	 *  @param name         the name of the class
	 *  @param instanceSize the size of each instance (object) of this class
	 *  @param flags        flags associated with the object (currently unused)
	 *  @param hashCode     the object's hash code
	 *  @param refs         the enumeration of this class's static references. 
	 *  
	 */
	void classDump(long address, long superAddress, String name, int instanceSize, int flags, int hashCode, LongEnumeration refs) throws Exception;
	/**
	 *  This call represents a primitive array.
	 *  @param address      the address of the array
	 *  @param type         the type of the array elements as follows:
	 *      <table cellpadding="2" cellspacing="0" border="1">
	 *        <thead>
	 *          <tr>
	 *            <th align="left" valign="top">Value<br></th>
	 *            <th align="left" valign="top">Array type<br></th>
	 *          </tr>
	 *        </thead>
	 *        <tbody>
	 *          <tr>
	 *            <td valign="top">0<br></td>
	 *            <td valign="top">bool<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">1<br></td>
	 *            <td valign="top">char<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">2<br></td>
	 *            <td valign="top">float<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">3<br></td>
	 *            <td valign="top">double<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">4<br></td>
	 *            <td valign="top">byte<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">5<br></td>
	 *            <td valign="top">short<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">6<br></td>
	 *            <td valign="top">int<br></td>
	 *          </tr>
	 *          <tr>
	 *            <td valign="top">7<br></td>
	 *            <td valign="top">long<br></td>
	 *          </tr>
	 *        </tbody>
	 *      </table>
	 *  @param length       the number of elements in the array
	 *  @param flags        flags associated with the object (currently unused)
	 *  @param hashCode     the object's hash code
	 *  @param instanceSize will be set for primitive arrays with a version 6 PHD, otherwise == PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE
	 */
	void primitiveArrayDump(long address, int type, int length, int flags, int hashCode, long instanceSize) throws Exception;
}
