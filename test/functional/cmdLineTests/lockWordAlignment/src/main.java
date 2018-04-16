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

import sun.misc.Unsafe;

class main {

	public static void main(String[] args) throws Throwable {
		Unsafe u = Unsafe.getUnsafe();
		long offset;

		Class cl = Class.forName(args[0]);

		int alignment = com.ibm.oti.vm.VM.FJ9OBJECT_SIZE;
		
		try {
			offset = u.objectFieldOffset(cl.getDeclaredField("i"));
			System.out.println(offset);
			if (offset % 4 != 0) {
				System.out.println("error: int is not aligned");
			}
		} catch (Exception e) {
	
		}

		try {
			offset = u.objectFieldOffset(cl.getDeclaredField("l"));
			System.out.println(offset);
			if (offset % 8 != 0) {
				System.out.println("error: long is not aligned");
			}
		} catch (Exception e) {

		}

		try {
			offset = u.objectFieldOffset(cl.getDeclaredField("o"));
			System.out.println(offset);
			if (offset % alignment != 0) {
				System.out.println("error: object is not aligned");
			}
		} catch (Exception e) {

		}
		
		try {
			offset = u.objectFieldOffset(cl.getDeclaredField("d"));
			System.out.println(offset);
			if (offset % 8 != 0) {
				System.out.println("error: double is not aligned");
			}
		} catch (Exception e) {

		}
	}
}
