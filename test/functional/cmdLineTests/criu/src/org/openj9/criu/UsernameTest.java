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

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;

public class UsernameTest {

	private static String getUsernameFromProperty() {
		return System.getProperty("user.name");
	}

	private static String getExpectedUsername() throws IOException {
		// Getting the username from the USER env var is easier, but
		// since env vars also have special treatment in checkpoint mode
		// we don't want to rely on them to validate username handling.
		// Instead we call the `id` system command.
		return new BufferedReader(new InputStreamReader(Runtime.getRuntime().exec("id --user --name").getInputStream())).readLine().trim();
	}

	public static void main(String args[]) throws IOException {
		Path path = Paths.get("cpData");
		String expectedUsernamePreCheckpoint = getExpectedUsername();
		// Checkpointing allowed, should skip getpwuid and fall back to env.
		String usernamePreCheckpoint = getUsernameFromProperty();
		CRIUTestUtils.checkPointJVM(path);
		String expectedUsernamePostCheckpoint = getExpectedUsername();
		// Checkpointing not allowed, should call getpwuid and only fall back on error.
		String usernamePostCheckpoint = getUsernameFromProperty();
		boolean failed = false;
		if (!usernamePreCheckpoint.equals(usernamePostCheckpoint)) {
			System.out.println("FAILURE: user.name property value has changed across checkpoint (" + usernamePreCheckpoint + " -> " + usernamePostCheckpoint + ")");
			failed = true;
		}
		if (!expectedUsernamePreCheckpoint.equals(expectedUsernamePostCheckpoint)) {
			System.out.println("FAILURE: expected user name has changed across checkpoint (" + expectedUsernamePreCheckpoint + " -> " + expectedUsernamePostCheckpoint + ")");
			failed = true;
		}
		if (!usernamePreCheckpoint.equals(expectedUsernamePreCheckpoint)) {
			System.out.println("FAILURE: user.name property value (" + usernamePreCheckpoint + ") does not match expected user name (" + expectedUsernamePreCheckpoint + ")");
			failed = true;
		}
		if (!failed)
			System.out.println("SUCCESS");
	}
}
