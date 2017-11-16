/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.util;

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;

import org.testng.log4testng.Logger;

/**
 * Print stream which captures its output in a buffer which can later be converted to a String.
 * This is useful for methods such a Properties.list() and Throwable.printStackTrace() which take a PrintStream argument.
 */
public class StringPrintStream extends PrintStream {

	private static final String UTF_8 = "UTF-8"; //$NON-NLS-1$
	public StringPrintStream(ByteArrayOutputStream byteBuff) throws UnsupportedEncodingException {
		super(byteBuff, true, UTF_8);
		this.byteBuff = byteBuff;
	}
	@Override
	public String toString() {
		flush();
		try {
			return byteBuff.toString(UTF_8);
		} catch (UnsupportedEncodingException e) {
			return null; /* should not happen */
		}
	}
	ByteArrayOutputStream byteBuff;
	
	public static PrintStream factory() {
		try {
			return new StringPrintStream(new ByteArrayOutputStream());
		} catch (UnsupportedEncodingException e) {
			return null; /* should not happen */
		}
	}
	
	public static void logStackTrace(Exception e, Logger testLogger) {
		PrintStream buff = StringPrintStream.factory();
		e.printStackTrace(buff);
		testLogger.error(buff.toString());
	}
}
