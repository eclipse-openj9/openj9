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
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.oti.vm.VM;
import jdk.internal.misc.Unsafe;

@SuppressWarnings("nls")
final class JFRHelpers {
	private static Class<?> jfrjvmClass;
	private static Class<?> logTagClass;
	private static Class<?> logLeveLClass;
	private static Class<?> loggerClass;
	private static Class<?> jfrUpCallClass;
	private static Object[] logTagValues;
	private static Object[] logLevelValues;
	private static Method log;
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	private static Method logEvent;
	private static Method bytesForEagerInstrumentation;
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	private static volatile boolean jfrClassesInitialized = false;
	private static String jfrCMDLineOption = null;

	static {
		if (VM.isJFREnabled() && VM.isJFRV2SupportEnabled()) {
			VM.initializeInternalJFRStructures();
			initJFRClasses();
		}
	}

	private static final class DCmdInvocation {
		private Object dcmdInstance;
		private Method dcmdExecute;

		DCmdInvocation(Object dcmdInstance, Method dcmdExecute) {
			this.dcmdInstance = dcmdInstance;
			this.dcmdExecute = dcmdExecute;
		}

		Object getDCmdInstance() {
			return this.dcmdInstance;
		}
		Method getDcmdExecute() {
			return this.dcmdExecute;
		}
	}
	private static DCmdInvocation dcmdStart;
	private static DCmdInvocation dcmdStop;
	private static DCmdInvocation dcmdConfigure;
	private static DCmdInvocation dcmdDump;

	private static Long convertToBytes(String text) {
		Pattern pattern = Pattern.compile("(\\d+)([gkm]b?)?", Pattern.CASE_INSENSITIVE);
		Matcher matcher = pattern.matcher(text);
		if (!matcher.matches()) {
			throw new IllegalArgumentException("Invalid memory size");
		}
		String textValue = matcher.group(1);
		long bytes;
		try {
			bytes = Long.parseLong(textValue);
		} catch (NumberFormatException nfe) {
			throw new IllegalArgumentException("Invalid memory size");
		}
		String unit = matcher.group(2);
		if ((null != unit) && !unit.isEmpty()) {
			switch (unit.toLowerCase()) {
			case "k":
			case "kb":
				bytes *= 1024L;
				break;
			case "m":
			case "mb":
				bytes *= 1024L * 1024L;
				break;
			case "g":
			case "gb":
				bytes *= 1024L * 1024L * 1024L;
				break;
			}
		}
		return bytes;
	}

	private static Long convertToNanoSeconds(String text) {
		Pattern timePattern = Pattern.compile("(\\d+)(ns|us|ms|s|m|h|d)", Pattern.CASE_INSENSITIVE);
		Matcher matcher = timePattern.matcher(text);
		if (!matcher.matches()) {
			throw new IllegalArgumentException("Invalid time value");
		}
		String textValue = matcher.group(1);
		long time;
		try {
			time = Long.parseLong(textValue);
		} catch (NumberFormatException nfe) {
			throw new IllegalArgumentException("Invalid time value");
		}
		String unit = matcher.group(2);
		switch (unit.toLowerCase()) {
		case "ns":
			break;
		case "us":
			time *= 1000L;
			break;
		case "ms":
			time *= 1000L * 1000L;
			break;
		case "s":
			time *= 1000L * 1000L * 1000L;
			break;
		case "m":
			time *= 60 * 1000L * 1000L * 1000L;
			break;
		case "h":
			time *= 60 * 60 * 1000L * 1000L * 1000L;
			break;
		case "d":
			time *= 24 * 60 * 60 * 1000L * 1000L * 1000L;
			break;
		}
		return time;
	}

	private static void initJFRCmdlineOptions() {
		/*[IF (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17)]*/
		try {
			dcmdStart = initDCmdInvocation(dcmdStart, "jdk.jfr.internal.dcmd.DCmdStart");
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
			String results = (String) dcmdStart.getDcmdExecute().invoke(
				dcmdStart.getDCmdInstance(),
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
			/*[ELSE] JAVA_SPEC_VERSION == 11 */
			String[] results = (String []) dcmdStart.getDcmdExecute().invoke(
					dcmdStart.getDCmdInstance(), "internal", jfrCMDLineOption, ',');
			if (null != results) {
				for (String result : results) {
					logJFR(result, 0, 2);
				}
			}
			/*[ENDIF] JAVA_SPEC_VERSION == 11 */
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
		/*[ELSE] (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17) */
		throw new InternalError("JFR support is not enabled");
		/*[ENDIF] (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17) */
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
				jfrUpCallClass = Class.forName("jdk.jfr.internal.JVMUpcalls");

				Module javabase = System.class.getModule();
				Module jdkJFR = jfrjvmClass.getModule();
				Module systemUnnamedModule = VM.getUnnamedModuleForSystemLoader();
				jdkJFR.implAddExports("jdk.jfr.internal", javabase);
				jdkJFR.implAddExports("jdk.jfr.internal.dcmd", javabase);
				jdkJFR.implAddExports("jdk.jfr.internal.handlers", systemUnnamedModule);
				jdkJFR.implAddExportsToAllUnnamed("jdk.jfr.internal.handlers");
				VM.getUnnamedModuleForSystemLoader().implAddReads(jdkJFR);

				logTagValues = (Object[])logTagClass.getDeclaredMethod("values", (Class[])null).invoke(logTagClass);

				logLevelValues = (Object[])logLeveLClass.getDeclaredMethod("values", (Class[])null).invoke(logLeveLClass);
				log = loggerClass.getDeclaredMethod("log", new Class[]{logTagClass, logLeveLClass, String.class});

				/*[IF JAVA_SPEC_VERSION >= 17]*/
				logEvent = loggerClass.getDeclaredMethod("logEvent", new Class[]{logLeveLClass, String[].class, boolean.class});
				bytesForEagerInstrumentation = jfrUpCallClass.getDeclaredMethod("bytesForEagerInstrumentation", long.class, boolean.class, Class.class, byte[].class);
				bytesForEagerInstrumentation.setAccessible(true);
				/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				Unsafe.getUnsafe().ensureClassInitialized(jfrjvmClass);
				Unsafe.getUnsafe().ensureClassInitialized(jfrUpCallClass);
				Unsafe.getUnsafe().ensureClassInitialized(Class.forName("jdk.jfr.events.AbstractJDKEvent"));
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

	/*[IF JAVA_SPEC_VERSION == 17]*/
	private static byte[] transformClassAndInvokebytesForEagerInstrumentation(long traceId, boolean forceInstrumentation, Class<?> superClass, byte[] oldBytes, boolean addMethods) throws ReflectiveOperationException {
		try {
			oldBytes = JFRClassTransformer.transformClass(oldBytes, addMethods);
			return (byte[])bytesForEagerInstrumentation.invoke(null, traceId, forceInstrumentation, superClass, oldBytes);
		} catch (Throwable t) {
			t.printStackTrace();
			throw t;
		}
	}

	private static List<?> transformToList(Object[] array) {
		if (array == null) {
			return Collections.emptyList();
		}
		return Arrays.asList(array);
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	private static void initJFRv2() {
		if (!VM.isJFREnabled() || !VM.isJFRV2SupportEnabled()) {
			return;
		}

		jfrCMDLineOption = VM.getjfrCMDLineOption();
		// JFR support is enabled with V2 implementation.
		if (null != jfrCMDLineOption) {
			initJFRClasses();
			initJFRCmdlineOptions();
		}
	}

	static void initJFR() {
		if (!VM.isJFREnabled() || VM.isJFRV2SupportEnabled()) {
			return;
		}
		// JFR support is enabled with V1 implementation.
		if (VM.isStartFlightRecordingSpecified()) {
			String filenameStr = VM.getJfrRecordingFileName();
			String delayStr = VM.getJfrDelay();
			String durationStr = VM.getJfrDuration();

			long delayNanos = 0;
			long durationNanos = 0;

			if ((delayStr != null) && !delayStr.isEmpty()) {
				delayNanos = convertToNanoSeconds(delayStr);
				System.setProperty("openj9.jfr.cmdline.startflightrecording.delay", String.valueOf(delayNanos));
			}

			if ((durationStr != null) && !durationStr.isEmpty()) {
				durationNanos = convertToNanoSeconds(durationStr);
				System.setProperty("openj9.jfr.cmdline.startflightrecording.duration", String.valueOf(durationNanos));
			}

			if ((filenameStr != null) && !filenameStr.isEmpty()) {
				System.setProperty("openj9.jfr.cmdline.startflightrecording.filename", filenameStr);
			}

			scheduleJFRRecording(delayNanos, durationNanos);
		}
	}

	/**
	 * Schedule JFR recording to start after delay and optionally stop after duration.
	 * If delay is 0, starts immediately.
	 *
	 * @param delayNanos delay before starting in nanoseconds (0 for immediate start)
	 * @param durationNanos duration in nanoseconds (0 means no automatic stop)
	 */
	private static void scheduleJFRRecording(long delayNanos, long durationNanos) {
		Thread schedulerThread = new Thread(() -> {
			try {
				if (delayNanos > 0) {
					long delayMillis = delayNanos / 1_000_000;
					long delayNanosRemainder = delayNanos % 1_000_000;
					Thread.sleep(delayMillis, (int)delayNanosRemainder);
				}

				int result = VM.startJFR();

				if ((result == 0) && durationNanos > 0) {
					scheduleJFRStop(durationNanos);
				}
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}, "JFR-Scheduler");
		schedulerThread.setDaemon(true);
		schedulerThread.start();
	}

	/**
	 * Schedule JFR recording to stop after duration.
	 *
	 * @param durationNanos duration in nanoseconds
	 */
	private static void scheduleJFRStop(long durationNanos) {
		Thread stopThread = new Thread(() -> {
			try {
				long durationMillis = durationNanos / 1_000_000;
				long durationNanosRemainder = durationNanos % 1_000_000;

				Thread.sleep(durationMillis, (int)durationNanosRemainder);

				VM.stopJFR();
				VM.jfrDump();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}, "JFR-Stop-Scheduler");
		stopThread.setDaemon(true);
		stopThread.start();
	}

	/*[IF (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17) ]*/
	private static DCmdInvocation initDCmdInvocation(DCmdInvocation dcmdInvocation, String className) {
		// No synchronization overhead, it is okay to initialize dcmdInvocation more than once.
		if (dcmdInvocation == null) {
			try {
				Class<?> dcmdClass = Class.forName(className);
				Constructor<?> constructor = dcmdClass.getDeclaredConstructor();
				constructor.setAccessible(true);
				Method dcmdExecute;
				if (className.equals("jdk.jfr.internal.dcmd.DCmdConfigure")) {
					// DCmdConfigure doesn't implement execute(ArgumentParser parser)
					dcmdExecute = dcmdClass.getDeclaredMethod(
							"execute",
							boolean.class, // verbose
							String.class, // repositoryPath
							String.class, // dumpPath
							Integer.class, // stackDepth
							Long.class, // globalBufferCount
							Long.class, // globalBufferSize
							Long.class, // threadBufferSize
							Long.class, // memorySize
							Long.class, // maxChunkSize
							Boolean.class // sampleThreads
							);
				} else
				/*[IF JAVA_SPEC_VERSION == 11]*/
				if (className.equals("jdk.jfr.internal.dcmd.DCmdStart")) {
					dcmdExecute = dcmdClass.getDeclaredMethod(
							"execute",
							String.class, // name
							String[].class, // settings
							Long.class, // delay
							Long.class, // duration
							Boolean.class, // disk
							String.class, // path
							Long.class, // maxAge
							Long.class, // maxSize
							Boolean.class, // dumpOnExit
							Boolean.class // pathToGcRoots
							);
				} else
				/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
				{
					dcmdExecute = dcmdClass.getSuperclass().getDeclaredMethod(
							"execute",
							String.class, // source
							String.class, // arg
							char.class // delimiter
							);
				}
				dcmdExecute.setAccessible(true);
				dcmdInvocation = new DCmdInvocation(constructor.newInstance(), dcmdExecute);
			} catch (InvocationTargetException e) {
				throw new InternalError(e.getCause());
			} catch (Exception e) {
				throw new InternalError(e);
			}
		}
		return dcmdInvocation;
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdStart.execute().
	 *
	 * @param execArgs The string arguments separated by a delimiter
	 * @return A string array returned from DCmdStart.execute()
	 */
	public static String[] doJFRDCmdStartExecute(String execArgs) {
		dcmdStart = initDCmdInvocation(dcmdStart, "jdk.jfr.internal.dcmd.DCmdStart");
		try {
			return (String[]) dcmdStart.getDcmdExecute().invoke(dcmdStart.getDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdStop.execute().
	 *
	 * @param execArgs The string arguments separated by a delimiter
	 * @return A string array returned from DCmdStop.execute()
	 */
	public static String[] doJFRDCmdStopExecute(String execArgs) {
		dcmdStop = initDCmdInvocation(dcmdStop, "jdk.jfr.internal.dcmd.DCmdStop");
		try {
			return (String[]) dcmdStop.getDcmdExecute().invoke(dcmdStop.getDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdConfigure.execute().
	 *
	 * @param verbose print verbose output
	 * @param repositoryPath the repository path
	 * @param dumpPath the dump path when fatal error
	 * @param stackDepth the stack trace depth
	 * @param globalBufferCount the number of global buffers
	 * @param globalBufferSize the size of global buffers
	 * @param threadBufferSize the size of thread buffer
	 * @param memorySize the memory size
	 * @param maxChunkSize the chunk size threshold that a new one is to be created
	 * @param sampleThreads if thread sampling is to be enabled
	 * @return A string array returned from DCmdConfigure.execute()
	 */
	public static String[] doJFRDCmdConfigureExecute(
			boolean verbose, String repositoryPath, String dumpPath, Integer stackDepth, Long globalBufferCount,
			Long globalBufferSize, Long threadBufferSize, Long memorySize, Long maxChunkSize, Boolean sampleThreads) {
		dcmdConfigure = initDCmdInvocation(dcmdConfigure, "jdk.jfr.internal.dcmd.DCmdConfigure");
		try {
			return (String[]) dcmdConfigure.getDcmdExecute().invoke(
					dcmdConfigure.getDCmdInstance(),
					verbose,
					repositoryPath,
					dumpPath,
					stackDepth,
					globalBufferCount,
					globalBufferSize,
					threadBufferSize,
					memorySize,
					maxChunkSize,
					sampleThreads);
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdDump.execute().
	 *
	 * @param execArgs The string arguments separated by a delimiter
	 * @return A string array returned from DCmdDump.execute()
	 */
	public static String[] doJFRDCmdDumpExecute(String execArgs) {
		dcmdDump = initDCmdInvocation(dcmdDump, "jdk.jfr.internal.dcmd.DCmdDump");
		try {
			return (String[]) dcmdDump.getDcmdExecute().invoke(dcmdDump.getDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}
	/*[ENDIF] (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17) */
}
