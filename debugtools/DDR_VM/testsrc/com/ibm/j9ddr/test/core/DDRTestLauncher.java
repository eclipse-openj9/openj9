package com.ibm.j9ddr.test.core;
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.VMDataFactory;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;

public class DDRTestLauncher {
	
	public static void main(String[] args) throws Exception {
		// This code would be located in the DTFJ ImageFactory
		String testName = args[0];
		String coreFileName = args[1];
		
		List<String> remainingArguments = new LinkedList<String>();
		
		for (int i=2;i<args.length;i++) {
			remainingArguments.add(args[i]);
		}
		
		ICore core = CoreReader.readCoreFile(coreFileName);

		List<IAddressSpace> addressSpaces = new ArrayList<IAddressSpace>(core.getAddressSpaces());
		IVMData aVMData = VMDataFactory.getVMData(addressSpaces.get(0).getProcesses().iterator().next());
		aVMData.bootstrap(testName, remainingArguments);
	}
}
