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
 * @author PBurka
 *
 * This test checks to ensure that modified arrays are reported correctly.
 */
public class ModifyArrayData extends Test {

	public static void main(String[] args) {
		if (args.length != 5) {
			System.out.println("Usage:");
			System.out.println("  java " + ModifyArrayData.class.getName() + " {boolean|byte|char|double|float|int|long|short|string} <size> <offset1> <offset2> <mode>");
			System.exit(0);
		}
		
		int size = Integer.parseInt(args[1]);
		int mod1 = Integer.parseInt(args[2]);
		int mod2 = Integer.parseInt(args[3]);
		int mode = Integer.parseInt(args[4]);
		
		if (args[0].equals("boolean")) {
			test(new boolean[size], 'Z', mod1, mod2, mode);
		} else if (args[0].equals("byte")) {
			test(new byte[size], 'B', mod1, mod2, mode);
		} else if (args[0].equals("char")) {
			test(new char[size], 'C', mod1, mod2, mode);
		} else if (args[0].equals("double")) {
			test(new double[size], 'D', mod1, mod2, mode);
		} else if (args[0].equals("float")) {
			test(new float[size], 'F', mod1, mod2, mode);
		} else if (args[0].equals("int")) {
			test(new int[size], 'I', mod1, mod2, mode);
		} else if (args[0].equals("long")) {
			test(new long[size], 'J', mod1, mod2, mode);
		} else if (args[0].equals("short")) {
			test(new short[size], 'S', mod1, mod2, mode);
		} else {
			System.err.println("Unrecognized type: " + args[0]);
		}
	}

	private static native boolean test(Object array, char type, int mod1, int mod2, int mode);
	
}
