/*[INCLUDE-IF JFR_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package java.lang;

import java.lang.reflect.Method;

import jdk.internal.misc.Unsafe;

final class JFRHelpers {
	private static Class<?> jfrjvmClass;
	private static Class<?> logTagClass;
	private static Class<?> logLeveLClass;
	private static Class<?> loggerClass;
	private static Object[] logTagValues;
	private static Object[] logLevelValues;
	private static Method log;
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	private static Method logEvent;
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	private static volatile boolean jfrClassesInitialized = false;

	/**
	 * Initializes the JFR reflect classes for loggin etc. Only call this if we
	 * are certain to enter JFR mode.
	 */
	private static synchronized void initJFRClasses() {
		if (!jfrClassesInitialized) {
			try {
				jfrjvmClass = Class.forName("jdk.jfr.internal.JVM");
				logTagClass = Class.forName("jdk.jfr.internal.LogTag");
				logLeveLClass = Class.forName("jdk.jfr.internal.LogLevel");
				loggerClass = Class.forName("jdk.jfr.internal.Logger");

				jfrjvmClass.getModule().implAddExports("jdk.jfr.internal", System.class.getModule());

				logTagValues = (Object[])logTagClass.getDeclaredMethod("values", (Class[])null).invoke(logTagClass);

				logLevelValues = (Object[])logLeveLClass.getDeclaredMethod("values", (Class[])null).invoke(logLeveLClass);
				log = loggerClass.getDeclaredMethod("log", new Class[]{logTagClass, logLeveLClass, String.class});

				/*[IF JAVA_SPEC_VERSION >= 17]*/
				logEvent = loggerClass.getDeclaredMethod("logEvent", new Class[]{logLeveLClass, String[].class, boolean.class});
				/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				Unsafe.getUnsafe().ensureClassInitialized(jfrjvmClass);
				jfrClassesInitialized = true;
			} catch (ReflectiveOperationException e) {
				throw new RuntimeException(e);
			}
		}
	}

	/**
	 * Logs a JFR message. initJFRClasses() must be called first before using this API.
	 * @param message message to log
	 * @param tag the logtag level which matches jdk.jfr.internal.LogTag values
	 * @param level loglevel which matches jdk.jfr.internal.LogLevel values
	 */
	static void logJFR(String message, int tag, int level) {
		if (jfrClassesInitialized) {
			try {
				log.invoke(loggerClass, logTagValues[tag], logLevelValues[level], message);
			} catch (ReflectiveOperationException e) {
				e.printStackTrace();
				throw new RuntimeException(e);
			}
		}
	}


	/*[IF JAVA_SPEC_VERSION >= 17]*/
	/**
	 * Log a JFR event. initJFRClasses() must be called first before using this API.
	 *
	 * @param logLevel loglevel which matches jdk.jfr.internal.LogLevel values
	 * @param lines the string array to log
	 * @param system indicates if its system logging or not
	 */
	static void logJFREvent(int logLevel, String[] lines, boolean system) {
		if (jfrClassesInitialized) {
			try {
				logEvent.invoke(loggerClass, logLevelValues[logLevel], lines, system);
			} catch (ReflectiveOperationException e) {
				e.printStackTrace();
				throw new RuntimeException(e);
			}
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	static void initJFR() {
		//initJFRClasses()
	}
}
