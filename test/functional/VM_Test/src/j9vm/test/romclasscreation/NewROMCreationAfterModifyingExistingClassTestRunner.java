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
package j9vm.test.romclasscreation;

import j9vm.runner.Runner;
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.regex.Pattern;

/**
 * This test checks that when using an existing shared cache created with
 * enableBCI option, if JVMTI agent modifies a class which is already present in
 * the cache, VM creates a local ROMClass instead of loading the class from
 * shared cache.
 * 
 * It runs three commands:
 * <p>
 * Command 1 is used to create a shared cache with -Xshareclasse:enableBCI
 * option.
 * <p>
 * Command 2 uses the same cache but uses a JVMTI agent that modifies a
 * bootstrap class.
 * <p>
 * Command 3 uses the same cache but uses a JVMTI agent that modifies a
 * non-bootstrap class.
 * 
 * Output of Command 2 and 3 is analyzed to verify that a local ROMClass is
 * created for the modified class and the class loaded from shared cache is not
 * used. This is achieved by verifying the presence of appropriate trace points.
 * These are:
 * <p>
 * j9shr.150: This is generated when adding a ROMClass read from shared cache
 * into local hashtable.
 * <p>
 * j9bcu.205: This is generated when a class is to be defined.
 * <p>
 * j9bcu.31: This is generated when a ROMClass is successfully created.
 * 
 * ROMClass address in trace point j9shr.150 should not be same as that in
 * j9bcu.31.
 * 
 * @author Ashutosh Mehra
 */
public class NewROMCreationAfterModifyingExistingClassTestRunner extends Runner {

	public static final String BOOTSTRAP_CLASS = "java/lang/Class";
	public static final String NON_BOOTSTRAP_CLASS = "j9vm/test/romclasscreation/NewROMCreationAfterModifyingExistingClassTest";
	public static final int NUM_COMMANDS = 3;
	public static int commandIndex = 1;

	private static Pattern hexPattern = Pattern.compile("([0-9]|[a-f]|[A-F])*");

	public NewROMCreationAfterModifyingExistingClassTestRunner(
			String className, String exeName, String bootClassPath,
			String userClassPath, String javaVersion) {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* Overrides method in Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		if (commandIndex == 1) {
			customOptions += "-Xshareclasses:enableBCI,reset";
		} else if (commandIndex == 2) {
			/* CMVC 195054: Disable AttachAPI because the classes loaded by AttachAPI thread 
			 * can break the expected order of trace points and cause the test to fail.
			 */ 
			customOptions += "-Dcom.ibm.tools.attach.enable=no"
					+ " -Xshareclasses:enableBCI -agentlib:jvmtitest=test:ecflh001,args:modifyBootstrap="
					+ BOOTSTRAP_CLASS
					+ " -Xtrace:print=tpnid{j9shr.150} -Xtrace:print=tpnid{j9bcu.205} -Xtrace:print=tpnid{j9bcu.31}";
		} else if (commandIndex == 3) {
			/* CMVC 195054: Disable AttachAPI because the classes loaded by AttachAPI thread 
			 * can break the expected order of trace points and cause the test to fail.
			 */
			customOptions += "-Dcom.ibm.tools.attach.enable=no"
					+ " -Xshareclasses:enableBCI -agentlib:jvmtitest=test:ecflh001,args:modifyNonBootstrap="
					+ NON_BOOTSTRAP_CLASS
					+ " -Xtrace:print=tpnid{j9shr.150} -Xtrace:print=tpnid{j9bcu.205} -Xtrace:print=tpnid{j9bcu.31}";
		}

		return customOptions;
	}

	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		for (int i = 1; i <= NUM_COMMANDS; i++) {
			success = super.run();
			if (success == true) {
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
			if (success == false) {
				return false;
			}
			System.gc();
			commandIndex += 1;
		}
		return success;
	}

	public boolean analyze(byte[] stdOut, byte[] stdErr) throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				new ByteArrayInputStream(stdErr)));
		boolean done = false;
		String line = null;
		if (commandIndex == 1) {
			line = in.readLine();
			while (line != null) {
				if (line.indexOf(NewROMCreationAfterModifyingExistingClassTest.TEST_OUTPUT) != -1) {
					done = true;
					break;
				}
				line = in.readLine();
			}
			if (done == false) {
				System.out
						.println("ERROR: Expected \""
								+ NewROMCreationAfterModifyingExistingClassTest.TEST_OUTPUT
								+ "\" not found");
				return false;
			}
		} else {
			String modifiedClass = null;
			String sharedROMClass = null;
			String privateROMClass = null;

			if (commandIndex == 2) {
				modifiedClass = BOOTSTRAP_CLASS;
			} else if (commandIndex == 3) {
				modifiedClass = NON_BOOTSTRAP_CLASS;
			}
			line = in.readLine();

			while (line != null) {
				line = line.trim();
				/*
				 * Example of j9shr.150: RMI storeNew: storing romclass
				 * java/lang/Class in local hashtable (address 0xF19DF288)"
				 */
				if (line.indexOf("j9shr.150") != -1
						&& line.indexOf(modifiedClass) != -1) {
					int index = line.lastIndexOf(" ");
					if (index == -1) {
						System.out.println("ERROR: Space not found in line ["
								+ line + "]!");
						return false;
					}

					/*
					 * Need to skip '0x' and ')' from the last token to get
					 * ROMClass address.
					 */
					sharedROMClass = line.substring(index + 3,
							line.length() - 1);
					if (sharedROMClass.length() < 4
							|| !hexPattern.matcher(sharedROMClass).matches()) {
						System.out
								.println("ERROR: Unexpected value for romAddr ("
										+ sharedROMClass + ") in j9shr150!");
						return false;
					}
				} else if (line.indexOf("j9bcu(j9vm).205") != -1
						&& line.indexOf(modifiedClass) != -1) {
					int index;

					/*
					 * Example of j9bcu.205: 13:21:13.377 0xf6c53000 j9bcu.205 >
					 * BCU internalDefineClass: classnamePtr=0031388C,
					 * classname=java/lang/Class, existingROMClass=F19DF288
					 */
					if (sharedROMClass == null) {
						System.out
								.println("ERROR: Expected to read shared ROMClass before j9bcu.205!");
						return false;
					}

					line = in.readLine();
					if (line == null) {
						System.out
								.println("ERROR: Unexpected end of input stream following j9bcu.205!");
						return false;
					}
					line = line.trim();
					/*
					 * Example of j9bcu.31: 13:21:13.381 0xf6c53000 j9bcu.31 -
					 * BCU internalLoadROMClass: ROMClass loaded successfully
					 * into address F192A020
					 */
					if (line.indexOf("j9bcu(j9vm).31") == -1) {
						System.out
								.println("ERROR: j9bcu.31 tracepoint expected!");
						return false;
					}
					index = line.lastIndexOf(" ");
					if (index == -1) {
						System.out.println("ERROR: Space not found in line ["
								+ line + "]!");
						return false;
					}

					/* ROMClass address is the last word in the trace */
					privateROMClass = line.substring(index + 1, line.length());
					if (privateROMClass.length() < 4
							|| !hexPattern.matcher(privateROMClass).matches()) {
						System.out
								.println("ERROR: Unexpected value for romAddr ("
										+ privateROMClass + ") in j9bcu.31!");
						return false;
					}
					done = true;
					break;
				}
				line = in.readLine();
			}
			if (done == false) {
				System.out.println("ERROR: Private ROMClass for "
						+ modifiedClass + " not created");
				return false;
			}
			if (privateROMClass.equalsIgnoreCase(sharedROMClass)) {
				System.out
						.println("ERROR: Private ROMClass is expected to be different than shared ROMClass");
				return false;
			}
		}
		return true;
	}
}
