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

import java.io.IOException;

import java.util.Iterator;
import java.util.List;
import java.util.Properties;

public interface Builder {

	public Object buildProcess(Object addressSpace, String pid, String commandLine, Properties environment, Object currentThread, Iterator threads, Object executable, Iterator libraries, int addressSize);

	public Object buildAddressSpace(String name, int id);

	public Object buildRegister(String name, Number value);

	public Object buildStackSection(Object addressSpace, long stackStart, long stackEnd);

	public Object buildThread(String name, Iterator registers, Iterator stackSections, Iterator stackFrames, Properties properties, int signalNumber);

	public Object buildModuleSection(Object addressSpace, String name, long imageStart, long imageEnd);

	public Object buildModule(String name, Properties properties, Iterator sections, Iterator symbols, long startAddress);

	public Object buildStackFrame(Object addressSpace, long stackBasePointer, long pc);

	public Object buildSymbol(Object addressSpace, String functionName, long relocatedFunctionAddress);

	public Object buildCorruptData(Object addressSpace, String message, long address);

	public ClosingFileReader openFile(String filename) throws IOException;
	
	public long getEnvironmentAddress();

	public long getValueOfNamedRegister(List registers, String string);

	/**
	 * Called to inform the builder that the executable data cannot be trusted.
	 * Note that this also set the libraries unavailable as a side-effect
	 * 
	 * @param description
	 */
	public void setExecutableUnavailable(String description);

	public void setCPUType(String cpuType);

	public void setCPUSubType(String subType);

	public void setCreationTime(long millis);

	public void setOSType(String osType);
}
