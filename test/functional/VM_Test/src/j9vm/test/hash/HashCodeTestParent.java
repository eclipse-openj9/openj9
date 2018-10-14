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
/*
 * Created on Jul 9, 2007
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.hash;

public abstract class HashCodeTestParent {
	public static final int MODE_SYSTEM_GC = 0;
	public static final int MODE_SCAVENGE = 1;
	public static final int[] MODES = { MODE_SYSTEM_GC, MODE_SCAVENGE };
	
	private int mode;
	
	/* use a static to make it less likely that the allocate is optimized away */
	static Object obj;
	
	public HashCodeTestParent(int mode) {
		switch (mode) {
		case MODE_SCAVENGE:
		case MODE_SYSTEM_GC:
			this.mode = mode;
			break;
		default:
			throw new IllegalArgumentException();
		}
	}
	
	public void gc() {
		switch (mode) {
		case MODE_SYSTEM_GC:
			System.gc();
			System.gc();
			break;
		case MODE_SCAVENGE:
			long freeMemoryAtStart;
			int count = 0;
			do {
				/* loop until an allocate results in more free memory */
				freeMemoryAtStart = Runtime.getRuntime().freeMemory(); 
				obj = new byte[64];
			} while ( (count++ < 1000000) && (Runtime.getRuntime().freeMemory() <= freeMemoryAtStart));
			break;
		}
	}
	
}
