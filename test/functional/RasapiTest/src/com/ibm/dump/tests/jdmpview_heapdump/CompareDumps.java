/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.jdmpview_heapdump;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class CompareDumps {

	public static void main(String[] args) throws IOException {
		
		int error_count = 0;
		if (args.length != 2) {
			System.err.println("CreateDump takes two arguments: the names of two files to compare");
		}
		
		System.err.println("Comparing the first 1000 lines of " + args[0] + " with the first 1000 lines of " + args[1]);
		BufferedReader in1 = new BufferedReader(new FileReader(args[0]));
		BufferedReader in2 = new BufferedReader(new FileReader(args[1]));
		
		for (int i = 0;i<1000;i++) {
			String line1 = in1.readLine();
			String line2 = in2.readLine();
			if ( ! line1.equals(line2)) {
				error_count++;
				System.err.println("The two files differ at line " + (i + 1));
				System.err.println("The first file has:");
				System.err.println(line1);
				System.err.println("The second file has:");
				System.err.println(line2);
			}
		}
		
		System.err.println("Count of differences in the first 1000 lines: " + error_count);
		
		System.exit(error_count);
}

}
