/*
 * Copyright IBM Corp. and others 2024
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
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.PosixFilePermissions;

import org.eclipse.openj9.criu.*;

public class TestCRIUFailurePath {

	public static void main(String[] args) throws Throwable {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		}

		String test = args[0];

		switch (test) {
		case "badDir":
			badDir();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}
	}

	private static void badDir() throws Throwable {
		CRIUTestUtils.showThreadCurrentTime("Test badDir() starts ..");
		Path path = Paths.get("badDir");
		path.toFile().mkdir();
		Files.setPosixFilePermissions(path, PosixFilePermissions.fromString("r--r--r--"));
		CRIUSupport criu = new CRIUSupport(path);

		try {
			CRIUTestUtils.checkPointJVMNoSetup(criu, path, true);
			System.out.println("TEST FAILED");
		} catch (SystemCheckpointException e) {
			System.out.println("TEST PASSED");
		}

		CRIUTestUtils.showThreadCurrentTime("Test badDir() after doCheckpoint()");
	}
}
