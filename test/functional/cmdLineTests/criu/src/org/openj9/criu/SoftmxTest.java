/*
 * Copyright IBM Corp. and others 2023
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

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.nio.file.Files;
import java.lang.management.*;
import java.util.*;
import java.util.stream.*;
import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import org.eclipse.openj9.criu.*;


public class SoftmxTest {
	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "HalfSize":
			PercentAndSoftmx(1, 0);
			break;
		case "FullSize":
			PercentAndSoftmx(2, 0);
			break;
		case "OneAndHalfSize":
			PercentAndSoftmx(3, 0);
			break;
		case "HalfSizeSoftmxRestore":
			PercentAndSoftmx(1, 1);
			break;
		case "FullSizeSoftmxRestore":
			PercentAndSoftmx(2, 1);
			break;
		case "OneAndHalfSizeSoftmxRestore":
			PercentAndSoftmx(3, 1);
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}

	}
	static void PercentAndSoftmx(Integer percent_type, Integer restore_type) {
		String optionsContents = "-XXgc:fvtest_testRAMSizePercentage=";
		if (1 == percent_type) {
			optionsContents += "50";
		}
		else if (2 == percent_type) {
			optionsContents += "100";
		}
		else if (3 == percent_type) {
			optionsContents += "150";
		}
		optionsContents += "\n-Xverbosegclog:output.txt";
		if (1 == restore_type) {
			optionsContents += "\n";
			optionsContents += "-Xsoftmx";
			try {
				// read total memory value
				BufferedReader Buff = new BufferedReader(new FileReader("/proc/meminfo"));
				String text = Buff.readLine();
				String numberOnly= text.replaceAll("[^0-9]", "");
				System.out.println(numberOnly);
				long memorySize = Long.parseLong(numberOnly);
				optionsContents = optionsContents + Long.toString(memorySize/8*3) + "k";
			} catch (IOException e) {
				System.out.println("/proc/meminfo file cannot be found.");
				e.printStackTrace();
			}
		}
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);
		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}
}