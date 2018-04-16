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
package jnurlcldr.shared.nonexistjar;

import java.io.File;

public class A_Main {

	public static final int RESULT=3;
	
	static class Data {
		public int getInt() {
			System.out.println("ROGUE A");
			return new B_Dummy().getInt();
		}
	}
	
	public static void main(String[] args) {
		if (args.length > 0 && args[0].equals("ROGUE")) {
			int i = new C_DoesNothing().getInt(); /* i == 0 */
			if (changeJarFileName()) {
				System.out.println("Successfully renamed B_.jar to B.jar");
				System.out.println("Result="+(new Data().getInt()+i));
			} else {
				System.out.println("SETUP ERROR: Error renaming Jar File");
			}
		} else {
			System.out.println("Result="+(new Data().getInt()));
		}
	}
	
	private static boolean changeJarFileName() {
		File f = new File("B_.jar");
		File f2 = new File ("B.jar");
		if (f.isFile() && f.canWrite()) {
			return f.renameTo(f2);
		}
		return false;
	}
	
}
