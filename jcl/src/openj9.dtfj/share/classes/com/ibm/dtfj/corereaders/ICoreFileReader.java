/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.util.Iterator;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;

/**
 * @author jmdisher
 * The top-level abstraction for reading core files.  This is the set of methods which are required by DTFJ.
 */
public interface ICoreFileReader extends ResourceReleaser
{
	/**
	 * Used to extract OS-specific data.  Called with a builder which is a sort of factory which will create
	 * the required implementation-specific data structures exist solely above the layer of this project.
	 * 
	 * @param builder
	 */
	public void extract(Builder builder);
	
	/**
	 * @return An iterator of String object specifying names of additional files needed by the Dump
	 * 
	 * @see java.lang.String
	 */
	public Iterator getAdditionalFileNames();
	
	/**
	 * Creates a representation of the address space capable of reading data from memory as a flat address
	 * space even though it may be fragmented across regions of several files or transport media.
	 * 
	 * Note that this method is expected to be called several times and should always return the same instance.
	 * @return
	 */
	public IAbstractAddressSpace getAddressSpace();
	
	/**
	 * @return true if the core file is truncated, false otherwise
	 */
	public boolean isTruncated();
}
