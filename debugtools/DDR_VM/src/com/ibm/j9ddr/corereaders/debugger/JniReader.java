/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.debugger;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.AbstractCoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ICoreFileReader;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryImageInputStream;
import com.ibm.j9ddr.corereaders.memory.IProcess;

public class JniReader extends AbstractCoreReader implements ICoreFileReader{
	private final Properties properties = new Properties();
	private List<IAddressSpace> addressSpaces;
	
	public JniReader() throws IOException {
		setReader(new IMemoryImageInputStream(new JniSearchableMemory(), 0));
	}
	public ICore processDump(String path) throws InvalidDumpFormatException, IOException {
			return this;
	}

	public DumpTestResult testDump(String path) throws IOException {
		return DumpTestResult.RECOGNISED_FORMAT;
	}

	public DumpTestResult testDump(ImageInputStream in) throws IOException {
		return DumpTestResult.RECOGNISED_FORMAT;
	}
	/**
	 * This method creates JNI address space and puts it into collection instance. 
	 * 
	 * @return Collection of address spaces
	 */
	public Collection<? extends IAddressSpace> getAddressSpaces() {
		if (null == addressSpaces) {			
			addressSpaces = new ArrayList<IAddressSpace>(1);
			JniAddressSpace addressSpace = new JniAddressSpace(0, this);
			Iterator<? extends IProcess> ite = addressSpace.getProcesses().iterator();
			if (ite.hasNext()) {
				IProcess process = ite.next();
				/* This is sanity check. It should always be the JniProcess instance */
				if (process instanceof JniProcess) {
					JniProcess jniProcess = (JniProcess) process;
					/* Set ICore.PROCESSOR_TYPE_PROPERTY in the properties by checking the target architecture */
					int targetArchitecture = jniProcess.getTargetArchitecture();
					switch (targetArchitecture) {
					case JniProcess.TARGET_TYPE_X86_32:
						properties.setProperty(ICore.PROCESSOR_TYPE_PROPERTY, "x86");
						break;
					case JniProcess.TARGET_TYPE_X86_64:
						properties.put(ICore.PROCESSOR_TYPE_PROPERTY, "amd64");
						break;
					case JniProcess.TARGET_TYPE_S390_31:
					case JniProcess.TARGET_TYPE_S390_64:
						properties.put(ICore.PROCESSOR_TYPE_PROPERTY, "s390");
						break;
					case JniProcess.TARGET_TYPE_PPC_32:
					case JniProcess.TARGET_TYPE_PPC_64:
						properties.put(ICore.PROCESSOR_TYPE_PROPERTY, "ppc");
						break;
					default: 
						/* Do Nothing */
						
					}
					
				}
			}
			addressSpaces.add(addressSpace);
		}
		return addressSpaces;
	}

	public native String getDumpFormat();

	public Platform getPlatform() {
		return JniNatives.getPlatform();
	}

	public Properties getProperties() {
		return properties;
	}

	public ICore processDump(ImageInputStream in) throws InvalidDumpFormatException, IOException {
		return this;
	}
}
