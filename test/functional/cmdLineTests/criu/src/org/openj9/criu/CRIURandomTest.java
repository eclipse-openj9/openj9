/*
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.criu;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Random;

public class CRIURandomTest {

	public static void main(String args[]) {
		Random r1 = new Random();
		System.out.println("Pre-checkpoint");

		r1.nextInt();
		r1.nextInt();

		CRIUTestUtils.checkPointJVM(Paths.get("cpData"), false);
		System.out.println("Post-checkpoint");

		String testResultsFile = "testResults";
		Path testResultsFilePath = Paths.get(testResultsFile);
		try {
			if (Files.exists(testResultsFilePath)) {
				System.out.println("Second Restore");
				BufferedReader reader = new BufferedReader(new FileReader(testResultsFile));
				String line = null;
				boolean sameValues = true;
				while ((line = reader.readLine()) != null) {
					int val = Integer.parseInt(line);
					if (val != r1.nextInt()) {
						sameValues = false;
					}
				}
				reader.close();
				if (sameValues) {
					System.out.println("ERR: Same random values");
				} else {
					System.out.println("Different random values");
				}
				Files.deleteIfExists(testResultsFilePath);
			} else {
				System.out.println("First Restore");
				PrintWriter writer = new PrintWriter(new FileWriter(testResultsFile));
				writer.println(r1.nextInt());
				writer.println(r1.nextInt());
				writer.println(r1.nextInt());
				writer.println(r1.nextInt());
				writer.println(r1.nextInt());
				writer.close();
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
}
