/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.commandline;

import java.io.File;
import java.io.PrintStream;

import com.ibm.jpp.om.Logger;

/**
 * Commandline implementation of the Preprocessor logger.  This implementation simply
 * printers the preprocessor errors to the screen in a organized fashion.
 */
public class CommandlineLogger extends Logger {

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 *
	 * @see         Logger#log(String, String, int)
	 */
	@Override
	public void logImpl(String message, String messageSource, int severity) {
		@SuppressWarnings("resource")
		PrintStream out = createOutputStream(severity);

		out.print("[");
		out.print(messageSource);
		out.print("] ");
		out.print(message);
		out.println();
		out.flush();
	}

	/**
	 * Logs a throwable preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       rootThrowable       the message throwable
	 *
	 * @see         Logger#log(String, String, int, Throwable)
	 */
	@Override
	public void logImpl(String message, String messageSource, int severity, Throwable rootThrowable) {
		@SuppressWarnings("resource")
		PrintStream out = createOutputStream(severity);

		out.print("[");
		out.print(messageSource);
		out.print("] ");
		out.print(message);
		out.println();
		out.println("---------- START STACK TRACE ----------");
		rootThrowable.printStackTrace(out);
		out.println("----------- END STACK TRACE -----------");
		out.flush();
	}

	protected PrintStream createOutputStream(int severity) {
		PrintStream out;
		if (severity >= SEVERITY_ERROR) {
			out = System.err;
			out.print("ERROR ");
		} else {
			out = System.out;
		}
		return out;
	}

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       file                the file in which the message occurs
	 *
	 * @see         Logger#log(String, String, int, File)
	 */
	@Override
	public void logImpl(String message, String messageSource, int severity, File file) {
		@SuppressWarnings("resource")
		PrintStream out = createOutputStream(severity);

		out.print("[");
		out.print(messageSource);
		out.print("] ");
		out.print(message);
		out.println();
		out.print("    file: ");
		out.print(file.getAbsolutePath());
		out.println();
		out.flush();
	}

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       file                the file in which the message occurs
	 * @param       line                the line in which the message occurs
	 * @param       charStart           the character in which the message starts
	 * @param       charEnd             the character in which the message ends
	 *
	 * @see         Logger#log(String, String, int, File, int, int, int)
	 */
	@Override
	public void logImpl(String message, String messageSource, int severity, File file, int line, int charStart, int charEnd) {
		@SuppressWarnings("resource")
		PrintStream out = createOutputStream(severity);

		out.print("[");
		out.print(messageSource);
		out.print("] ");
		out.print(message);
		out.println();

		out.print("    file: ");
		out.print(file.getAbsolutePath());
		out.println();

		out.print("    line: ");
		out.print(line);
		out.print(" (char ");
		out.print(charStart);
		out.print(" to ");
		out.print(charEnd);
		out.print(")");
		out.println();

		out.flush();
	}

	/**
	 * Logs a throwable preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       file                the file in which the message occurs
	 * @param       rootThrowable       the message throwable
	 *
	 * @see         Logger#logImpl(String, String, int, File, Throwable)
	 */
	@Override
	protected void logImpl(String message, String messageSource, int severity, File file, Throwable rootThrowable) {
		@SuppressWarnings("resource")
		PrintStream out = createOutputStream(severity);

		out.print("[");
		out.print(messageSource);
		out.print("] ");
		out.print(message);
		out.println();
		out.print("    file: ");
		out.print(file.getAbsolutePath());
		out.println();
		out.println("---------- START STACK TRACE ----------");
		rootThrowable.printStackTrace(out);
		out.println("----------- END STACK TRACE -----------");
		out.flush();
	}

}
