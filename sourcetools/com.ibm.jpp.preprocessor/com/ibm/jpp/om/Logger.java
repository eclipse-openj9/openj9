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
package com.ibm.jpp.om;

import java.io.File;

/**
 * The J9 JCL Preprocessor logging class.
 */
public abstract class Logger {
	/** The default message source ID */
	public static final String DEFAULT_MESSAGE_SOURCE = "builder";

	/** Info severity constant (value 0) indicating information only. */
	public static final int SEVERITY_INFO = 1;
	/** Warning severity constant (value 1) indicating a warning. */
	public static final int SEVERITY_WARNING = 2;
	/** Error severity constant (value 2) indicating an error state. */
	public static final int SEVERITY_ERROR = 3;
	/** Fatal error severity constant (value 2) indicating an fatal state. */
	public static final int SEVERITY_FATAL = 4;

	/* The current source of error messages */
	private String messageSource = DEFAULT_MESSAGE_SOURCE;

	private int errorCount = 0;

	/**
	 * Sets the message source.
	 *
	 * @param       messageSource       the message source
	 */
	public void setMessageSource(String messageSource) {
		if (messageSource == null) {
			messageSource = DEFAULT_MESSAGE_SOURCE;
		}
		this.messageSource = messageSource;
	}

	/**
	 * Returns the logger's error count.
	 *
	 * @return      the error count
	 */
	public int getErrorCount() {
		return this.errorCount;
	}

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       severity            the message severity
	 */
	public void log(String message, int severity) {
		log(message, this.messageSource, severity);
	}

	/**
	 * Logs a throwable preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       severity            the message severity
	 * @param       rootThrowable       the message throwable
	 */
	public void log(String message, int severity, Throwable rootThrowable) {
		log(message, this.messageSource, severity, rootThrowable);
	}

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 */
	public void log(String message, String messageSource, int severity) {
		if (severity >= SEVERITY_ERROR) {
			errorCount++;
		}
		logImpl(message, messageSource, severity);
	}

	/**
	 * Logs a throwable preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       rootThrowable       the message throwable
	 */
	public void log(String message, String messageSource, int severity, Throwable rootThrowable) {
		if (severity >= SEVERITY_ERROR) {
			errorCount++;
		}
		logImpl(message, messageSource, severity, rootThrowable);
	}

	/**
	 * Logs a preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       file                the file in which the message occurs
	 */
	public void log(String message, String messageSource, int severity, File file) {
		if (severity >= SEVERITY_ERROR) {
			errorCount++;
		}
		logImpl(message, messageSource, severity, file);
	}

	/**
	 * Logs a throwable preprocessor message.
	 *
	 * @param       message             the message to be logged
	 * @param       messageSource       the message source
	 * @param       severity            the message severity
	 * @param       file                the file in which the message occurs
	 * @param       rootThrowable       the message throwable
	 */
	public void log(String message, String messageSource, int severity, File file, Throwable rootThrowable) {
		if (severity >= SEVERITY_ERROR) {
			errorCount++;
		}
		logImpl(message, messageSource, severity, file, rootThrowable);
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
	 */
	public void log(String message, String messageSource, int severity, File file, int line, int charStart, int charEnd) {
		if (severity >= SEVERITY_ERROR) {
			errorCount++;
		}
		logImpl(message, messageSource, severity, file, line, charStart, charEnd);
	}

	protected abstract void logImpl(String message, String messageSource, int severity);

	protected abstract void logImpl(String message, String messageSource, int severity, Throwable rootThrowable);

	protected abstract void logImpl(String message, String messageSource, int severity, File file);

	protected abstract void logImpl(String message, String messageSource, int severity, File file, Throwable rootThrowable);

	protected abstract void logImpl(String message, String messageSource, int severity, File file, int line, int charStart, int charEnd);

}
