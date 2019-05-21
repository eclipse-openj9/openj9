/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class ImageProcessComparator extends DTFJComparator {

	public static final int COMMAND_LINE = 1;
	public static final int CURRENT_THREAD = 2;
	public static final int ENVIRONMENT = 4;
	public static final int EXECUTABLE = 8;
	public static final int ID = 16;
	public static final int LIBRARIES = 32;
	public static final int POINTER_SIZE = 64;
	public static final int RUNTIMES = 128;
	public static final int SIGNAL_NAME = 256;
	public static final int SIGNAL_NUMBER = 512;
	public static final int THREADS = 1024;
	
	// getCommandLine()
	// getCurrentThread()
	// getEnvironment()
	// getExecutable()
	// getID()
	// getLibraries()
	// getPointerSize()
	// getRuntimes()
	// getSignalName()
	// getSignalNumber()
	// getThreads()
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		ImageProcess ddrImageProcess = (ImageProcess) ddrObject;
		ImageProcess jextractImageProcess = (ImageProcess) jextractObject;
		
		ImageThreadComparator imageThreadComparator = new ImageThreadComparator();
		ImageModuleComparator imageModuleComparator = new ImageModuleComparator();
		
		// getCommandLine()
		if ((COMMAND_LINE & members) != 0) {
			log.fine("Testing getCommandLine()");
			testJavaEquals(ddrImageProcess, jextractImageProcess, "getCommandLine");
		}
		
		// getCurrentThread()
		if ((CURRENT_THREAD & members) != 0) {
			log.fine("Testing getCurrentThread()");
			imageThreadComparator.testComparatorEquals(ddrImageProcess, jextractImageProcess, "getCurrentThread");
		}
		
		// getEnvironment()
		if ((ENVIRONMENT & members) != 0) { 
			log.fine("Testing getEnvironment()");
			testPropertiesEquals(ddrImageProcess, jextractImageProcess, "getEnvironment");
		}
		
		// getExecutable()
		if ((EXECUTABLE & members) != 0) { 
			log.fine("Testing getExecutable()");
			imageModuleComparator.testComparatorEquals(ddrImageProcess, jextractImageProcess, "getExecutable");
		}
		
		// getID()
		if ((ID & members) != 0) {
			log.fine("Testing getID()");
			testJavaEquals(ddrImageProcess, jextractImageProcess, "getID");
		}
		
		// getLibraries()
		if ((LIBRARIES & members) != 0) { 
			log.fine("Testing getLibraries()");
			imageModuleComparator.testComparatorIteratorEquals(ddrImageProcess, jextractImageProcess, "getLibraries", ImageModule.class);
		}

		// getPointerSize();
		if ((POINTER_SIZE & members) != 0) { 
			log.fine("Testing getPointerSize()");
			testJavaEquals(ddrImageProcess, jextractImageProcess, "getPointerSize");
		}
		
		// getRuntimes()  (Heavy Handed?)
		if ((RUNTIMES & members) != 0) { 
			log.fine("Testing getRuntimes()");
			new JavaRuntimeComparator().testComparatorIteratorEquals(ddrImageProcess, jextractImageProcess, "getRuntimes", JavaRuntime.class);
		}
		
		// getSignalName()
		if ((SIGNAL_NAME & members) != 0) { 
			log.fine("Testing getSignalName()");
			testJavaEquals(ddrImageProcess, jextractImageProcess, "getSignalName");
		}
		
		// getSignalNumber()
		if ((SIGNAL_NUMBER & members) != 0) {
			log.fine("Testing getSignalNumber()");
			testJavaEquals(ddrImageProcess, jextractImageProcess, "getSignalNumber");
		}
		
		// getThreads();
		if ((THREADS & members) != 0) { 
			log.fine("Testing getThreads()");
			imageThreadComparator.testComparatorIteratorEquals(ddrImageProcess, jextractImageProcess, "getThreads", ImageThread.class);
		}
	}

	public int getDefaultMask() {
		return COMMAND_LINE | ID | POINTER_SIZE;
	}
}
