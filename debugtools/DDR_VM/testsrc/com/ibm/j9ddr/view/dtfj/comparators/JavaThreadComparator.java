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

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaThreadComparator extends DTFJComparator {

	public static final int IMAGE_THREAD = 1;
	public static final int JNI_ENV = 2;
	public static final int NAME = 4;
	public static final int OBJECT = 8;
	public static final int PRIORITY = 16;
	public static final int STACK_FRAMES = 32;
	public static final int STACK_SECTIONS = 64;
	public static final int STATE = 128;
	
	// getImageThread()
	// getJNIEnv()
	// getName()
	// getObject()
	// getPriority()
	// getStackFrames()
	// getStackSections()
	// getState()
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaThread ddrJavaThread = (JavaThread) ddrObject;
		JavaThread jextraJavaThread	= (JavaThread) jextractObject;
		
		// getImageThread()
		if ((members & IMAGE_THREAD) != 0)
			new ImageThreadComparator().testComparatorEquals(ddrJavaThread, jextraJavaThread, "getImageThread");
		
		// getJNIEnv()
		if ((members & JNI_ENV) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaThread, jextraJavaThread, "getJNIEnv");
		
		// getName()
		if ((members & NAME) != 0)
			testJavaEquals(ddrJavaThread, jextraJavaThread, "getName");
		
		// getObject()
		if ((members & OBJECT) != 0)
			new JavaObjectComparator().testComparatorEquals(ddrJavaThread, jextraJavaThread, "getObject");
		
		// getPriority()
		if ((members & PRIORITY) != 0)
			testJavaEquals(ddrJavaThread, jextraJavaThread, "getPriority");
		
		// getStackFrames()
		if ((members & STACK_FRAMES) != 0)
			new JavaStackFrameComparator().testComparatorIteratorEquals(ddrJavaThread, jextraJavaThread, "getStackFrames", JavaStackFrame.class);
		
		// getStackSections()
		if ((members & STACK_SECTIONS) != 0)
			new ImageSectionComparator().testComparatorIteratorEquals(ddrJavaThread, jextraJavaThread, "getStackSections", ImageSection.class);
		
		// getState()
		if ((members & STATE) != 0)
			testJavaEquals(ddrJavaThread, jextraJavaThread, "getState");
	}

	// JEXTRACT: Appears to be throwing DataUnavailable for getImageThread() for all JavaThreads I can find
	public int getDefaultMask() {
		return ~IMAGE_THREAD;
	}
}
