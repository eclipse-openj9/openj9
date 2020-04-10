/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders;

import java.io.IOException;
import java.util.Collection;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.memory.IAddressSpace;

/**
 * Interface representing core dump.
 * 
 * @author andhall
 */
public interface ICore
{
	public static final String PROCESSOR_COUNT_PROPERTY = "cpu.count";
	public static final String PROCESSOR_TYPE_PROPERTY = "cpu.type";
	public static final String PROCESSOR_SUBTYPE_PROPERTY = "cpu.subtype";
	
	public static final String SYSTEM_TYPE_PROPERTY = "system.type";
	public static final String SYSTEM_SUBTYPE_PROPERTY = "system.subtype";
	
	public static final String CORE_CREATE_TIME_PROPERTY = "core.creation.time";
	
	/**
	 * 
	 * @return Address spaces held in this core dump
	 */
	public Collection<? extends IAddressSpace> getAddressSpaces();

	/**
	 * This is the dump format expressed as a string e.g. elf or xcoff. It is
	 * recommended that this name is in lower case.
	 * 
	 * @return
	 */
	public String getDumpFormat();
	
	/**
	 * 
	 * @return Platform that created the dump
	 */
	public Platform getPlatform();
	
	/**
	 * 
	 * @return Property set for this core
	 */
	public Properties getProperties();
	
	/**
	 * Close the handle to the core file and release any resources
	 */
	public void close() throws IOException;
}
