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

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
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
	private static String jfrCMDLineOption = com.ibm.oti.vm.VM.getjfrCMDLineOption();

	private static Long convertToBytes(String sizeValue) {
		long sizeInBytes = 0L;
		String numericPart = sizeValue.replaceAll("[^0-9]", "");
		String sizeUnit = sizeValue.replaceAll("[0-9]", "").toLowerCase();
		long size = Long.parseLong(numericPart);
		switch (sizeUnit) {
		case "k": /* intentional fall through - KiloBytes */
			sizeInBytes = size * 1_024L;
			break;
		case "m": /* intentional fall through - MegaBytes*/
			sizeInBytes = size * 1_024L * 1_024L;
			break;
		case "g": /* intentional fall through - GigaBytes */
			sizeInBytes = size * 1_024L * 1_024L * 1_024L;
			break;
		default: /* No unit or unrecognized unit, assume bytes */
			sizeInBytes = size;
			break;
		}
		return sizeInBytes;
	}

	private static Long convertToNanoSeconds(String timeValue) {
		long timeInNanos = 0L;
		String numericPart = timeValue.replaceAll("[^0-9]", "");
		String timeUnit = timeValue.replaceAll("[0-9]", "").toLowerCase();
		long time = Long.parseLong(numericPart);
		switch (timeUnit) {
		case "d": /* days */
			timeInNanos = time * 24 * 60 * 60 * 1_000_000_000L;
			break;
		case "h": /* hours */
			timeInNanos = time * 60 * 60 * 1_000_000_000L;
			break;
		case "m": /* minutes */
			timeInNanos = time * 60 * 1_000_000_000L;
			break;
		case "s": /* seconds */
			timeInNanos = time * 1_000_000_000L;
			break;
		default: /* No unit or unrecognized unit, assume nanoseconds */
			timeInNanos = time;
			break;
		}
		return timeInNanos;
	}

	private static void initJFRCmdlineOptions() {
		try {
			Class<?> dcmdStartClass = Class.forName("jdk.jfr.internal.dcmd.DCmdStart");
			Constructor<?> constructor = dcmdStartClass.getDeclaredConstructor();
			constructor.setAccessible(true);
			Object dcmdStartInstance = constructor.newInstance();

			/*[IF JAVA_SPEC_VERSION == 11]*/
			String fileName = null;
			String settings = null;
			Long delay = null;
			Long duration = null;
			Boolean disk = null;
			Long maxAge = null;
			Long maxSize = null;
			Boolean dumpOnExit = null;
			String[] jfrCMDLineOptionPairs = jfrCMDLineOption.split(",");
			for (String pair : jfrCMDLineOptionPairs) {
				String[] configKeyValue = pair.split("=");
				if (2 == configKeyValue.length) {
					String key = configKeyValue[0];
					String value = configKeyValue[1];
					switch (key) {
					case "filename":
						fileName = value;
						break;
					case "settings":
						settings = value;
						break;
					case "delay":
						delay = convertToNanoSeconds(value);
						break;
					case "duration":
						duration = convertToNanoSeconds(value);
						break;
					case "maxage":
						maxAge = convertToNanoSeconds(value);
						break;
					case "maxsize":
						maxSize = convertToBytes(value);
						break;
					case "dumponexit":
						dumpOnExit = Boolean.valueOf(value);
						break;
					case "disk":
						disk = Boolean.valueOf(value);
						break;
					}
				}
			}
			if (null == settings) {
				settings = "default";
			}
			/* Convert the string to a String array */
			String[] settingsArray = new String[] { settings };

			Method executeMethod = dcmdStartClass.getDeclaredMethod(
				"execute",
				String.class, //name
				String[].class, //settings
				Long.class, //delay
				Long.class, //duration
				Boolean.class, //disk
				String.class, //path
				Long.class, //maxAge
				Long.class, //maxSize
				Boolean.class, //dumpOnExit
				Boolean.class //pathToGcRoots
			);

			executeMethod.setAccessible(true);
			String results = (String) executeMethod.invoke(
				dcmdStartInstance,
				null,
				settingsArray,
				delay,
				duration,
				disk,
				fileName,
				maxAge,
				maxSize,
				dumpOnExit,
				null
			);
			if (null != results) {
				logJFR(results, 0, 2);
			}
			/*[ENDIF] JAVA_SPEC_VERSION == 11 */

			/*[IF JAVA_SPEC_VERSION >= 17]*/
			Method executeMethod = dcmdStartClass.getSuperclass().getDeclaredMethod("execute", String.class, String.class, char.class);
			executeMethod.setAccessible(true);

			String[] results = (String []) executeMethod.invoke(dcmdStartInstance, "internal", jfrCMDLineOption, ',');
			if (null != results) {
				for (String result : results) {
					logJFR(result, 0, 2);
				}
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();
			e.printStackTrace();
			throw new InternalError(cause);
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

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
		if (null != jfrCMDLineOption) {
			initJFRClasses();
			initJFRCmdlineOptions();
		}
	}
}
