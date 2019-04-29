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

package org.openj9.test.util;

import java.util.List;
import java.util.Optional;

/**
 * Utility methods for String input and output.
 *
 */
public class StringUtilities {

	/**
	 * Find the first occurrence of a substring in a list of Strings.
	 * @param needle pattern for which to search
	 * @param haystack list of Strings to search
	 * @return Optional which is either empty or contains matching string 
	 */
	public static Optional<String> searchSubstring(String needle, List<String> haystack) {
		return haystack.stream().filter(s -> s.contains(needle)).findFirst();
	}
	

	/**
	 * Determine if a pattern (needle) matches exactly  in a list of Strings.
	 * @param needle pattern for which to search
	 * @param haystack list of Strings to search
	 * @return true if at least one member of haystack equals needle 
	 */
	public static boolean contains(String needle, List<String> haystack) {
		return haystack.stream().anyMatch(s -> s.equals(needle));
	}
}
