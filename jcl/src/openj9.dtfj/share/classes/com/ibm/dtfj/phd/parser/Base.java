/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.parser;

import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.text.DateFormat;
import java.util.Calendar;
import java.util.HashMap;


public abstract class Base {

	static DateFormat df = DateFormat.getTimeInstance();
	static boolean xml = false;
	static boolean verbose = false;
	static boolean debug = false;
	static HashMap frozenObjects = new HashMap();

	/**
	 * Output a log message with time stamp. If xml flag is set, use xml formatting.
	 */
	public final void log(String message) {
		_log(message);
	}

	static final void _log(String message) {
		if (!verbose())
			return;
		Calendar c = Calendar.getInstance();
		String time = df.format(c.getTime());

		if (xml)
			System.out.println("<log time='" + time + "'>" + message + "</log>");
		else
			System.out.println(time + ": " + message);
	}


	public static boolean verbose() {
		return verbose;
	}

	public static void setVerbose(boolean flag) {
		verbose = flag;
	}

	public static boolean debug() {
		return debug;
	}

	public static void setDebug(boolean flag) {
		debug = flag;
	}

	public final void trace(String message) {
		if (!debug())
			return;
		System.out.println(message);
	}

	public static void Assert(boolean condition) {
		if (!condition)
			throw new Error("assert failed!");
	}

	public static String hex(int i) {
		return Integer.toHexString(i);
	}

	public static String hex(long i) {
		return Long.toHexString(i);
	}

	String[] options() {
		return null;
	}

	String[] optionDescriptions() {
		return null;
	}

	boolean parseOption(String arg, String opt) {
		return false;
	}

	abstract String className();

	void usage() {
		System.err.print("Usage: java com.ibm.dtfj.phd.parser." + className());
		if (options() != null) {
			for (int i = 0; i < options().length; i++)
				System.err.print(" " + options()[i]);
		}
		System.err.println(" <filename>");
		if (options() != null && optionDescriptions() != null) {
			for (int i = 0; i < options().length; i++)
				System.err.println("\t" + options()[i] + "\t" + optionDescriptions()[i]);
		}
		System.exit(1);
	}

	void parseOptions(String args[]) {
		if (args.length == 0) {
			usage();
		}
		for (int i = 0; i < args.length; i++) {
			if (args[i].charAt(0) == '-') {
				if (args[i].equals("-verbose")) {
					verbose = true;
				} else if (args[i].equals("-debug")) {
					debug = true;
				} else if (!parseOption(args[i], args[i + 1])) {
					usage();
				}
			}
		}
	}
}

