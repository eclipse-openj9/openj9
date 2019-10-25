/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.testKitGen;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Counter {
	private static Map<String, Integer> count = new HashMap<String, Integer>();
	private static String countmk = Options.getProjectRootDir() + "/TestConfig/" + Constants.COUNTMK;

	private Counter(Options op) {
	}

	public static void generateFile() {
		FileWriter f;
		try {
			f = new FileWriter(countmk);
			f.write(Constants.HEADERCOMMENTS);

			List<String> targetCountKeys = new ArrayList<>(count.keySet());
			Collections.sort(targetCountKeys);

			f.write("_GROUPTARGET = $(firstword $(MAKECMDGOALS))\n\n");
			f.write("GROUPTARGET = $(patsubst _%,%,$(_GROUPTARGET))\n\n");
			for (String key : targetCountKeys) {
				f.write("ifeq ($(GROUPTARGET)," + key + ")\n");
				f.write("\tTOTALCOUNT := " + count.get(key) + "\n");
				f.write("endif\n\n");
			}

			f.close();

			System.out.println();
			System.out.println("Generated " + countmk);
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(1);
		}
	}

	public static void add(String key, int value) {
		count.put(key, count.getOrDefault(key, 0) + value);
	}
}