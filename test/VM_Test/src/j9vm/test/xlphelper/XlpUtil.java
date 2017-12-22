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

package j9vm.test.xlphelper;

public class XlpUtil {
	
	public static final String XLP_PAGE_TYPE_NOT_USED = "not used";
	public static final String XLP_PAGE_TYPE_PAGEABLE = "pageable";
	public static final String XLP_PAGE_TYPE_NONPAGEABLE = "nonpageable";
	
	/**
	 * Accepts a memory size in human-readable format (like 2K, 4M, 6G) and
	 * returns memory size in bytes.
	 * If error occurs during conversion, it returns 0.
	 *  
	 * @param pageSizeString
	 * 
	 * @return memory size in bytes
	 */
	public static long pageSizeStringToLong(String pageSizeString) {
		long pageSizeInBytes = 0;
		long pageSizeQualifier = 0;
		boolean invalidQualifier = false;
		String qualifier = pageSizeString.substring(pageSizeString.length()-1);		/* last character must be a qualifier if present */

		if (qualifier.matches("[a-zA-Z]")) {
			switch(qualifier.charAt(0)) {
			case 'k':
			case 'K':
				pageSizeQualifier = 10;
				break;
			case 'm':
			case 'M':
				pageSizeQualifier = 20;
				break;
			case 'g':
			case 'G':
				pageSizeQualifier = 30;
				break;
			default:
				System.out.println("ERROR: Unrecognized qualifier found in page size string");
				invalidQualifier = true;
				break;
			}
		}
		if (invalidQualifier) {
			pageSizeInBytes = 0;			
		} else {
			long pageSizeValue = 0;
			if (pageSizeQualifier != 0) {
				/* qualifier found, ignore last character */
				pageSizeValue = Long.parseLong(pageSizeString.substring(0, pageSizeString.length()-1));
			} else {
				/* qualifier not found */
				pageSizeValue = Long.parseLong(pageSizeString.substring(0, pageSizeString.length()));
			}
			pageSizeInBytes = pageSizeValue << pageSizeQualifier;
		}
		return pageSizeInBytes;
	}

}
