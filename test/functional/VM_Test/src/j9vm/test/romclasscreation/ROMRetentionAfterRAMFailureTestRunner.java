package j9vm.test.romclasscreation;

/*******************************************************************************
 * Copyright (c) 2010, 2012 IBM Corp. and others
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

import j9vm.runner.Runner;

import java.io.*;
import java.util.regex.Pattern;

/**
 * Runner for ROMRetentionAfterRAMFailureTest, to pass in extra
 * command-line options and analyze the output.
 * 
 * @see ROMRetentionAfterRAMFailureTest
 * 
 * @author Alexei Svitkine
 *
 */
public class ROMRetentionAfterRAMFailureTestRunner extends Runner {
	private static Pattern hexPattern = Pattern.compile("([0-9]|[a-f]|[A-F])*");

	public ROMRetentionAfterRAMFailureTestRunner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion)  {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* Overrides method in Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		customOptions += " -Xtrace:print=tpnid{j9bcu.205} -Xtrace:print=tpnid{j9bcu.31}";
		return customOptions;
	}

	/* Overrides method in j9vm.runner.Runner. */
	public boolean run()  {
		boolean success = super.run();
		if (success) {
			byte[] stdOut = inCollector.getOutputAsByteArray();
			byte[] stdErr = errCollector.getOutputAsByteArray();
			try {
				success = analyze(stdOut, stdErr);
			} catch (Exception e) {
				success = false;
				System.out.println("Unexpected Exception:");
				e.printStackTrace();
			}
		}
		System.gc();
		return success;
	}

	public boolean analyze(byte[] stdOut, byte[] stdErr) throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(new ByteArrayInputStream(stdErr)));
		String line = in.readLine();
		String lastRomAddr = null;
		int numMatches = 0;
		String expectedClassName = "classname=" + ROMRetentionAfterRAMFailureTest.CLASS_TO_LOAD;
		int expectedMatches = ROMRetentionAfterRAMFailureTest.ATTEMPTS;

		while (line != null) {
			line = line.trim();

			if (line.indexOf("j9bcu(j9vm).205") != -1 && line.indexOf(expectedClassName) != -1) {
				line = in.readLine();
				if (line == null) {
					System.out.println("ERROR: Unexpected end of input stream following j9bcu.205!");
					return false;
				}

				line = line.trim();
				if (line.indexOf("j9bcu(j9vm).31") == -1) {
					System.out.println("ERROR: j9bcu.31 tracepoint expected!");
					return false;
				}

				int index = line.lastIndexOf(" ");
				if (index == -1) {
					System.out.println("ERROR: Space not found in line [" + line + "]!");
					return false;
				}

				String romAddr = line.substring(index + 1, line.length());
				if (romAddr.length() < 4 || !hexPattern.matcher(romAddr).matches()) {
					System.out.println("ERROR: Unexpected value for romAddr (" + romAddr + ")!");
					return false;
				}

				if (lastRomAddr != null && !romAddr.equals(lastRomAddr)) {
					System.out.println("ERROR: Addresses " + romAddr + " and " + lastRomAddr + " expected to match!");
					return false;
				}
				lastRomAddr = romAddr;

				numMatches++;
			}

			line = in.readLine();
		}

		if (numMatches != expectedMatches) {
			System.out.println("ERROR: Unexpected number of matched lines " +
				"(got " + numMatches + ", expected " +expectedMatches + ")!");
			return false;
		}

		return true;
	}
}
