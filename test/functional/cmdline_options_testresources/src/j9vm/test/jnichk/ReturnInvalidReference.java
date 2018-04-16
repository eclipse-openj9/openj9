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
package j9vm.test.jnichk;

/**
 * 
 * @author hickeng
 *
 * See CMVC 123202
 * Test to ensure that Xcheck:jni catches invalid references on return 
 * 
 * To run:
 * java -Xcheck:jni j9vm.test.jnichk.ReturnInvalidReference
 */
public class ReturnInvalidReference extends Test {

	public static void main(String[] args) {
		Object ref;

		System.err.println("validLocalRef: shouldn't generate Xcheck error:");
		System.err.flush();		
		ref = validLocalRef();
		ref.getClass();
		System.err.println("validLocalRef: done");

		System.err.println("validGlobalRef: shouldn't generate Xcheck error:");
		System.err.flush();		
		ref = validGlobalRef();
		ref.getClass();
		System.err.println("validGlobalRef: done");

		System.err.println("explicitReturnOfNull: shouldn't generate Xcheck error:");
		System.err.flush();		
		ref = explicitReturnOfNull();
		System.err.println("explicitReturnOfNull: done");
		
		System.err.println("deletedLocalRef: should generate Xcheck error:");
		System.err.flush();
		ref = deletedLocalRef();
		System.err.println("deletedLocalRef: done");
		
		System.err.println("deletedGlobalRef: should generate Xcheck error:");
		System.err.flush();
		ref = deletedGlobalRef();
		System.err.println("deletedGlobalRef: done");
		
		System.err.println("localRefFromPoppedFrame: should generate Xcheck error:");
		System.err.flush();
		ref = localRefFromPoppedFrame();
		System.err.println("localRefFromPoppedFrame: done");
	}
	
	private static native Object deletedLocalRef();
	private static native Object validLocalRef();
	private static native Object deletedGlobalRef();
	private static native Object validGlobalRef();
	private static native Object localRefFromPoppedFrame();
	private static native Object explicitReturnOfNull();
}
