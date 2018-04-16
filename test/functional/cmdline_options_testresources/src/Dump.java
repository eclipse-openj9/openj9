/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

/**
 * 
 * Provides mechanism for generating dumps using the Java RAS APIs
 * 
 * 
 * @author grollest
 *
 */
public class Dump {
	
	/**
	 * Generate a javacore file.
	 * 
	 */
	private static void doJavaDump() {
		com.ibm.jvm.Dump.JavaDump();
	}
	
	/**
	 * Write usage information to the console
	 * 
	 */
	private static void printUsage() {
		System.err.println("Incorrect args.");
		System.err.println("Args: javacore");
	}
	
	/**
	 * See printUsage() for usage. 
	 */
	public static void main(String[] args) {
		
		if (args.length != 1) {
			printUsage();
			System.exit(1);
		}
		
		if (args[0].equalsIgnoreCase("javacore")) {
			doJavaDump();
		} else {
			printUsage();
			System.exit(2);
		}
	}
}
