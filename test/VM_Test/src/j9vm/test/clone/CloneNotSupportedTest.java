/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.clone;


public class CloneNotSupportedTest {

	// An inner class declaration 
  	static class T2 extends Object
	{
		Object cl() throws CloneNotSupportedException{
			return super.clone();
		}
	}

	// Use my inner class in the clinit
	T2 t2 = new  T2();


	public void run() {
		try {
			t2.cl();
			System.out.println("did not throw an exception");
			throw new RuntimeException();
		} catch(CloneNotSupportedException e) {
			System.out.println("caught CloneNotSupportedException");
		}catch(Exception e) {
			System.out.println("caught something unexpected:");
			e.printStackTrace();
			throw new RuntimeException();
		}

	}
	
	public static void main(String[] args) { new CloneNotSupportedTest().run(); }
}
