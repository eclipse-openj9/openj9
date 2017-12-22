/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob;

/**
 * Extracts Linux-cpp-friendly flags from cross-platform CFLAGS
 * 
 * usage:
 * 
 * java com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)
 * 
 * @author andhall
 *
 */
public class ExtractCPPFlags
{

	/**
	 * @param args
	 */
	public static void main(String[] args)
	{
		for (String arg : args) {
			arg = arg.trim();
			
			//Windows args use / instead of -
			if (arg.startsWith("/")) {
				arg = "-" + arg.substring(1);
			}
			
			if (arg.startsWith("-D") || arg.startsWith("-I")) {
				System.out.print(arg);
				System.out.print(" ");
			}
		}
	}

}
