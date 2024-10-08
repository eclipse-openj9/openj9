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
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;

import org.eclipse.openj9.criu.*;

public class EnvVarFileTest {
	static private final String RESTORE_ENV_VAR = "OPENJ9_RESTORE_JAVA_OPTIONS";

	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "EnvVarFileTest1":
			envVarFileTest1();
			break;
		case "EnvVarFileTest2":
			envVarFileTest2();
			break;
		case "EnvVarFileTest3":
			envVarFileTest3();
			break;
		case "EnvVarFileTest4":
			envVarFileTest4();
			break;
		case "EnvVarFileTest5":
			envVarFileTest5();
			break;
		case "EnvVarFileTest6":
			envVarFileTest6();
			break;
		case "EnvVarFileTest7":
			envVarFileTest7();
			break;
		case "EnvVarFileTest8":
			envVarFileTest8();
			break;
		case "EnvVarFileTest9":
			envVarFileTest9();
			break;
		case "EnvVarFileTest10":
			envVarFileTest10();
			break;
		case "EnvVarFileTest11":
			envVarFileTest11();
			break;
		case "EnvVarFileTest12":
			envVarFileTest12();
			break;
		case "EnvVarFileTest13":
			envVarFileTest13();
			break;
		case "EnvVarFileTest14":
			envVarFileTest14();
			break;
		case "EnvVarFileTest15":
			envVarFileTest15();
			break;
		case "EnvVarFileTest16":
			envVarFileTest16();
			break;
		case "EnvVarFileTest17":
			envVarFileTest17();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}

	}

	static void envVarFileTest1() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

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

	static void envVarFileTest2() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=valA -Dprop2=valB -Dprop3=valC";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

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

	static void envVarFileTest3() {
		String optionsContents = RESTORE_ENV_VAR + "=-Xdump:none";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		if (System.getProperty("prop1").equalsIgnoreCase("val1")) {
			System.out.println("ERR: failed properties test");
		}

		if (System.getProperty("prop2").equalsIgnoreCase("val2")) {
			System.out.println("ERR: failed properties test");
		}

		if (System.getProperty("prop3").equalsIgnoreCase("val3")) {
			System.out.println("ERR: failed properties test");
		}
	}

	static void envVarFileTest4() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1\r\n-Dprop2=\\\nval2\n-Dprop3=v \\ \n a \\     \n  l3 \\";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		//shouldnt get here
		System.out.println("ERR: failed env var test");
	}

	static void envVarFileTest5() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest6() {
		String optionsContents = RESTORE_ENV_VAR + "S=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest7() {
		String optionsContents = "S" + RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest8() {
		String optionsContents = "S" + RESTORE_ENV_VAR + "=-Dprop1=vala -Dprop2=valb -Dprop3=valc\n";
		optionsContents += RESTORE_ENV_VAR + "S=-Dprop1=valx -Dprop2=valy -Dprop3=valz\n";
		optionsContents += RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest9() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dspace=\"hello world\" -Dweird=hello\" world\" -Dprop=-Dprop\n";

		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");

		if (!System.getProperty("space").equalsIgnoreCase("hello world")) {
			System.out.println("ERR: failed properties test");
			System.out.println("space: " + System.getProperty("space"));
		}

		if (!System.getProperty("weird").equalsIgnoreCase("hello world")) {
			System.out.println("ERR: failed properties test");
			System.out.println("weird: " + System.getProperty("weird"));
		}

		if (!System.getProperty("prop").equalsIgnoreCase("-Dprop")) {
			System.out.println("ERR: failed properties test");
			System.out.println("prop: " + System.getProperty("prop"));
		}
	}

	static void envVarFileTest10() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3 -Xmx64m\n";

		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
		System.out.println("ERR: failed properties test");

	}

	static void envVarFileTest11() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3 -Xmx64m -XX:+IgnoreUnrecognizedRestoreOptions\n";

		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

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

	static void envVarFileTest12() {
		String optionsContents = RESTORE_ENV_VAR + "=-Dprop1=val1 -Dprop2=val2 -Dprop3=val3\r\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

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

	static void envVarFileTest13() {
		String optionsContents = RESTORE_ENV_VAR + "=-Xrs\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
		System.out.println("ERR: failed properties test");
	}

	static void envVarFileTest14() {
		String optionsContents = RESTORE_ENV_VAR + "=-Xrs:sync\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
		System.out.println("ERR: failed properties test");
	}

	static void envVarFileTest15() {
		String optionsContents = RESTORE_ENV_VAR + "=-Xrs:onRestore\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest16() {
		String optionsContents = RESTORE_ENV_VAR + "=-Xrs:syncOnRestore\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}

	static void envVarFileTest17() {
		String optionsContents = RESTORE_ENV_VAR + "=-XX:+DebugInterpreter\n";
		Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);

		Path imagePath = Paths.get("cpData");
		CRIUTestUtils.createCheckpointDirectory(imagePath);
		CRIUSupport criuSupport = new CRIUSupport(imagePath);
		criuSupport.registerRestoreEnvFile(optionsFilePath);

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVM(criuSupport, imagePath, true);
		System.out.println("Post-checkpoint");
	}
}
