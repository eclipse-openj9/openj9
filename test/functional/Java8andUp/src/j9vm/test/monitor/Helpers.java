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
/*
 * Created on Jun 6, 2006
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.monitor;

/**
 * 
 * @author PBurka
 *
 * This class provides a family of native methods for manipulating an
 * object's monitor. They allow us to test situations which should not
 * occur in a normal Java program.
 * 
 * They also allow us to directly manipulate lock reservations without
 * relying on the JIT. On systems which don't support reservations
 * monitorReserve is a no-op.
 */
public class Helpers {

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {
			System.out.println("No natives for JNI tests");
		}
	}
	
	public static native int getLastReturnCode();
	
	public static native int monitorEnter(Object obj);
	
	public static native int monitorExit(Object obj);
	
	public static native int monitorExitWithException(Object obj, Throwable throwable);
	
	public static native void monitorReserve(Object obj);
	
}
