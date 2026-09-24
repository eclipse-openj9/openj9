/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2025
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

package org.eclipse.openj9.criu;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Scanner;
import java.util.regex.Pattern;

/**
 * Parse a CRIU checkpoint/restore console output or log file, search for known
 * error strings, and map them to detailed messages.
 *
 */
@SuppressWarnings("nls")
public class SearchKnownFailures {

	private static final String[] originalErrors = new String[] {
			// CRIU source: https://github.com/ibmruntimes/criu/blob/criu-dev/criu/cr-check.c#L1911
			"Running as non-root requires '--unprivileged'" };

	private static final String[] detailedMessages = new String[] {
			"Running as non-root requires '--unprivileged' indicates that '-Dopenj9.internal.criu.unprivilegedMode=true'"
					+ " is required for the command performing checkpoint. In addition, the container has to be run with additional"
					+ " Linux capabilities --cap-add=CHECKPOINT_RESTORE, --cap-add=NET_ADMIN and --cap-add=SYS_PTRACE." };

	/**
	 * @param args An input file name
	 */
	public static void main(String[] args) {
		if (args.length == 0) {
			System.out.println("An input file name must be provided!");
		} else if (args.length > 1) {
			System.out.println("Incorrect number of arguments are provided!");
		} else {
			searchKnownErrorsMaptoDetailedMessages(args[0]);
		}
	}

	private static void searchKnownErrorsMaptoDetailedMessages(String fileName) {
		boolean foundKnownFailures = false;

		try (Scanner scanner = new Scanner(new File(fileName))) {
			for (int index = 0; index < originalErrors.length; index++) {
				String originalError = originalErrors[index];
				Pattern pattern = Pattern.compile(Pattern.quote(originalError));
				if (scanner.findWithinHorizon(pattern, 0) != null) {
					System.out.println("Found: " + originalError);
					System.out.println("Further clarification: " + detailedMessages[index]);
					foundKnownFailures = true;
					break;
				}
			}
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
		if (!foundKnownFailures) {
			System.out.println("No known failures found.");
		}
	}

	private SearchKnownFailures() {
	}
}
