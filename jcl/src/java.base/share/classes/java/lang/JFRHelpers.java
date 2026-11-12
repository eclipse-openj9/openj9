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
	/* Indices into logTagValues[] / logLevelValues[] to retrieve LogTag and LogLevel objects;
	 * keep in sync with the enum declaration order in jdk.jfr.internal.LogTag
	 * and jdk.jfr.internal.LogLevel.
	 */
	private static final int LOGTAG_JFR_START_ORDINAL = 13; // LogTag.JFR_START.ordinal()
	private static final int LOGLEVEL_WARN_ORDINAL = 3; // LogLevel.WARN.ordinal()

	private static Class<?> jfrjvmClass;
	private static Class<?> logTagClass;
	private static Class<?> logLeveLClass;
	private static Class<?> loggerClass;
	private static Class<?> eventWriter;
	private static Object[] logTagValues;
	private static Object[] logLevelValues;
	private static Method log;
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	private static Method logEvent;
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	private static Constructor<?> constructorEventWriter;
	private static volatile boolean jfrClassesInitialized = false;
	private static String jfrCMDLineOption = null;
	// Non-null exactly when ensureJfrModuleAvailable() has run and failed.
	private static String[] jfrModuleUnavailableMessage;

	static {
		if (VM.isJFREnabled() && VM.isJFRV2SupportEnabled()) {
			if (ensureJfrModuleAvailable()) {
				VM.initializeInternalJFRStructures();
				initJFRClasses();
			}
		}
	}

	/**
	 * Ensure jdk.jfr is resolved into the running module graph, loading it on
	 * demand if the launched application never itself required it (e.g. a
	 * --module launch, or a jcmd-triggered command with no -XX:StartFlightRecording
	 * on the original command line).
	 *
	 * @return true if jdk.jfr is available, false if it could not be loaded
	 */
	private static boolean ensureJfrModuleAvailable() {
		if (ModuleLayer.boot().findModule("jdk.jfr").isPresent()) {
			return true;
		}
		try {
			jdk.internal.module.Modules.loadModule("jdk.jfr");
			return true;
		} catch (Throwable t) {
			jfrModuleUnavailableMessage = new String[] {
				"Flight Recorder can not be enabled.",
				t.toString() + "."
			};
			VM.disableJFRV2Support();
			return false;
		}
	}

	private static final class DCmdInvocation {
		private final Method dcmdExecute;
		private final Constructor<?> dcmdConstructor;

		DCmdInvocation(Constructor<?> dcmdConstructor, Method dcmdExecute) {
			this.dcmdConstructor = dcmdConstructor;
			this.dcmdExecute = dcmdExecute;
		}

		Object newDCmdInstance() {
			// Always create a new instance, otherwise last JFR command results will stay.
			try {
				return dcmdConstructor.newInstance();
			} catch (ReflectiveOperationException e) {
				throw new InternalError(e);
			}
		}

		Method getDcmdExecute() {
			return this.dcmdExecute;
		}
	}

	private static DCmdInvocation dcmdStart;
	private static DCmdInvocation dcmdStop;
	private static DCmdInvocation dcmdConfigure;
	private static DCmdInvocation dcmdDump;
	private static DCmdInvocation dcmdCheck;

	/*[IF JAVA_SPEC_VERSION == 17]*/
	/* -XX:FlightRecorderOptions limits, matching the reference implementation. */
	private static final long MIN_MEMORY_SIZE = 1024 * 1024;
	private static final long MIN_GLOBAL_BUFFER_SIZE = 64 * 1024;
	private static final long MAX_GLOBAL_BUFFER_SIZE = 2L * 1024 * 1024 * 1024;
	private static final long MIN_GLOBAL_BUFFER_COUNT = 2;
	private static final long MIN_THREAD_BUFFER_SIZE = 4 * 1024;
	private static final long MAX_THREAD_BUFFER_SIZE = 2L * 1024 * 1024 * 1024;
	/* Same as the jdk.jfr.internal.Options default: the page size, but at least 8K. */
	private static final long DEFAULT_THREAD_BUFFER_SIZE = Math.max(Unsafe.getUnsafe().pageSize(), 8 * 1024);
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	private static Long convertToBytes(String text) {
		if (null == text) {
			throw new IllegalArgumentException("Parsing error memory size value: syntax error, value is null");
		}
		if (text.startsWith("-")) {
			throw new IllegalArgumentException("Parsing error memory size value: negative values not allowed");
		}
		Pattern pattern = Pattern.compile("(\\d+)([gkm]b?)?", Pattern.CASE_INSENSITIVE);
		Matcher matcher = pattern.matcher(text);
		if (!matcher.matches()) {
			throw new IllegalArgumentException("Parsing error memory size value: invalid value");
		}
		String textValue = matcher.group(1);
		long bytes;
		try {
			bytes = Long.parseLong(textValue);
		} catch (NumberFormatException nfe) {
			throw new IllegalArgumentException("Parsing error memory size value: invalid value");
		}
		String unit = matcher.group(2);
		if ((null != unit) && !unit.isEmpty()) {
			try {
				switch (unit.toLowerCase()) {
				case "k":
				case "kb":
					bytes = Math.multiplyExact(bytes, 1024L);
					break;
				case "m":
				case "mb":
					bytes = Math.multiplyExact(bytes, 1024L * 1024L);
					break;
				case "g":
				case "gb":
					bytes = Math.multiplyExact(bytes, 1024L * 1024L * 1024L);
					break;
				default:
					throw new IllegalArgumentException("Parsing error memory size value: invalid value");
				}
			} catch (ArithmeticException ae) {
				throw new IllegalArgumentException("Parsing error memory size value: invalid value");
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
				dcmdStart.newDCmdInstance(),
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
				logJFR(results, LOGTAG_JFR_START_ORDINAL, LOGLEVEL_WARN_ORDINAL);
			}
			/*[ELSE] JAVA_SPEC_VERSION == 11 */
			String[] results = (String []) dcmdStart.getDcmdExecute().invoke(
					dcmdStart.newDCmdInstance(), "internal", jfrCMDLineOption, ',');
			if (null != results) {
				for (String result : results) {
					logJFR(result, LOGTAG_JFR_START_ORDINAL, LOGLEVEL_WARN_ORDINAL);
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
				eventWriter = Class.forName("jdk.jfr.internal.EventWriter");

				Module javabase = System.class.getModule();
				Module jdkJFR = jfrjvmClass.getModule();
				jdkJFR.implAddExports("jdk.jfr.internal", javabase);
				jdkJFR.implAddExports("jdk.jfr.internal.dcmd", javabase);
				jdkJFR.implAddExportsToAllUnnamed("jdk.jfr.internal.handlers");

				logTagValues = (Object[])logTagClass.getDeclaredMethod("values", (Class[])null).invoke(logTagClass);

				logLevelValues = (Object[])logLeveLClass.getDeclaredMethod("values", (Class[])null).invoke(logLeveLClass);
				log = loggerClass.getDeclaredMethod("log", new Class[]{logTagClass, logLeveLClass, String.class});

				/*[IF JAVA_SPEC_VERSION >= 17]*/
				logEvent = loggerClass.getDeclaredMethod("logEvent", new Class[]{logLeveLClass, String[].class, boolean.class});
				/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				/*[IF JAVA_SPEC_VERSION == 17]*/
				constructorEventWriter = eventWriter.getDeclaredConstructor(long.class, long.class, long.class, long.class, boolean.class);
				constructorEventWriter.setAccessible(true);
				/*[ENDIF] JAVA_SPEC_VERSION == 17 */

				Unsafe.getUnsafe().ensureClassInitialized(jfrjvmClass);
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
	private static Object constructorEventWriter(long startPos, long maxPos, long startPosAddress, long threadID, boolean valid) {
		try {
			return constructorEventWriter.newInstance(startPos, maxPos, startPosAddress, threadID, valid);
		} catch (Throwable t) {
			t.printStackTrace();
			throw new RuntimeException(t);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	/*[IF JAVA_SPEC_VERSION == 17]*/
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

		String flightRecorderOptions = VM.getJFRFlightRecorderOptions();
		jfrCMDLineOption = VM.getjfrCMDLineOption();
		// JFR support is enabled with V2 implementation.
		if ((null != flightRecorderOptions) || (null != jfrCMDLineOption)) {
			initJFRClasses();
			if (null != flightRecorderOptions) {
				initJFRFlightRecorderOptions(flightRecorderOptions);
			}
			if (null != jfrCMDLineOption) {
				initJFRCmdlineOptions();
			}
		}
	}

	/**
	 * Apply -XX:FlightRecorderOptions through DCmdConfigure.
	 *
	 * @param options the comma separated key=value pairs
	 */
	private static void initJFRFlightRecorderOptions(String options) {
		/*[IF JAVA_SPEC_VERSION == 17]*/
		String repositoryPath = null;
		Integer stackDepth = null;
		Long globalBufferCount = null;
		Long globalBufferSize = null;
		Long threadBufferSize = null;
		Long memorySize = null;
		Long maxChunkSize = null;
		Boolean sampleThreads = null;
		try {
			for (String pair : options.split(",")) {
				String[] keyValue = pair.split("=", 2);
				String key = keyValue[0];
				String value = (2 == keyValue.length) ? keyValue[1] : null;
				try {
					switch (key) {
					case "repository":
						repositoryPath = value;
						break;
					case "stackdepth":
						stackDepth = Integer.valueOf(value);
						break;
					case "numglobalbuffers":
						globalBufferCount = Long.valueOf(value);
						if (globalBufferCount.longValue() < MIN_GLOBAL_BUFFER_COUNT) {
							throw new IllegalArgumentException("Value specified for option \"" + key + "\" is " + value
									+ "\nThis value is lower than the minimum required number " + MIN_GLOBAL_BUFFER_COUNT);
						}
						break;
					case "globalbuffersize":
						globalBufferSize = checkMemorySizeLimit(key, value, MIN_GLOBAL_BUFFER_SIZE, MAX_GLOBAL_BUFFER_SIZE);
						break;
					case "threadbuffersize":
						threadBufferSize = checkMemorySizeLimit(key, value, MIN_THREAD_BUFFER_SIZE, MAX_THREAD_BUFFER_SIZE);
						break;
					case "memorysize":
						memorySize = checkMemorySizeLimit(key, value, MIN_MEMORY_SIZE, Long.MAX_VALUE);
						break;
					case "maxchunksize":
						maxChunkSize = convertToBytes(value);
						break;
					case "samplethreads":
						sampleThreads = parseBoolean(key, value);
						break;
					case "retransform":
						/* TODO: VM-side option, not supported yet. The value is still validated. */
						parseBoolean(key, value);
						break;
					case "old-object-queue-size":
						/* TODO: VM-side option, not supported yet. */
						break;
					default:
						throw new IllegalArgumentException("Unknown argument '" + key + "' in diagnostic command.");
					}
				} catch (NumberFormatException e) {
					throw new IllegalArgumentException("Integer parsing error in command argument '" + key
							+ "'. Could not parse: " + ((null == value) ? "" : value) + ".", e);
				}
			}
			/* Each value is within its own limits; now check the relations between the memory options. */
			checkMemoryOptionConsistency(memorySize, globalBufferSize, globalBufferCount, threadBufferSize);
		} catch (IllegalArgumentException e) {
			throw new InternalError("Failure when starting JFR: " + e.getMessage(), e);
		}
		doJFRDCmdConfigureExecute(
				false,
				repositoryPath,
				null,
				stackDepth,
				globalBufferCount,
				globalBufferSize,
				threadBufferSize,
				memorySize,
				maxChunkSize,
				sampleThreads);
		/*[ELSE] JAVA_SPEC_VERSION == 17 */
		throw new InternalError("JFR support is not enabled");
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */
	}

	/*[IF JAVA_SPEC_VERSION == 17]*/
	/**
	 * Convert a -XX:FlightRecorderOptions memory size to bytes and check it is within [minimum, maximum].
	 *
	 * @param key the option name
	 * @param value the option value, or null if no value was given
	 * @param minimum the smallest allowed size in bytes
	 * @param maximum the largest allowed size in bytes
	 * @return the size in bytes
	 */
	private static Long checkMemorySizeLimit(String key, String value, long minimum, long maximum) {
		long size = convertToBytes(value).longValue();
		if (size < minimum) {
			throw new IllegalArgumentException("Value specified for option \"" + key + "\" is " + value
					+ "\nThis value is lower than the minimum size required " + minimum);
		}
		if (size > maximum) {
			throw new IllegalArgumentException("Value specified for option \"" + key + "\" is " + value
					+ "\nThis value is higher than the maximum size limited " + maximum);
		}
		return Long.valueOf(size);
	}

	/**
	 * Parse a -XX:FlightRecorderOptions boolean, which must be true or false.
	 *
	 * @param key the option name
	 * @param value the option value
	 * @return the parsed value
	 */
	private static Boolean parseBoolean(String key, String value) {
		if ("true".equals(value)) {
			return Boolean.TRUE;
		}
		if ("false".equals(value)) {
			return Boolean.FALSE;
		}
		throw new IllegalArgumentException("Boolean parsing error in command argument '" + key
				+ "'. Could not parse: " + value + ".");
	}

	/**
	 * Check that the -XX:FlightRecorderOptions memory options are consistent with each other,
	 * where memorysize = globalbuffersize * numglobalbuffers and threadbuffersize <= globalbuffersize.
	 * Each value has already been range checked on its own. An option that was not specified
	 * is only checked where it can be derived from the specified ones:
	 *
	 * @param specifiedMemorySize memorysize in bytes, or null if not specified
	 * @param specifiedGlobalBufferSize globalbuffersize in bytes, or null if not specified
	 * @param specifiedGlobalBufferCount numglobalbuffers, or null if not specified
	 * @param specifiedThreadBufferSize threadbuffersize in bytes, or null if not specified
	 * @throws IllegalArgumentException if the options are inconsistent
	 */
	private static void checkMemoryOptionConsistency(Long specifiedMemorySize, Long specifiedGlobalBufferSize,
			Long specifiedGlobalBufferCount, Long specifiedThreadBufferSize) {
		boolean hasMemorySize = (null != specifiedMemorySize);
		boolean hasGlobalBufferSize = (null != specifiedGlobalBufferSize);
		boolean hasGlobalBufferCount = (null != specifiedGlobalBufferCount);
		boolean hasThreadBufferSize = (null != specifiedThreadBufferSize);
		long threadBufferSize = hasThreadBufferSize ? specifiedThreadBufferSize.longValue() : DEFAULT_THREAD_BUFFER_SIZE;

		if (hasGlobalBufferSize) {
			long globalBufferSize = specifiedGlobalBufferSize.longValue();
			/* A thread buffer can not be larger than a global buffer. */
			if (hasThreadBufferSize && (threadBufferSize > globalBufferSize)) {
				throw new IllegalArgumentException("The value for option \"threadbuffersize\" should not be larger than the value specified for option \"globalbuffersize\"");
			}
			if (hasGlobalBufferCount) {
				long memorySize = 0;
				try {
					memorySize = Math.multiplyExact(globalBufferSize, specifiedGlobalBufferCount.longValue());
				} catch (ArithmeticException e) {
					/* numglobalbuffers has no upper limit, so the product can overflow. */
					throw new IllegalArgumentException("\"globalbuffersize\" * \"numglobalbuffers\" is too large", e);
				}
				/* All three given: they must agree. */
				if (hasMemorySize && (memorySize != specifiedMemorySize.longValue())) {
					throw new IllegalArgumentException("These values are causing an ambiguity when trying to determine how much memory to use"
							+ "\n\"globalbuffersize\" * \"numglobalbuffers\" do not equal \"memorysize\"");
				}
				/* No memorysize: the implied memorysize must meet the minimum. */
				if (!hasMemorySize && (memorySize < MIN_MEMORY_SIZE)) {
					throw new IllegalArgumentException("\"globalbuffersize\" * \"numglobalbuffers\" (" + memorySize
							+ ") is lower than minimum memory size required " + MIN_MEMORY_SIZE);
				}
			} else if (hasMemorySize) {
				/* No numglobalbuffers: memorysize / globalbuffersize buffers must be at least the minimum count. */
				long globalBufferCount = specifiedMemorySize.longValue() / globalBufferSize;
				if (globalBufferCount < MIN_GLOBAL_BUFFER_COUNT) {
					throw new IllegalArgumentException("numglobalbuffers " + globalBufferCount + " is less than minimal value " + MIN_GLOBAL_BUFFER_COUNT
							+ "\nDecrease globalbuffersize/threadbuffersize or increase memorysize");
				}
			}
		} else if (hasMemorySize) {
			long memorySize = specifiedMemorySize.longValue();
			/* No globalbuffersize: the thread buffer must fit in memorysize. */
			if (hasThreadBufferSize && (threadBufferSize > memorySize)) {
				throw new IllegalArgumentException("The value for option \"threadbuffersize\" should not be larger than the value specified for option \"memorysize\"");
			}
			/* The globalbuffersize implied by memorysize / numglobalbuffers must fit a thread buffer. */
			if (hasGlobalBufferCount) {
				long globalBufferSize = memorySize / specifiedGlobalBufferCount.longValue();
				if (globalBufferSize < threadBufferSize) {
					throw new IllegalArgumentException("globalbuffersize " + globalBufferSize + " is less than threadbuffersize " + threadBufferSize
							+ "\nDecrease globalbuffersize or increase memorysize or adjust global/threadbuffersize");
				}
			}
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

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
				Constructor<?> dcmdConstructor = dcmdClass.getDeclaredConstructor();
				dcmdConstructor.setAccessible(true);
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
				/*[ENDIF] JAVA_SPEC_VERSION == 11 */
				{
					dcmdExecute = dcmdClass.getSuperclass().getDeclaredMethod(
							"execute",
							String.class, // source
							String.class, // arg
							char.class // delimiter
							);
				}
				dcmdExecute.setAccessible(true);
				dcmdInvocation = new DCmdInvocation(dcmdConstructor, dcmdExecute);
			} catch (Exception e) {
				throw new InternalError(e);
			}
		}
		return dcmdInvocation;
	}
	/*[ENDIF] (JAVA_SPEC_VERSION == 11) | (JAVA_SPEC_VERSION == 17) */

	/*[IF JAVA_SPEC_VERSION == 17]*/
	/**
	 * Get the message describing why Flight Recorder could not be enabled.
	 *
	 * @return the message lines, or null if jdk.jfr was available
	 */
	public static String[] getJFRModuleUnavailableMessage() {
		return jfrModuleUnavailableMessage;
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdStart.execute().
	 *
	 * @param execArgs arguments separated by ','
	 * @return A string array returned from DCmdStart.execute()
	 */
	public static String[] doJFRDCmdStartExecute(String execArgs) {
		dcmdStart = initDCmdInvocation(dcmdStart, "jdk.jfr.internal.dcmd.DCmdStart");
		try {
			return (String[]) dcmdStart.getDcmdExecute().invoke(dcmdStart.newDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdStop.execute().
	 *
	 * @param execArgs arguments separated by ','
	 * @return A string array returned from DCmdStop.execute()
	 */
	public static String[] doJFRDCmdStopExecute(String execArgs) {
		dcmdStop = initDCmdInvocation(dcmdStop, "jdk.jfr.internal.dcmd.DCmdStop");
		try {
			return (String[]) dcmdStop.getDcmdExecute().invoke(dcmdStop.newDCmdInstance(), "internal", execArgs, ',');
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
					dcmdConfigure.newDCmdInstance(),
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
	 * @param execArgs arguments separated by ','
	 * @return A string array returned from DCmdDump.execute()
	 */
	public static String[] doJFRDCmdDumpExecute(String execArgs) {
		dcmdDump = initDCmdInvocation(dcmdDump, "jdk.jfr.internal.dcmd.DCmdDump");
		try {
			return (String[]) dcmdDump.getDcmdExecute().invoke(dcmdDump.newDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}

	/**
	 * Invoke jdk.jfr.internal.dcmd.DCmdCheck.execute().
	 *
	 * @param execArgs arguments separated by ','
	 * @return A string array returned from DCmdCheck.execute()
	 */
	public static String[] doJFRDCmdCheckExecute(String execArgs) {
		dcmdCheck = initDCmdInvocation(dcmdCheck, "jdk.jfr.internal.dcmd.DCmdCheck");
		try {
			return (String[]) dcmdCheck.getDcmdExecute().invoke(dcmdCheck.newDCmdInstance(), "internal", execArgs, ',');
		} catch (InvocationTargetException e) {
			throw new InternalError(e.getCause());
		} catch (Exception e) {
			throw new InternalError(e);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */
}
