/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.junit.framework;

import static org.junit.Assert.fail;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.VMDataFactory;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IProcess;

public abstract class BootstrapJUnitTest implements IBootstrapRunnable {

	private static IVMData vmData;

	@BeforeClass
	public static void beforeClass() {

		try {
			String coreFileName = System.getProperty("ddr.core.file.name");
			if (coreFileName == null) {
				fail("Did not specify core file name with -Dddr.core.file.name");
			}

			ICore core = CoreReader.readCoreFile(coreFileName);

			List<IAddressSpace> addressSpaces = new ArrayList<IAddressSpace>(core.getAddressSpaces());
			
			for (IAddressSpace thisAS : addressSpaces) {
				for (IProcess thisProc : thisAS.getProcesses()) {
					try {
						vmData = VMDataFactory.getVMData(thisProc);
						return;
					} catch (IOException ex) {
						//This happens if we can't find a JVM or a blob. Keep looking
					}
				}
			}
			
			fail("Couldn't initialize VMData");
		} catch (FileNotFoundException e) {
			fail("Could not initialize VMData: " + e.getMessage());
		} catch (IOException e) {
			fail("Could not initialize VMData: " + e.getMessage());
		}
	}

	@Test
	public void runTests() throws Exception {
		int testCount = 0;
		System.out.println(String.format("Running tests in: %s", getClass().getName()));
		Method[] methods = this.getClass().getMethods();
		for (int i = 0; i < methods.length; i++) {
			Method method = methods[i];
			if (method.getName().endsWith("Impl")) {
				testCount++;
				vmData.bootstrap(getClass().getName(), new Object[] { method.getName() });
			}
		}
		
		if (testCount == 0) {
			fail(String.format("No tests found.  Define some *Impl methods"));
		} else {
			System.out.println(String.format("Ran %s tests.", testCount));
		}
	}

	public void run(IVMData vmData, Object[] userData) {
		try {
			Method testMethod = this.getClass().getMethod((String) userData[0],
					new Class[] {});
			testMethod.invoke(this, new Object[] {});
		} catch (SecurityException e) {
			fail(e.getMessage());
		} catch (IllegalArgumentException e) {
			fail(e.getMessage());
		} catch (NoSuchMethodException e) {
			fail("Can not find test test message " + userData[0]);
		} catch (IllegalAccessException e) {
			fail(e.getMessage());
		} catch (InvocationTargetException e) {
			Throwable cause = e.getTargetException();
			if (cause instanceof Error) {
				throw (Error) e.getTargetException();
			} else if (cause instanceof RuntimeException) {
				throw (RuntimeException) cause;
			} else {
				cause.printStackTrace();
				fail(cause.getMessage());
			}
		}
	}
}
