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

import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaMonitorComparator extends DTFJComparator {

	public static final int ENTER_WAITERS = 1;
	public static final int ID = 2;
	public static final int NAME = 4;
	public static final int NOTIFY_WAITERS = 8;
	public static final int OBJECT = 16;
	public static final int OWNER = 32;
	
	// getEnterWaiters
	// getID
	// getName
	// getNotifyWaiters
	// getObject
	// getOwner
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaMonitor ddrJavaMonitor = (JavaMonitor) ddrObject;
		JavaMonitor jextractJavaMonitor = (JavaMonitor) jextractObject;
		
		JavaThreadComparator javaThreadComparator = new JavaThreadComparator();

		// getEnterWaiters
		if ((ENTER_WAITERS & members) != 0)
			javaThreadComparator.testComparatorIteratorEquals(ddrJavaMonitor, jextractJavaMonitor, "getEnterWaiters", JavaThread.class, JavaThreadComparator.JNI_ENV);
		
		// getID
		if ((ID & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaMonitor, jextractJavaMonitor, "getID");
		
		// getName()
		if ((NAME & members) != 0)
			testJavaEquals(ddrJavaMonitor, jextractJavaMonitor, "getName");
		
		// getNotifyWaiters
		if ((NOTIFY_WAITERS & members) != 0)
			javaThreadComparator.testComparatorIteratorEquals(ddrJavaMonitor, jextractJavaMonitor, "getNotifyWaiters", JavaThread.class, JavaThreadComparator.JNI_ENV);
		
		// getObject
		if ((OBJECT & members) != 0)
			new JavaObjectComparator().testComparatorEquals(ddrJavaMonitor, jextractJavaMonitor, "getObject");
		
		// getOwner
		if ((OWNER & members) != 0)
			javaThreadComparator.testComparatorEquals(ddrJavaMonitor, jextractJavaMonitor, "getOwner", JavaThreadComparator.JNI_ENV);
	}

	public int getDefaultMask() {
		return ID;
	}
}
