/*[INCLUDE-IF Sidecar18-SE]*/
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

package openj9.tools.attach.diagnostics.tools;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Common functions for diagnostic tools
 *
 */
public class Util {
	
	/**
	 * Read the text from an input stream, split it into separate strings at line breaks,
	 * remove blank lines, and strip leading and trailing whitespace.
	 * @param inStream text input stream
	 * @return contents of inStream as a list of strings
	 */
	public static List<String> inStreamToStringList(InputStream inStream) {
		List<String> stdinList;
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(inStream))) {
			stdinList = jpsOutReader
					.lines()
					.map(s -> s.trim())
					.filter(s -> !s.isEmpty())
					.collect(Collectors.toList());
		} catch (IOException e) {
			stdinList = Collections.emptyList();
		}
		return stdinList;
	}


}
