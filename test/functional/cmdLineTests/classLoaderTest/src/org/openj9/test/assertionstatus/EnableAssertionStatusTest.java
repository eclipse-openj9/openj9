/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

package org.openj9.test.assertionstatus;

/* The intention of this test is to verify whether these public methods (overridden by
 * the inner class loader) are not directly invoked during the initialization so as to
 * match the behavior of RI. 
 */

class SubClassLoader extends ClassLoader {
	
	class InnerClassLoader extends SubClassLoader {
		private boolean setDefaultAssertionStatusMethodCalled;
		private boolean setPackageAssertionStatusMethodCalled;
		private boolean setClassAssertionStatusMethodCalled;
		
		public boolean isAssertionStatusPublicMethodCalled() {
			return (setDefaultAssertionStatusMethodCalled || setPackageAssertionStatusMethodCalled || setClassAssertionStatusMethodCalled);
		}
		
		public void setDefaultAssertionStatus(boolean enable) {
			setDefaultAssertionStatusMethodCalled = true;
			new Throwable().printStackTrace();
		}
		
		public void setPackageAssertionStatus(String pname, boolean enable) {
			setPackageAssertionStatusMethodCalled = true;
			new Throwable().printStackTrace();
		}
		
		public void setClassAssertionStatus(String cname, boolean enable) {
			setClassAssertionStatusMethodCalled = true;
			new Throwable().printStackTrace();
		}
	}
}

public class EnableAssertionStatusTest {
	
	public static void main(String[] args) throws Throwable {
		SubClassLoader subCldr = new SubClassLoader();
		SubClassLoader.InnerClassLoader innerCldr = subCldr.new InnerClassLoader();
		
		if (innerCldr.isAssertionStatusPublicMethodCalled()) {
			System.out.println("EnableAssertionStatusTest FAILED");
		} else {
			System.out.println("EnableAssertionStatusTest PASSED");
		}
	}
}
