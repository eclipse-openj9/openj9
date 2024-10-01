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

import org.eclipse.openj9.criu.*;

public class OptionsFileTest {
	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "PropertiesTest1":
			propertiesTest1();
			break;
		case "PropertiesTest2":
			propertiesTest2();
			break;
		case "PropertiesTest3":
			propertiesTest3();
			break;
		case "PropertiesTest4":
			propertiesTest4();
			break;
		case "TraceOptionsTest1":
			traceOptionsTest1();
			break;
		case "TraceOptionsTest2":
			traceOptionsTest2();
			break;
		case "TraceOptionsTest3":
			traceOptionsTest3();
			break;
		case "DumpOptionsTest":
			dumpOptionsTest();
			break;
		case "criuDumpOptionsTest":
			criuDumpOptionsTest();
			break;
		case "criuRestoreDumpOptionsTest":
			criuRestoreDumpOptionsTest();
			break;
		case "dumpOptionsTestRequireDynamic":
			dumpOptionsTestRequireDynamic();
			break;
		case "JitOptionsTest":
			jitOptionsTest(args);
			break;
		case "testTransitionToDebugInterpreterViaXXDebugInterpreterWithOptionsFile":
			testTransitionToDebugInterpreterViaXXDebugInterpreterWithOptionsFile();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}

	}

	static void propertiesTest1() {
		String optionsContents = "-Dprop1=val1\n-Dprop2=val2\n-Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		if (!System.getProperty("prop1").equalsIgnoreCase("val1")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop2").equalsIgnoreCase("val2")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop3").equalsIgnoreCase("val3")) {
			System.out.println("ERR: failed properties test");
		}
	}

	static void propertiesTest2() {
		String optionsContents = "-Dprop1=valA\n-Dprop2=valB\n-Dprop3=valC";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.setProperty("prop1", "val1");
		System.setProperty("prop2", "val2");
		System.setProperty("prop3", "val3");

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		if (!System.getProperty("prop1").equalsIgnoreCase("valA")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop2").equalsIgnoreCase("valB")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop3").equalsIgnoreCase("valC")) {
			System.out.println("ERR: failed properties test");
		}
	}

	static void propertiesTest3() {
		String optionsContents = "-Dprop1=val1\n-Dprop2=\\\nval2\n-Dprop3=v \\\n a \\\n  l3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		if (!System.getProperty("prop1").equalsIgnoreCase("val1")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop2").equalsIgnoreCase("val2")) {
			System.out.println("ERR: failed properties test");
		}

		if (!System.getProperty("prop3").equalsIgnoreCase("val3")) {
			System.out.println("ERR: failed properties test");
		}
	}

	static void propertiesTest4() {
		String optionsContents = "-Dprop1=val1\n-Dprop2=\\\nval2\n-Dprop3=v \\ \n a \\     \n  l3 \\";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		//shouldnt get here
		System.out.println("ERR: failed properties test");
	}

	static void traceOptionsTest1() {
		String traceOutput = "traceOutput1.trc";
		String optionsContents = "-Xtrace:print={j9vm.40}\n"
				+ "-Xtrace:print=mt,methods=java/lang/System.getProperties()\n"
				+ "-Xtrace:trigger=method{java/lang/System.getProperties,javadump}\n"
				+ "-Xtrace:maximal=mt,methods=java/lang/System.getProperties(),output=" + traceOutput;
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.getProperties();
		System.out.println("Post-checkpoint");
		if (new File(traceOutput).exists()) {
			System.out.println("TEST PASSED - " + traceOutput + " was created successfully");
		} else {
			System.out.println("TEST FAILED - " + traceOutput + " was NOT created");
		}
	}

	static void traceOptionsTest2() {
		String traceOutput = "traceOutput2.trc";
		String optionsContents = "-Xtrace:print={j9vm.40}\n"
				+ "-Xtrace:none,maximal=j9vm,output={" + traceOutput + ",100m}";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.getProperties();
		System.out.println("Post-checkpoint");
		if (new File(traceOutput).exists()) {
			System.out.println("TEST PASSED - " + traceOutput + " was created successfully");
		} else {
			System.out.println("TEST FAILED - " + traceOutput + " was NOT created");
		}
	}

	static void traceOptionsTest3() {
		String optionsContents = "-Xtrace:print={j9vm.40}\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.getProperties();
		System.out.println("Post-checkpoint");
	}

	static void dumpOptionsTest() {
		String optionsContents = "-Xdump:java:events=vmstop";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		try {
			throw new OutOfMemoryError("dumpOptionsTest");
		} catch (OutOfMemoryError ome) {
			ome.printStackTrace();
		}
		System.out.println("Post-checkpoint");
	}

	static void criuDumpOptionsTest() {
		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void criuRestoreDumpOptionsTest() {
		String optionsContents = "-Xdump:java:events=criuRestore";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void dumpOptionsTestRequireDynamic() {
		String optionsContents = "-Xdump:java:events=vmstop\n"
				+ "-Xdump:nofailover\n"
				+ "-Xdump:java:events=throw,filter=java/lang/OutOfMemoryError,request=exclusive+prepwalk+serial+preempt";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		try {
			throw new OutOfMemoryError("dumpOptionsTestRequireDynamic");
		} catch (OutOfMemoryError ome) {
			ome.printStackTrace();
		}
		System.out.println("Post-checkpoint");
	}

	static void jitOptionsTest(String[] args) {
		String optionsContents = "";

		// index 0 is the test name
		int startIndex = 1;

		// index args.length-1 is the number of checkpoints
		int lastIndex = args.length - 1;

		for (int i = startIndex; i < lastIndex; i++) {
			optionsContents = optionsContents + args[i] + "\n";
		}
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreOptionsFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		// Sleep to ensure that the JVM produces output needed for test success.
		try {
			Thread.sleep(2000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	static void testTransitionToDebugInterpreterViaXXDebugInterpreterWithOptionsFile() {
		String optionsContents = "-XX:+DebugInterpreter";
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
