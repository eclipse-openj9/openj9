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
package com.ibm.j9ddr.view.dtfj.comparators;

import static org.junit.Assert.assertEquals;

import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageAddressSpace;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class ImageAddressSpaceComparator extends DTFJComparator {
	
	public static final int ASID = 1;
	public static final int CURRENT_PROCESS = 2;
	public static final int IMAGE_SECTIONS = 4;
	public static final int PROCESSES = 8;
	
	J9DDRImageAddressSpace ddrImageAddressSpace;
	ImageAddressSpace jextractImageAddressSpace;
	
	// asid
	// getCurrentProcess()
	// getImageSections()
	// getProcesses()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		ddrImageAddressSpace = (J9DDRImageAddressSpace) ddrObject; 
		jextractImageAddressSpace = (ImageAddressSpace) jextractObject;

		// asid
		// asid is private but we know what the toString is
		if ((members & ASID) != 0) {
			int jextractASID = 0;
			String string = jextractImageAddressSpace.toString();
			if (string.startsWith("0x")) {
				jextractASID = Integer.parseInt(string.substring(2), 16);
			}
			assertEquals("ImageAddressSpace ASIDs do not match.", jextractASID, ddrImageAddressSpace.getID());
		}
		
		// getCurrentProcess()
		if ((members & CURRENT_PROCESS) != 0)
			//just test the primitive data components to avoid a descent into the other collections which are tested directly in ImageProcessTest
			new ImageProcessComparator().testComparatorEquals(ddrImageAddressSpace, jextractImageAddressSpace, "getCurrentProcess");
		
		// getImageSections()
		if ((members & IMAGE_SECTIONS) != 0)
			new ImageSectionComparator().testComparatorIteratorEquals(ddrImageAddressSpace, jextractImageAddressSpace, "getImageSections", ImageSection.class);
		
		// getProcesses()
		if ((members & PROCESSES) != 0)
			new ImageProcessComparator().testComparatorIteratorEquals(ddrImageAddressSpace, jextractImageAddressSpace, "getProcesses", ImageProcess.class);
	}
	
	public int getDefaultMask() {
		return ASID;
	}
}
