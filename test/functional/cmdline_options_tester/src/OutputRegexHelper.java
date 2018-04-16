/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/* CMVC 163891:
 * This class is only exists to to ensure 'java.util.regex.Matcher', and 'java.util.regex.Pattern'
 * do not encounter errors with the verifier when using the embedded class library.
 */
public class OutputRegexHelper {

	public static boolean ContainsMatches(String data, String regex, boolean matchCase, boolean showRegexMatch, String type) {
		try {
			Pattern p = null;
			if (matchCase) {
				p = Pattern.compile(regex);
			} else {
				p = Pattern.compile(regex, Pattern.CASE_INSENSITIVE);
			}
			Matcher m = p.matcher(data);
			boolean retval = m.find();
			if(	retval && showRegexMatch) {
				int start = m.start();
				int end = m.end();				
				System.out.println("\tMatch ("+type+"): "+data.substring(start, end));
			}
			return retval;
		} catch (Exception e) {
			System.out.println("Exception " + e.getClass().toString() + " message " + e.getMessage());
			System.out.println("Regex:" + regex);
			e.printStackTrace();
			return false;
		}
		
	}
	
}
