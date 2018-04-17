package j9vm.test.thread;

/*******************************************************************************
 * Copyright (c) 2008, 2012 IBM Corp. and others
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

public class TestNatives {
	static final int TESTNATIVES_SCHED_OTHER = 1;
	static final int TESTNATIVES_SCHED_RR = 2;
	static final int TESTNATIVES_SCHED_FIFO = 3;
	
	static final String NATIVE_LIBRARY_NAME = "j9thrtestnatives" + System.getProperty("com.ibm.oti.vm.library.version");
	static {
		try {
			System.loadLibrary(NATIVE_LIBRARY_NAME);
		} catch (UnsatisfiedLinkError e) {
			System.out.println("No natives for j9vm.test.thread.TestNatives");
			e.printStackTrace();
		}
	}	

	public static native int getSchedulingPolicy(Thread javaThread);
	public static native int getSchedulingPriority(Thread javaThread);
	public static native int getGCAlarmSchedulingPolicy();
	public static native int getGCAlarmSchedulingPriority();
}
