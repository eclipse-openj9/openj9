/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 1998
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

import com.ibm.oti.util.Msg;

import java.io.*;
/*[IF Sidecar18-SE-OpenJ9 | JAVA_SPEC_VERSION >= 11]*/
import java.nio.charset.Charset;
/*[ENDIF] Sidecar18-SE-OpenJ9 | JAVA_SPEC_VERSION >= 11 */
import java.util.Map;
import java.util.Properties;
import java.util.PropertyPermission;
import java.security.*;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;

/*[IF JAVA_SPEC_VERSION >= 24]*/
import jdk.internal.javac.Restricted;
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
/*[IF JAVA_SPEC_VERSION >= 9]*/
import jdk.internal.misc.Unsafe;
/*[IF JAVA_SPEC_VERSION > 11]*/
import jdk.internal.access.SharedSecrets;
/*[ELSE] JAVA_SPEC_VERSION > 11
import jdk.internal.misc.SharedSecrets;
/*[ENDIF] JAVA_SPEC_VERSION > 11 */
import jdk.internal.misc.VM;
import java.lang.StackWalker.Option;
import jdk.internal.reflect.Reflection;
import jdk.internal.reflect.CallerSensitive;
import java.util.*;
import java.util.function.*;
import com.ibm.gpu.spi.GPUAssist;
import com.ibm.gpu.spi.GPUAssistHolder;
/*[ELSE] JAVA_SPEC_VERSION >= 9 */
import sun.misc.Unsafe;
import sun.misc.SharedSecrets;
import sun.misc.VM;
import sun.reflect.CallerSensitive;
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
/*[IF PLATFORM-mz31 | PLATFORM-mz64 | !Sidecar18-SE-OpenJ9]*/
import com.ibm.jvm.io.ConsolePrintStream;
/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 | !Sidecar18-SE-OpenJ9 */

/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.lang.reflect.Field;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
import jdk.internal.util.SystemProps;
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

/*[IF JAVA_SPEC_VERSION >= 24]*/
import java.net.URL;
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

/**
 * Class System provides a standard place for programs
 * to find system related information. All System API
 * is static.
 *
 * @author		OTI
 * @version		initial
 */
public final class System {

	// The standard input, output, and error streams.
	// Typically, these are connected to the shell which
	// ran the Java program.
	/**
	 * Default input stream
	 */
	public static final InputStream in = null;

/*[IF JAVA_SPEC_VERSION >= 20]*/
	static InputStream initialIn;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	/**
	 * Default output stream
	 */
	public static final PrintStream out = null;
	/**
	 * Default error output stream
	 */
	public static final PrintStream err = null;
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	// The initial err to print SecurityManager related warning messages
	static PrintStream initialErr;
	// Show only one setSecurityManager() warning message for each caller
	private static Map<Class<?>, Object> setSMCallers;
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	/*[IF JAVA_SPEC_VERSION > 11]*/
	/**
	 * setSecurityManager() should throw UnsupportedOperationException
	 * if throwUOEFromSetSM is set to true.
	 */
	private static boolean throwUOEFromSetSM;
	/*[ENDIF] JAVA_SPEC_VERSION > 11 */

	//Get a ref to the Runtime instance for faster lookup
	private static final Runtime RUNTIME = Runtime.getRuntime();
	/**
	 * The System Properties table.
	 */
	private static Properties systemProperties;

	/*[IF JAVA_SPEC_VERSION < 24]*/
	/**
	 * The System default SecurityManager.
	 */
	private static SecurityManager security;
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	private static volatile Console console;
	private static volatile boolean consoleInitialized;

	private static String lineSeparator;

	private static boolean propertiesInitialized;

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	private static final int sysPropID_OSVersion = 0;
	private static final String sysPropOSVersion;
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	private static final int sysPropID_PlatformEncoding = 1;
	private static String platformEncoding;

	private static final int sysPropID_FileEncoding = 2;
	private static String fileEncoding;

	private static final int sysPropID_OSEncoding = 3;
	private static String osEncoding;

	/*[IF (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64)]*/
	private static final int sysPropID_zOSAutoConvert = 4;
	private static String zOSAutoConvert;
	/*[ENDIF] (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64) */

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	private static boolean hasSetErrEncoding;
	private static boolean hasSetOutEncoding;
	/*[IF JAVA_SPEC_VERSION < 24]*/
	private static String consoleDefaultEncoding;
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	/* The consoleDefaultCharset is different from the default console encoding when the encoding
	 * doesn't exist, or isn't available at startup. Some character sets are not available in the
	 * java.base module, there are more in the jdk.charsets module, and so are not used at startup.
	 */
	private static Charset consoleDefaultCharset;
	/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

/*[IF JAVA_SPEC_VERSION >= 9]*/
	static java.lang.ModuleLayer	bootLayer;
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

	// Initialize all the slots in System on first use.
	static {
		/* Get following system properties in clinit and make it available via static variables
		 * at early boot stage in which System is not fully initialized
		 * os.version, os.encoding, ibm.system.encoding/sun.jnu.encoding, file.encoding
		 */
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		sysPropOSVersion = getSysPropBeforePropertiesInitialized(sysPropID_OSVersion);
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		platformEncoding = getSysPropBeforePropertiesInitialized(sysPropID_PlatformEncoding);
		String definedOSEncoding = getSysPropBeforePropertiesInitialized(sysPropID_OSEncoding);
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		/* -Dfile.encoding=COMPAT is mapped to platform encoding within getSysPropBeforePropertiesInitialized.
		 * During early VM boot stage, JITHelpers hasn't been initialized yet, COMPACT_STRINGS related APIs
		 * might not work properly.
		 */
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		String definedFileEncoding = getSysPropBeforePropertiesInitialized(sysPropID_FileEncoding);
		if (definedFileEncoding != null) {
			fileEncoding = definedFileEncoding;
			// if file.encoding is defined, and os.encoding is not, use the detected
			// platform encoding for os.encoding
			if (definedOSEncoding == null) {
				osEncoding = platformEncoding;
			}
		} else {
			fileEncoding = platformEncoding;
		}
		// if os.encoding is not defined, file.encoding will be used
		if (osEncoding == null) {
			osEncoding = definedOSEncoding;
		}

		/*[IF (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64)]*/
		/* As part of better handling of JEP400 constraints on z/OS, the com.ibm.autocvt property
		 * determines whether we convert input file I/O based on file tagging. If not explicitly specified,
		 * the property defaults to true, unless file.encoding is set to COMPAT.
		 */
		zOSAutoConvert = getSysPropBeforePropertiesInitialized(sysPropID_zOSAutoConvert);
		/*[ENDIF] (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64) */
	}

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	/*
	 * Return the Charset of the primary or when fallback is true, the default console encoding,
	 * if different from the default console Charset.
	 *
	 * consoleDefaultCharset must be initialized before calling.
	/*[IF JAVA_SPEC_VERSION < 24]
	 * consoleDefaultEncoding must be initialized before calling with fallback set to true.
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	static Charset getCharset(boolean isStdout, boolean fallback) {
		/*[IF JAVA_SPEC_VERSION >= 19]*/
		String primary = internalGetProperties().getProperty(isStdout ? "stdout.encoding" : "stderr.encoding"); //$NON-NLS-1$  //$NON-NLS-2$
		/*[ELSE] JAVA_SPEC_VERSION >= 19 */
		String primary = internalGetProperties().getProperty(isStdout ? "sun.stdout.encoding" : "sun.stderr.encoding"); //$NON-NLS-1$  //$NON-NLS-2$
		/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		if (primary != null) {
			try {
				Charset newCharset = Charset.forName(primary);
				if (newCharset.equals(consoleDefaultCharset)) {
					return null;
				} else {
					return newCharset;
				}
			} catch (IllegalArgumentException e) {
				// ignore unsupported or invalid encodings
			}
		}
		/*[IF JAVA_SPEC_VERSION < 24]*/
		if (fallback && (consoleDefaultEncoding != null)) {
			try {
				Charset newCharset = Charset.forName(consoleDefaultEncoding);
				if (newCharset.equals(consoleDefaultCharset)) {
					return null;
				} else {
					return newCharset;
				}
			} catch (IllegalArgumentException e) {
				// ignore unsupported or invalid encodings
			}
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		return null;
	}

	/*
	 * Return the appropriate System console using the requested Charset.
	 * If the Charset is null use the default console Charset.
	 *
	 * consoleDefaultCharset must be initialized before calling.
	 */
	static PrintStream createConsole(FileDescriptor desc, Charset charset) {
		BufferedOutputStream bufStream = new BufferedOutputStream(new FileOutputStream(desc));
		Charset consoleCharset = charset == null ? consoleDefaultCharset : charset;

		/*[IF PLATFORM-mz31 | PLATFORM-mz64]*/
		return ConsolePrintStream.localize(bufStream, true, consoleCharset);
		/*[ELSE]*/
		return new PrintStream(bufStream, true, consoleCharset);
		/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 */
	}

	static void finalizeConsoleEncoding() {
		/* Some character sets are not available in the java.base module and so are not used at startup. There
		 * are additional character sets in the jdk.charsets module, which is only loaded later. This means the
		 * default character set may not be the same as the desired encoding. When all modules and character sets
		 * are available, check if the desired encodings can be used for System.err and System.out.
		 */
		// If the encoding property was already set, don't change the encoding.
		if (!hasSetErrEncoding) {
			Charset stderrCharset = getCharset(false, true);
			if (stderrCharset != null) {
				err.flush();
				setErr(createConsole(FileDescriptor.err, stderrCharset));
			}
		}

		// If the encoding property was already set, don't change the encoding.
		if (!hasSetOutEncoding) {
			Charset stdoutCharset = getCharset(true, true);
			if (stdoutCharset != null) {
				out.flush();
				setOut(createConsole(FileDescriptor.out, stdoutCharset));
			}
		}
	}
	/*[ELSE]*/
	/*[IF Sidecar18-SE-OpenJ9]*/
	/*
	 * Return the Charset name of the primary encoding, if different from the default.
	 */
	private static String getCharsetName(String primary, Charset defaultCharset) {
		if (primary != null) {
			try {
				Charset newCharset = Charset.forName(primary);
				if (newCharset.equals(defaultCharset)) {
					return null;
				} else {
					return newCharset.name();
				}
			} catch (IllegalArgumentException e) {
				// ignore unsupported or invalid encodings
			}
		}
		return null;
	}

	/*
	 * Return the appropriate System console using the pre-validated encoding.
	 */
	private static PrintStream createConsole(FileDescriptor desc, String encoding) {
		BufferedOutputStream bufStream = new BufferedOutputStream(new FileOutputStream(desc));
		if (encoding == null) {
			return new PrintStream(bufStream, true);
		} else {
			try {
				return new PrintStream(bufStream, true, encoding);
			} catch (UnsupportedEncodingException e) {
				// not possible when the Charset name is validated first
				throw new InternalError("For encoding " + encoding, e); //$NON-NLS-1$
			}
		}
	}
	/*[ELSE]*/
	/*
	 * Return the appropriate System console.
	 */
	private static PrintStream createConsole(FileDescriptor desc) {
		return ConsolePrintStream.localize(new BufferedOutputStream(new FileOutputStream(desc)), true);
	}
	/*[ENDIF] Sidecar18-SE-OpenJ9 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

	static void afterClinitInitialization() {
		/*[PR CMVC 189091] Perf: EnumSet.allOf() is slow */
		/*[PR CMVC 191554] Provide access to ClassLoader methods to improve performance */
		/* Multitenancy change: only initialize VMLangAccess once as it's non-isolated */
		if (null == com.ibm.oti.vm.VM.getVMLangAccess()) {
			com.ibm.oti.vm.VM.setVMLangAccess(new VMAccess());
		}
		SharedSecrets.setJavaLangAccess(new Access());

		// Fill in the properties from the VM information.
		ensureProperties(true);

		/*[REM] Initialize the JITHelpers needed in J9VMInternals since the class can't do it itself */
		try {
			java.lang.reflect.Field f1 = J9VMInternals.class.getDeclaredField("jitHelpers"); //$NON-NLS-1$
			java.lang.reflect.Field f2 = String.class.getDeclaredField("helpers"); //$NON-NLS-1$

			Unsafe unsafe = Unsafe.getUnsafe();

			unsafe.putObject(unsafe.staticFieldBase(f1), unsafe.staticFieldOffset(f1), com.ibm.jit.JITHelpers.getHelpers());
			unsafe.putObject(unsafe.staticFieldBase(f2), unsafe.staticFieldOffset(f2), com.ibm.jit.JITHelpers.getHelpers());
		} catch (NoSuchFieldException e) { }

		/*[IF JAVA_SPEC_VERSION < 17]*/
		/**
		 * When the System Property == true, then disable sharing (i.e. arraycopy the underlying value array) in
		 * String.substring(int) and String.substring(int, int) methods whenever offset is zero. Otherwise, enable
		 * sharing of the underlying value array.
		 */
		String enableSharingInSubstringWhenOffsetIsZeroProperty = internalGetProperties().getProperty("java.lang.string.substring.nocopy"); //$NON-NLS-1$
		String.enableSharingInSubstringWhenOffsetIsZero = enableSharingInSubstringWhenOffsetIsZeroProperty == null || enableSharingInSubstringWhenOffsetIsZeroProperty.equalsIgnoreCase("false"); //$NON-NLS-1$
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */

		/*[IF JAVA_SPEC_VERSION == 8]*/
		// Check the default encoding
		/*[Bug 102075] J2SE Setting -Dfile.encoding=junk fails to run*/
		StringCoding.encode(new char[1], 0, 1);
		/*[ENDIF] JAVA_SPEC_VERSION == 8 */

		// Set up standard in, out, and err.
		/*[IF Sidecar18-SE-OpenJ9]*/
		Properties props = internalGetProperties();
		/*[IF JAVA_SPEC_VERSION >= 11]*/
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		/*[IF JAVA_SPEC_VERSION >= 24]*/
		consoleDefaultCharset = sun.nio.cs.UTF_8.INSTANCE;
		/*[ELSE] JAVA_SPEC_VERSION >= 24 */
		consoleDefaultEncoding = props.getProperty("native.encoding"); //$NON-NLS-1$
		consoleDefaultCharset = Charset.forName(consoleDefaultEncoding, sun.nio.cs.UTF_8.INSTANCE);
		/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
		/*[ELSE] JAVA_SPEC_VERSION >= 18 */
		String fileEncodingProp = props.getProperty("file.encoding"); //$NON-NLS-1$
		// Do not call Charset.defaultEncoding() since this would initialize the default encoding
		// before the jdk.charset module is loaded.
		try {
			if (Charset.isSupported(fileEncodingProp)) {
				consoleDefaultCharset = Charset.forName(fileEncodingProp);
			}
		} catch (IllegalArgumentException e) {
			// ignore
		}
		if (consoleDefaultCharset == null) {
			consoleDefaultCharset = sun.nio.cs.UTF_8.INSTANCE;
		}
		/*[IF PLATFORM-mz31|PLATFORM-mz64]*/
		try {
			consoleDefaultEncoding = props.getProperty("console.encoding"); //$NON-NLS-1$
			if (consoleDefaultEncoding == null) {
				consoleDefaultEncoding = props.getProperty("ibm.system.encoding"); //$NON-NLS-1$
			}
			consoleDefaultCharset = Charset.forName(consoleDefaultEncoding); //$NON-NLS-1$
		} catch (IllegalArgumentException e) {
			// use the defaultCharset()
		}
		/*[ELSE] PLATFORM-mz31|PLATFORM-mz64 */
		consoleDefaultEncoding = fileEncodingProp;
		/*[ENDIF] PLATFORM-mz31|PLATFORM-mz64 */
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		/* consoleDefaultCharset must be initialized before calling getCharset() */
		Charset stdoutCharset = getCharset(true, false);
		Charset stderrCharset = getCharset(false, false);
		/*[ELSE] JAVA_SPEC_VERSION >= 11 */
		Charset consoleCharset = Charset.defaultCharset();
		String stdoutCharset = getCharsetName(props.getProperty("sun.stdout.encoding"), consoleCharset); //$NON-NLS-1$
		String stderrCharset = getCharsetName(props.getProperty("sun.stderr.encoding"), consoleCharset); //$NON-NLS-1$
		/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
		/*[ENDIF] Sidecar18-SE-OpenJ9 */

		/*[IF Sidecar18-SE-OpenJ9]*/
		/*[IF JAVA_SPEC_VERSION >= 11]*/
		hasSetErrEncoding = stderrCharset != null;
		hasSetOutEncoding = stdoutCharset != null;
		/* consoleDefaultCharset must be initialized before calling createConsole() */
		/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		initialErr = createConsole(FileDescriptor.err, stderrCharset);
		setErr(initialErr);
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		setErr(createConsole(FileDescriptor.err, stderrCharset));
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		setOut(createConsole(FileDescriptor.out, stdoutCharset));
		/*[IF Sidecar19-SE_RAWPLUSJ9]*/
		setIn(new BufferedInputStream(new FileInputStream(FileDescriptor.in)));
		/*[ENDIF] Sidecar19-SE_RAWPLUSJ9 */
		/*[ELSE]*/
		/*[PR s66168] - ConsoleInputStream initialization may write to System.err */
		/*[PR s73550, s74314] ConsolePrintStream incorrectly initialized */
		setErr(createConsole(FileDescriptor.err));
		setOut(createConsole(FileDescriptor.out));
		/*[ENDIF] Sidecar18-SE-OpenJ9 */

		/*[IF JAVA_SPEC_VERSION >= 17]*/
		setSMCallers = Collections.synchronizedMap(new WeakHashMap<>());
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	}

native static void startSNMPAgent();

static void completeInitialization() {
	/*[IF !Sidecar19-SE_RAWPLUSJ9]*/
	/*[IF !Sidecar18-SE-OpenJ9]*/
	Class<?> systemInitialization = null;
	Method hook;
	try {
		/*[PR 167854] - Please note the incorrect name of the class is correct - somewhere the spelling was wrong and we just need to use the incorrect spelling */
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		systemInitialization = Class.forName("com.ibm.utils.SystemIntialization"); //$NON-NLS-1$
		/*[ELSE] JAVA_SPEC_VERSION >= 9 */
		systemInitialization = Class.forName("com.ibm.misc.SystemIntialization"); //$NON-NLS-1$
		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
	} catch (ClassNotFoundException e) {
		// Assume this is a raw configuration and suppress the exception
	} catch (Exception e) {
		throw new InternalError(e.toString());
	}
	try {
		if (null != systemInitialization) {
			hook = systemInitialization.getMethod("firstChanceHook");	//$NON-NLS-1$
			hook.invoke(null);
		}
	} catch (Exception e) {
		throw new InternalError(e.toString());
	}
	/*[ENDIF] !Sidecar18-SE-OpenJ9 */

	/*[IF Sidecar18-SE-OpenJ9 & !( PLATFORM-mz31 | PLATFORM-mz64 )]*/
	InputStream tempIn = new BufferedInputStream(new FileInputStream(FileDescriptor.in));
	setIn(tempIn);
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	initialIn = tempIn;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	/*[ELSE] Sidecar18-SE-OpenJ9 & !( PLATFORM-mz31 | PLATFORM-mz64 ) */
	/*[PR 100718] Initialize System.in after the main thread*/
	setIn(com.ibm.jvm.io.ConsoleInputStream.localize(new BufferedInputStream(new FileInputStream(FileDescriptor.in))));
	/*[ENDIF] Sidecar18-SE-OpenJ9 & !( PLATFORM-mz31 | PLATFORM-mz64 ) */
	/*[ENDIF] !Sidecar19-SE_RAWPLUSJ9 */

	/*[PR 102344] call Terminator.setup() after Thread init */
	Terminator.setup();

	/*[IF !Sidecar19-SE_RAWPLUSJ9&!Sidecar18-SE-OpenJ9]*/
	try {
		if (null != systemInitialization) {
			hook = systemInitialization.getMethod("lastChanceHook");	//$NON-NLS-1$
			hook.invoke(null);
		}
	} catch (Exception e) {
		throw new InternalError(e.toString());
	}
	/*[ENDIF]*/	//!Sidecar19-SE_RAWPLUSJ9&!Sidecar18-SE-OpenJ9

	/*[IF JFR_SUPPORT]*/
	JFRHelpers.initJFR();
	/*[ENDIF] JFR_SUPPORT */
}

/*[IF JAVA_SPEC_VERSION >= 9]*/
static void initGPUAssist() {
	Properties props = internalGetProperties();

	if ((props.getProperty("com.ibm.gpu.enable") == null) //$NON-NLS-1$
	&& (props.getProperty("com.ibm.gpu.enforce") == null) //$NON-NLS-1$
	) {
		/*
		 * The CUDA implementation of GPUAssist is not enabled by default:
		 * one of the above properties must be set.
		 */
		return;
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	PrivilegedAction<GPUAssist> finder = new PrivilegedAction<GPUAssist>() {
		@Override
		public GPUAssist run() {
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
			ServiceLoader<GPUAssist.Provider> loaded = ServiceLoader.load(GPUAssist.Provider.class);
			GPUAssist assist = null;
			for (GPUAssist.Provider provider : loaded) {
				assist = provider.getGPUAssist();

				if (assist != null) {
					break;
				}
			}

			if (null == assist) {
				assist = GPUAssist.NONE;
			}
	/*[IF JAVA_SPEC_VERSION >= 24]*/
			GPUAssistHolder.instance = assist;
	/*[ELSE] JAVA_SPEC_VERSION >= 24 */
			return assist;
		}
	};

	GPUAssistHolder.instance = AccessController.doPrivileged(finder);
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
}
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

/*[IF JAVA_SPEC_VERSION >= 24]*/
static URL codeSource(Class<?> callerClass) {
	CodeSource codeSource = callerClass.getProtectionDomainInternal().getCodeSource();
	return (codeSource == null) ? null : codeSource.getLocation();
}
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

/**
 * Sets the value of the static slot "in" in the receiver
 * to the passed in argument.
 *
 * @param		newIn 		the new value for in.
 */
public static void setIn(InputStream newIn) {
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	setFieldImpl("in", newIn); //$NON-NLS-1$
}

/**
 * Sets the value of the static slot "out" in the receiver
 * to the passed in argument.
 *
 * @param		newOut 		the new value for out.
 */
public static void setOut(java.io.PrintStream newOut) {
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	setFieldImpl("out", newOut); //$NON-NLS-1$
}

/**
 * Sets the value of the static slot "err" in the receiver
 * to the passed in argument.
 *
 * @param		newErr  	the new value for err.
 */
public static void setErr(java.io.PrintStream newErr) {
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	setFieldImpl("err", newErr); //$NON-NLS-1$
}

/**
 * Prevents this class from being instantiated.
 */
private System() {
}

/**
 * Copies the contents of <code>array1</code> starting at offset <code>start1</code>
 * into <code>array2</code> starting at offset <code>start2</code> for
 * <code>length</code> elements.
 *
 * @param		array1 		the array to copy out of
 * @param		start1 		the starting index in array1
 * @param		array2 		the array to copy into
 * @param		start2 		the starting index in array2
 * @param		length 		the number of elements in the array to copy
 */
public static native void arraycopy(Object array1, int start1, Object array2, int start2, int length);

/**
 * Private version of the arraycopy method used by the jit for reference arraycopies
 */
private static void arraycopy(Object[] A1, int offset1, Object[] A2, int offset2, int length) {
	if (A1 == null || A2 == null) throw new NullPointerException();
	if (offset1 >= 0 && offset2 >= 0 && length >= 0 &&
		length <= A1.length - offset1 && length <= A2.length - offset2)
	{
		// Check if this is a forward or backwards arraycopy
		if (A1 != A2 || offset1 > offset2 || offset1+length <= offset2) {
			for (int i = 0; i < length; ++i) {
				A2[offset2+i] = A1[offset1+i];
			}
		} else {
			for (int i = length-1; i >= 0; --i) {
				A2[offset2+i] = A1[offset1+i];
			}
		}
	} else throw new ArrayIndexOutOfBoundsException();
}

/**
 * Answers the current time expressed as milliseconds since
 * the time 00:00:00 UTC on January 1, 1970.
 *
 * @return		the time in milliseconds.
 */
public static native long currentTimeMillis();

/*[IF JAVA_SPEC_VERSION == 11]*/
private static native Properties initProperties(Properties props);
/*[ENDIF] JAVA_SPEC_VERSION == 11 */

/**
 * If systemProperties is unset, then create a new one based on the values
 * provided by the virtual machine.
 */
@SuppressWarnings("nls")
private static void ensureProperties(boolean isInitialization) {
	/*[IF JAVA_SPEC_VERSION == 11]*/
	Properties jclProps = new Properties();
	initProperties(jclProps);
	/*[ENDIF] JAVA_SPEC_VERSION == 11 */

	/*[IF JAVA_SPEC_VERSION > 11]*/
	Map<String, String> initializedProperties = new HashMap<>();
	/*[ELSE] JAVA_SPEC_VERSION > 11
	Properties initializedProperties = new Properties();
	/*[ENDIF] JAVA_SPEC_VERSION > 11 */

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	initializedProperties.put("os.version", sysPropOSVersion); //$NON-NLS-1$
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	if (osEncoding != null) {
		initializedProperties.put("os.encoding", osEncoding); //$NON-NLS-1$
	}
	initializedProperties.put("ibm.system.encoding", platformEncoding); //$NON-NLS-1$
	/*[IF JAVA_SPEC_VERSION == 8]*/
	/*[PR The launcher apparently needs sun.jnu.encoding property or it does not work]*/
	initializedProperties.put("sun.jnu.encoding", platformEncoding); //$NON-NLS-1$
	initializedProperties.put("file.encoding.pkg", "sun.io"); //$NON-NLS-1$ //$NON-NLS-2$
	/* System property java.specification.vendor is set via VersionProps.init(systemProperties) since JDK12 */
	initializedProperties.put("java.specification.vendor", "Oracle Corporation"); //$NON-NLS-1$ //$NON-NLS-2$
	/*[ENDIF] JAVA_SPEC_VERSION == 8 */
	initializedProperties.put("java.specification.name", "Java Platform API Specification"); //$NON-NLS-1$ //$NON-NLS-2$
	initializedProperties.put("com.ibm.oti.configuration", "scar"); //$NON-NLS-1$

	/*[IF CRIU_SUPPORT]*/
	initializedProperties.put("org.eclipse.openj9.criu.isCRIUCapable", "true"); //$NON-NLS-1$ //$NON-NLS-2$
	// CRIU support is required by CRaC
	/*[IF CRAC_SUPPORT]*/
	initializedProperties.put("org.eclipse.openj9.criu.isCRaCCapable", "true"); //$NON-NLS-1$ //$NON-NLS-2$
	/*[ENDIF] CRAC_SUPPORT */
	/*[ENDIF] CRIU_SUPPORT */

	/*[IF JFR_SUPPORT]*/
	/* Enables openj9 JFR tests. */
	initializedProperties.put("org.eclipse.openj9.jfr.isJFREnabled", "true"); //$NON-NLS-1$ //$NON-NLS-2$
	/* TODO disable JFR JCL APIs until JFR natives are implemented. */
	initializedProperties.put("jfr.unsupported.vm", "true"); //$NON-NLS-1$ //$NON-NLS-2$
	/*[ENDIF] JFR_SUPPORT */

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	initializedProperties.putAll(SystemProps.initProperties());
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	String[] list = getPropertyList();
	for (int i = 0; i < list.length; i += 2) {
		String key = list[i];
		/*[PR 100209] getPropertyList should use fewer local refs */
		if (key == null) {
			break;
		}
		initializedProperties.put(key, list[i+1]);
	}
	/*[IF JAVA_SPEC_VERSION == 11]*/
	for (Map.Entry<?, ?> entry : jclProps.entrySet()) {
		initializedProperties.putIfAbsent(entry.getKey(), entry.getValue());
	}
	/*[ELSE] JAVA_SPEC_VERSION == 11 */
	initializedProperties.put("file.encoding", fileEncoding); //$NON-NLS-1$
	/*[ENDIF] JAVA_SPEC_VERSION == 11 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	/*[IF (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64)]*/
	initializedProperties.put("com.ibm.autocvt", zOSAutoConvert); //$NON-NLS-1$
	/*[ENDIF] (JAVA_SPEC_VERSION >= 21) & (PLATFORM-mz31 | PLATFORM-mz64) */

	/* java.lang.VersionProps.init() eventually calls into System.setProperty() where propertiesInitialized needs to be true */
	propertiesInitialized = true;

/*[IF JAVA_SPEC_VERSION > 11]*/
	java.lang.VersionProps.init(initializedProperties);
/*[ELSE] JAVA_SPEC_VERSION > 11
	/* VersionProps.init requires systemProperties to be set */
	systemProperties = initializedProperties;

/*[IF JAVA_SPEC_VERSION >= 9]*/
	java.lang.VersionProps.init();
/*[ELSE] JAVA_SPEC_VERSION >= 9 */
	sun.misc.Version.init();

	StringBuffer.initFromSystemProperties(systemProperties);
	StringBuilder.initFromSystemProperties(systemProperties);
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
/*[ENDIF] JAVA_SPEC_VERSION > 11 */

/*[IF JAVA_SPEC_VERSION > 11]*/
	String javaRuntimeVersion = initializedProperties.get("java.runtime.version"); //$NON-NLS-1$
/*[ELSE] JAVA_SPEC_VERSION > 11
	String javaRuntimeVersion = initializedProperties.getProperty("java.runtime.version"); //$NON-NLS-1$
/*[ENDIF] JAVA_SPEC_VERSION > 11 */
	if (null != javaRuntimeVersion) {
	/*[IF JAVA_SPEC_VERSION > 11]*/
		String fullVersion = initializedProperties.get("java.fullversion"); //$NON-NLS-1$
	/*[ELSE] JAVA_SPEC_VERSION > 11
		String fullVersion = initializedProperties.getProperty("java.fullversion"); //$NON-NLS-1$
	/*[ENDIF] JAVA_SPEC_VERSION > 11 */
		if (null != fullVersion) {
			initializedProperties.put("java.fullversion", (javaRuntimeVersion + "\n" + fullVersion)); //$NON-NLS-1$ //$NON-NLS-2$
		}
		rasInitializeVersion(javaRuntimeVersion);
	}

/*[IF JAVA_SPEC_VERSION > 11]*/
	lineSeparator = initializedProperties.getOrDefault("line.separator", "\n"); //$NON-NLS-1$
/*[ELSE] JAVA_SPEC_VERSION > 11
	lineSeparator = initializedProperties.getProperty("line.separator", "\n"); //$NON-NLS-1$
/*[ENDIF] JAVA_SPEC_VERSION > 11 */

	if (isInitialization) {
		/*[PR CMVC 179976] System.setProperties(null) throws IllegalStateException */
		/*[IF JAVA_SPEC_VERSION > 11]*/
		VM.saveProperties(initializedProperties);
		/*[ELSE] JAVA_SPEC_VERSION > 11
		VM.saveAndRemoveProperties(initializedProperties);
		/*[ENDIF] JAVA_SPEC_VERSION > 11 */
	}

	/* create systemProperties from properties Map */
/*[IF JAVA_SPEC_VERSION > 11]*/
	initializeSystemProperties(initializedProperties);
/*[ELSE] JAVA_SPEC_VERSION > 11
	systemProperties = initializedProperties;
/*[ENDIF] JAVA_SPEC_VERSION > 11 */

	/* Preload system property jdk.serialFilter to prevent later modification */
	jdk.internal.util.StaticProperty.jdkSerialFilter();
}

/*[IF JAVA_SPEC_VERSION > 11]*/
/* Converts a Map<String, String> to a properties object.
 *
 * The system properties will be initialized as a Map<String, String> type to be compatible
 * with jdk.internal.misc.VM and java.lang.VersionProps APIs.
 */
private static void initializeSystemProperties(Map<String, String> mapProperties) {
	systemProperties = new Properties();
	for (Map.Entry<String, String> property : mapProperties.entrySet()) {
		String key = property.getKey();
		/* Remove OpenJDK private properties that should not be System properties. */
		switch (key) {
		case "java.lang.Integer.IntegerCache.high": //$NON-NLS-1$
		case "jdk.boot.class.path.append": //$NON-NLS-1$
		case "sun.java.launcher.diag": //$NON-NLS-1$
		case "sun.nio.MaxDirectMemorySize": //$NON-NLS-1$
		case "sun.nio.PageAlignDirectMemory": //$NON-NLS-1$
			continue;
		default:
			break;
		}
		systemProperties.put(key, property.getValue());
	}
}
/*[ENDIF] JAVA_SPEC_VERSION > 11 */

private static native void rasInitializeVersion(String javaRuntimeVersion);

/**
 * Causes the virtual machine to stop running, and the
 * program to exit.
 *
 * @param		code		the return code.
 *
/*[IF JAVA_SPEC_VERSION < 24]
 * @throws		SecurityException 	if the running thread is not allowed to cause the vm to exit.
 *
 * @see			SecurityManager#checkExit
/*[ENDIF] JAVA_SPEC_VERSION < 24
 */
public static void exit(int code) {
	RUNTIME.exit(code);
}

/**
 * Indicate to the virtual machine that it would be a
 * good time to collect available memory. Note that, this
 * is a hint only.
 */
public static void gc() {
	RUNTIME.gc();
}

/*[PR 127013] [JCL Desktop]VM throws runtime exception(no such method signature) when calling System.getenv() function (SPR MXZU76W5R5)*/
/**
 * Returns an environment variable.
 *
 * @param 		var			the name of the environment variable
 * @return		the value of the specified environment variable
 */
@SuppressWarnings("dep-ann")
public static String getenv(String var) {
	if (var == null) throw new NullPointerException();
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(new RuntimePermission("getenv." + var)); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	return ProcessEnvironment.getenv(var);
}

/**
 * Answers the system properties. Note that this is
 * not a copy, so that changes made to the returned
 * Properties object will be reflected in subsequent
 * calls to {@code getProperty()} and {@code getProperties()}.
/*[IF JAVA_SPEC_VERSION < 24]
 * <p>
 * Security managers should restrict access to this
 * API if possible.
/*[ENDIF] JAVA_SPEC_VERSION < 24
 *
 * @return		the system properties
 */
public static Properties getProperties() {
	if (!propertiesInitialized) throw new Error("bootstrap error, system property access before init"); //$NON-NLS-1$
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPropertiesAccess();
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	return systemProperties;
}

/**
 * Answers the system properties without any security
 * checks. This is used for access from within java.lang.
 *
 * @return		the system properties
 */
static Properties internalGetProperties() {
	if (!propertiesInitialized) throw new Error("bootstrap error, system property access before init");	 //$NON-NLS-1$

	return systemProperties;
}
/**
 * Answers the value of a particular system property.
 * Answers null if no such property exists.
 * <p>
 * The properties currently provided by the virtual
 * machine are:
 * <pre>
 *     file.separator
 *     java.class.path
 *     java.class.version
 *     java.home
/*[IF JAVA_SPEC_VERSION < 12]
 *     java.vendor
 *     java.vendor.url
/*[ENDIF] JAVA_SPEC_VERSION < 12
 *     java.version
 *     line.separator
 *     os.arch
 *     os.name
 *     os.version
 *     path.separator
 *     user.dir
 *     user.home
 *     user.name
 *     user.timezone
 * </pre>
 *
 * @param		prop		the system property to look up
 * @return		the value of the specified system property, or null if the property doesn't exist
 */
@SuppressWarnings("nls")
public static String getProperty(String prop) {
	return getProperty(prop, null);
}

/**
 * Answers the value of a particular system property.
 * If no such property is found, answers the defaultValue.
 *
 * @param		prop			the system property to look up
 * @param		defaultValue	return value if system property is not found
 * @return		the value of the specified system property, or defaultValue if the property doesn't exist
 */
public static String getProperty(String prop, String defaultValue) {
	if (prop.length() == 0) throw new IllegalArgumentException();

	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPropertyAccess(prop);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	if (!propertiesInitialized
			&& !prop.equals("com.ibm.IgnoreMalformedInput") //$NON-NLS-1$
			&& !prop.equals("sun.nio.cs.map") //$NON-NLS-1$
	) {
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		if (prop.equals("os.version")) { //$NON-NLS-1$
			return sysPropOSVersion;
		} else
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		if (prop.equals("os.encoding")) { //$NON-NLS-1$
			return osEncoding;
		} else if (prop.equals("ibm.system.encoding")) { //$NON-NLS-1$
			return platformEncoding;
		} else if (prop.equals("file.encoding")) { //$NON-NLS-1$
			return fileEncoding;
		}
		throw new Error("bootstrap error, system property access before init: " + prop); //$NON-NLS-1$
	}

	return systemProperties.getProperty(prop, defaultValue);
}

/**
 * Sets the value of a particular system property.
 *
 * @param		prop		the system property to change
 * @param		value		the value to associate with prop
 * @return		the old value of the property, or null
 */
public static String setProperty(String prop, String value) {
	if (!propertiesInitialized) throw new Error("bootstrap error, system property access before init: " + prop); //$NON-NLS-1$

	/*[PR CMVC 80288] should check for empty key */
	if (prop.length() == 0) throw new IllegalArgumentException();

	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(new PropertyPermission(prop, "write")); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	return (String)systemProperties.setProperty(prop, value);
}

/*[IF JAVA_SPEC_VERSION < 17]*/
/**
 * Answers an array of Strings containing key..value pairs
 * (in consecutive array elements) which represent the
 * starting values for the system properties as provided
 * by the virtual machine.
 *
 * @return		the default values for the system properties.
 */
private static native String [] getPropertyList();
/*[ENDIF] JAVA_SPEC_VERSION < 17 */

/**
 * Before propertiesInitialized is set to true,
 * this returns the requested system property according to sysPropID:
 *   0 - os.version
 *   1 - platform encoding
 *   2 - file.encoding
 *   3 - os.encoding
 *   Reserved for future
 */
private static native String getSysPropBeforePropertiesInitialized(int sysPropID);

/**
 * Answers the active security manager.
 *
 * @return		the system security manager object.
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@Deprecated(since="17", forRemoval=true)
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public static SecurityManager getSecurityManager() {
	/*[IF JAVA_SPEC_VERSION >= 24]*/
	return null;
	/*[ELSE] JAVA_SPEC_VERSION >= 24 */
	return security;
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
}

/**
 * Answers an integer hash code for the parameter.
 * The hash code returned is the same one that would
 * be returned by java.lang.Object.hashCode(), whether
 * or not the object's class has overridden hashCode().
 * The hash code for null is 0.
 *
 * @param		anObject	the object
 * @return		the hash code for the object
 *
 * @see			java.lang.Object#hashCode
 */
public static int identityHashCode(Object anObject) {
	if (anObject == null) {
		return 0;
	}
/*[IF INLINE-TYPES]*/
	if (anObject.getClass().isValue()) {
		return J9VMInternals.valueHashCode(anObject);
	}
/*[ENDIF] INLINE-TYPES */
	return J9VMInternals.fastIdentityHashCode(anObject);
}

/**
 * Loads the specified file as a dynamic library.
 *
 * @param pathName the path of the file to be loaded
 *
 * @throws UnsatisfiedLinkError if the library could not be loaded
 * @throws NullPointerException if pathName is null
/*[IF JAVA_SPEC_VERSION >= 24]
 * @throws IllegalCallerException if the caller belongs to a module where native access is not enabled
/*[ELSE] JAVA_SPEC_VERSION >= 24
 * @throws SecurityException if the library was not allowed to be loaded
/*[ENDIF] JAVA_SPEC_VERSION >= 24
 */
@CallerSensitive
/*[IF JAVA_SPEC_VERSION >= 24]*/
@Restricted
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
public static void load(String pathName) {
	/*[IF JAVA_SPEC_VERSION >= 24]*/
	Class<?> caller = Reflection.getCallerClass();
	Reflection.ensureNativeAccess(caller, System.class, "load", false);
	/*[ELSE] JAVA_SPEC_VERSION >= 24 */
	@SuppressWarnings("removal")
	SecurityManager smngr = System.getSecurityManager();
	if (smngr != null) {
		smngr.checkLink(pathName);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
	/*[IF JAVA_SPEC_VERSION >= 15]*/
	/*[IF PLATFORM-mz31 | PLATFORM-mz64]*/
	ClassLoader.loadZOSLibrary(getCallerClass(), pathName);
	/*[ELSE] PLATFORM-mz31 | PLATFORM-mz64 */
	File fileName = new File(pathName);
	if (!fileName.isAbsolute()) {
		/*[MSG "K0648", "Not an absolute path: {0}"]*/
		throw new UnsatisfiedLinkError(Msg.getString("K0648", pathName));//$NON-NLS-1$
	}
	ClassLoader.loadLibrary(getCallerClass(), fileName);
	/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 */
	/*[ELSE] JAVA_SPEC_VERSION >= 15 */
	ClassLoader.loadLibraryWithPath(pathName, ClassLoader.callerClassLoader(), null);
	/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
}

/**
 * Loads and links the library specified by the argument.
 *
 * @param libName the name of the library to load
 *
 * @throws UnsatisfiedLinkError if the library could not be loaded
 * @throws NullPointerException if libName is null
/*[IF JAVA_SPEC_VERSION >= 24]
 * @throws IllegalCallerException if the caller belongs to a module where native access is not enabled
/*[ELSE] JAVA_SPEC_VERSION >= 24
 * @throws SecurityException if the library was not allowed to be loaded
/*[ENDIF] JAVA_SPEC_VERSION >= 24
 */
@CallerSensitive
/*[IF JAVA_SPEC_VERSION >= 24]*/
@Restricted
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
public static void loadLibrary(String libName) {
	/*[IF JAVA_SPEC_VERSION >= 24]*/
	Class<?> caller = Reflection.getCallerClass();
	Reflection.ensureNativeAccess(caller, System.class, "loadLibrary", false);
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

	if (libName.indexOf(File.pathSeparator) >= 0) {
		/*[MSG "K0B01", "Library name must not contain a file path: {0}"]*/
		throw new UnsatisfiedLinkError(Msg.getString("K0B01", libName)); //$NON-NLS-1$
	}
	if (internalGetProperties().getProperty("os.name").startsWith("Windows")) { //$NON-NLS-1$ //$NON-NLS-2$
		if ((libName.indexOf('/') >= 0) || ((libName.length() > 1) && (libName.charAt(1) == ':'))) {
			/*[MSG "K0B01", "Library name must not contain a file path: {0}"]*/
			throw new UnsatisfiedLinkError(Msg.getString("K0B01", libName)); //$NON-NLS-1$
		}
	}
/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager smngr = System.getSecurityManager();
	if (smngr != null) {
		smngr.checkLink(libName);
	}
/*[ENDIF] JAVA_SPEC_VERSION < 24 */
/*[IF JAVA_SPEC_VERSION >= 15]*/
	Class<?> callerClass = getCallerClass();
/*[ELSE]*/
	ClassLoader callerClassLoader = ClassLoader.callerClassLoader();
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
	try {
/*[IF JAVA_SPEC_VERSION >= 15]*/
		ClassLoader.loadLibrary(callerClass, libName);
/*[ELSE]*/
		ClassLoader.loadLibraryWithClassLoader(libName, callerClassLoader);
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
	} catch (UnsatisfiedLinkError ule) {
		String errorMessage = ule.getMessage();
		if ((errorMessage != null) && errorMessage.contains("already loaded in another classloader")) { //$NON-NLS-1$
			// attempt to unload the classloader, and retry
			gc();
/*[IF JAVA_SPEC_VERSION >= 15]*/
			ClassLoader.loadLibrary(callerClass, libName);
/*[ELSE]*/
			ClassLoader.loadLibraryWithClassLoader(libName, callerClassLoader);
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
		} else {
			throw ule;
		}
	}
}

/**
 * Provides a hint to the virtual machine that it would
 * be useful to attempt to perform any outstanding
 * object finalizations.
 */
/*[IF JAVA_SPEC_VERSION >= 18]*/
@Deprecated(forRemoval=true, since="18")
/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
public static void runFinalization() {
	RUNTIME.runFinalization();
}

/*[IF JAVA_SPEC_VERSION < 11]*/
/**
 * Throws {@code UnsupportedOperationException}.
 *
 * @deprecated 	This method is unsafe.
 */
@Deprecated
public static void runFinalizersOnExit(boolean flag) {
	Runtime.runFinalizersOnExit(flag);
}
/*[ENDIF] JAVA_SPEC_VERSION < 11 */

/**
 * Sets the system properties. Note that the object which is passed in
 * is not copied, so that subsequent changes made to it will be reflected
 * in calls to {@code getProperty()} and {@code getProperties()}.
/*[IF JAVA_SPEC_VERSION < 24]
 * <p>
 * Security managers should restrict access to this
 * API if possible.
/*[ENDIF] JAVA_SPEC_VERSION < 24
 *
 * @param		p			the properties to set
 */
public static void setProperties(Properties p) {
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPropertiesAccess();
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	if (p == null) {
		ensureProperties(false);
	} else {
		systemProperties = p;
	}
}

static void checkTmpDir() {
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	if (SystemProps.isBadIoTmpdir()) {
		System.err.println("WARNING: java.io.tmpdir directory does not exist"); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
}

/*[IF JAVA_SPEC_VERSION >= 9]*/
/**
 * Initialize the security manager according
 * to the java.security.manager system property.
 * @param applicationClassLoader
 * @throws Error
/*[IF JAVA_SPEC_VERSION >= 24]
 *  if the user attempts to enable the security manager
/*[ELSE] JAVA_SPEC_VERSION >= 24
 *  if the security manager could not be initialized
/*[ENDIF] JAVA_SPEC_VERSION >= 24
 */
/*[IF JAVA_SPEC_VERSION < 24]*/
@SuppressWarnings("removal")
/*[ENDIF] JAVA_SPEC_VERSION < 24 */
static void initSecurityManager(ClassLoader applicationClassLoader) {
	/*[IF JAVA_SPEC_VERSION >= 24]*/
	boolean throwErrorOnInit = false;
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
	String javaSecurityManager = internalGetProperties().getProperty("java.security.manager"); //$NON-NLS-1$
	if (null == javaSecurityManager) {
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		throwUOEFromSetSM = true;
		/*[ELSE] JAVA_SPEC_VERSION >= 18 */
		/* Do nothing. */
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
	} else if ("allow".equals(javaSecurityManager)) { //$NON-NLS-1$
		/*[IF JAVA_SPEC_VERSION >= 24]*/
		throwErrorOnInit = true;
		/*[ELSE] JAVA_SPEC_VERSION >= 24 */
		/* Do nothing. */
		/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
	} else if ("disallow".equals(javaSecurityManager)) { //$NON-NLS-1$
		/*[IF JAVA_SPEC_VERSION > 11]*/
		throwUOEFromSetSM = true;
		/*[ELSE] JAVA_SPEC_VERSION > 11 */
		/* Do nothing. */
		/*[ENDIF] JAVA_SPEC_VERSION > 11 */
	} else {
		/*[IF JAVA_SPEC_VERSION >= 24]*/
		throwErrorOnInit = true;
		/*[ELSE] JAVA_SPEC_VERSION >= 24 */
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		initialErr.println("WARNING: A command line option has enabled the Security Manager"); //$NON-NLS-1$
		initialErr.println("WARNING: The Security Manager is deprecated and will be removed in a future release"); //$NON-NLS-1$
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		if (javaSecurityManager.isEmpty() || "default".equals(javaSecurityManager)) { //$NON-NLS-1$
			setSecurityManager(new SecurityManager());
		} else {
			try {
				Constructor<?> constructor = Class.forName(javaSecurityManager, true, applicationClassLoader).getConstructor();
				constructor.setAccessible(true);
				setSecurityManager((SecurityManager)constructor.newInstance());
			} catch (Throwable e) {
				/*[MSG "K0631", "JVM can't set custom SecurityManager due to {0}"]*/
				throw new Error(Msg.getString("K0631", e.toString()), e); //$NON-NLS-1$
			}
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
	}
	/*[IF JAVA_SPEC_VERSION >= 24]*/
	if (throwErrorOnInit) {
		/*[MSG "K0B04", "A command line option has attempted to allow or enable the Security Manager. Enabling a Security Manager is not supported."]*/
		throw new Error(Msg.getString("K0B04")); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
}
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

/*[IF (21 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24)]*/
static boolean allowSecurityManager() {
	return !throwUOEFromSetSM;
}
/*[ENDIF] (21 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24) */

/**
 * Sets the active security manager. Note that once
 * the security manager has been set, it can not be
 * changed. Attempts to do so will cause a security
 * exception.
 *
 * @param		s			the new security manager
 *
/*[IF JAVA_SPEC_VERSION >= 24]
 * @throws		UnsupportedOperationException	always
/*[ELSE] JAVA_SPEC_VERSION >= 24
 * @throws		SecurityException 	if the security manager has already been set and its checkPermission method doesn't allow it to be replaced.
/*[IF JAVA_SPEC_VERSION > 11]
 * @throws		UnsupportedOperationException 	if s is non-null and a special token "disallow" has been set for system property "java.security.manager"
 * 												which indicates that a security manager is not allowed to be set dynamically.
/*[ENDIF] JAVA_SPEC_VERSION > 11
/*[ENDIF] JAVA_SPEC_VERSION >= 24
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@Deprecated(since="17", forRemoval=true)
@CallerSensitive
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public static void setSecurityManager(final SecurityManager s) {
/*[IF JAVA_SPEC_VERSION >= 24]*/
	/*[MSG "K0B03", "Setting a Security Manager is not supported"]*/
	throw new UnsupportedOperationException(Msg.getString("K0B03")); //$NON-NLS-1$
/*[ELSE] JAVA_SPEC_VERSION >= 24 */
/*[IF CRIU_SUPPORT]*/
	if (openj9.internal.criu.InternalCRIUSupport.isCRIUSupportEnabled()) {
		/*[MSG "K0B02", "Enabling a SecurityManager currently unsupported when -XX:+EnableCRIUSupport is specified"]*/
		throw new UnsupportedOperationException(Msg.getString("K0B02")); //$NON-NLS-1$
	}
/*[ENDIF] CRIU_SUPPORT */
	/*[PR 113606] security field could be modified by another Thread */
	@SuppressWarnings("removal")
	final SecurityManager currentSecurity = security;

	/*[IF JAVA_SPEC_VERSION > 11]*/
	if (throwUOEFromSetSM) {
		/* The security manager is not allowed to be set dynamically. Return if the
		 * argument is null. UnsupportedOperationException should only be thrown for
		 * a non-null argument.
		 */
		if (s == null) {
			return;
		}
		/*[MSG "K0B00", "The Security Manager is deprecated and will be removed in a future release"]*/
		throw new UnsupportedOperationException(Msg.getString("K0B00")); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION > 11 */

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	Class<?> callerClz = getCallerClass();
	if (setSMCallers.putIfAbsent(callerClz, Boolean.TRUE) == null) {
		String callerName = callerClz.getName();
		CodeSource cs = callerClz.getProtectionDomainInternal().getCodeSource();
		String codeSource = (cs == null) ? callerName : callerName + " (" + cs.getLocation() + ")"; //$NON-NLS-1$ //$NON-NLS-2$
		initialErr.println("WARNING: A terminally deprecated method in java.lang.System has been called"); //$NON-NLS-1$
		initialErr.printf("WARNING: System::setSecurityManager has been called by %s" + lineSeparator(), codeSource); //$NON-NLS-1$
		initialErr.printf("WARNING: Please consider reporting this to the maintainers of %s" + lineSeparator(), callerName); //$NON-NLS-1$
		initialErr.println("WARNING: System::setSecurityManager will be removed in a future release"); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	if (s != null) {
		if (currentSecurity == null) {
			// only preload classes when current security manager is null
			// not adding an extra static field to preload only once
			try {
				/*[PR 95057] preload classes required for checkPackageAccess() */
				// Preload classes used for checkPackageAccess(),
				// otherwise we could go recursive
				s.checkPackageAccess("java.lang"); //$NON-NLS-1$
			} catch (Exception e) {
				// ignore any potential exceptions
			}
		}

		try {
			/*[PR 97686] Preload the policy permission */
			AccessController.doPrivileged(new PrivilegedAction<Void>() {
				@Override
				public Void run() {
					if (currentSecurity == null) {
						// initialize external messages and
						// also load security sensitive classes
						Msg.getString("K002c"); //$NON-NLS-1$
					}
					ProtectionDomain pd = s.getClass().getPDImpl();
					if (pd != null) {
						// initialize the protection domain, which may include preloading the
						// dynamic permissions from the policy before installing
						pd.implies(sun.security.util.SecurityConstants.ALL_PERMISSION);
					}
					return null;
				}});
		} catch (Exception e) {
			// ignore any potential exceptions
		}
	}
	if (currentSecurity != null) {
		currentSecurity.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetSecurityManager);
	}
	security = s;
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
}

/**
 * Answers the platform-specific filename for the shared
 * library named by the argument.
 *
 * @param		userLibName 	the name of the library to look up.
 * @return		the platform-specific filename for the library
 */
public static native String mapLibraryName(String userLibName);

/**
 * Sets the value of the named static field in the receiver
 * to the passed in argument.
 *
 * @param		fieldName 	the name of the field to set, one of in, out, or err
 * @param		stream 		the new value of the field
 */
private static native void setFieldImpl(String fieldName, Object stream);

@CallerSensitive
static Class getCallerClass() {
	return Class.getStackClass(2);
}

/**
 * Return the channel inherited by the invocation of the running vm. The channel is
 * obtained by calling SelectorProvider.inheritedChannel().
 *
 * @return the inherited channel or null
 *
 * @throws IOException if an IO error occurs getting the inherited channel
 */
public static java.nio.channels.Channel inheritedChannel() throws IOException {
	return java.nio.channels.spi.SelectorProvider.provider().inheritedChannel();
}

/**
 * Returns the current tick count in nanoseconds. The tick count
 * only reflects elapsed time.
 *
 * @return		the current nanosecond tick count, which may be negative
 */
public static native long nanoTime();

/**
 * Removes the property.
 *
 * @param 		prop		the name of the property to remove
 * @return		the value of the property removed
 */
public static String clearProperty(String prop) {
	if (!propertiesInitialized) throw new Error("bootstrap error, system property access before init: " + prop); //$NON-NLS-1$

	if (prop.length() == 0) throw new IllegalArgumentException();
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(new PropertyPermission(prop, "write")); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	return (String)systemProperties.remove(prop);
}

/**
 * Returns all of the system environment variables in an unmodifiable Map.
 *
 * @return	an unmodifiable Map containing all of the system environment variables.
 */
public static Map<String, String> getenv() {
	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(new RuntimePermission("getenv.*")); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	return ProcessEnvironment.getenv();
}

/**
 * Return the Console or null.
 *
 * @return the Console or null
 */
public static Console console() {
	/*[PR CMVC 114090] System.console does not return null in subprocess */
	/*[PR CMVC 114119] Console exists when stdin, stdout are redirected */
	if (consoleInitialized) return console;
	synchronized(System.class) {
		if (consoleInitialized) return console;
		console = SharedSecrets.getJavaIOAccess().console();
		consoleInitialized = true;
	}
	return console;
}

/**
 * JIT Helper method
 */
private static void longMultiLeafArrayCopy(Object src, int srcPos,
		Object dest, int destPos, int length, int elementSize, int leafSize) {

	// Check if this is a forward or backwards arraycopy
	boolean isFwd;
	if (src != dest || srcPos > destPos || srcPos+length <= destPos)
		isFwd = true;
	else
		isFwd = false;

	int newSrcPos;
	int newDestPos;
	int numOfElemsPerLeaf = leafSize / elementSize;
	int srcLeafPos;
	int destLeafPos;
	int L1, L2, L3;
	int offset;

	if (isFwd)
	{
		newSrcPos = srcPos;
		newDestPos = destPos;
	}
	else
	{
		newSrcPos = srcPos + length - 1;
		newDestPos = destPos + length - 1;
	}

	srcLeafPos = newSrcPos % numOfElemsPerLeaf;
	destLeafPos = newDestPos % numOfElemsPerLeaf;
	if (srcLeafPos < destLeafPos)
	{
		if (isFwd)
		{
			// align dst to leaf boundary first
			L1 = numOfElemsPerLeaf - destLeafPos;
			L2 = numOfElemsPerLeaf - (srcLeafPos + L1);
		}
		else
		{
			L1 = srcLeafPos + 1;
			L2 = destLeafPos - srcLeafPos;
		}
	}
	else
	{
		if (isFwd)
		{
			// align src to leaf boundary first
			L1 = numOfElemsPerLeaf - srcLeafPos;
			L2 = numOfElemsPerLeaf - (destLeafPos + L1);
		}
		else
		{
			L1 = destLeafPos + 1;
			L2 = srcLeafPos - destLeafPos;
		}
	}

	L3 = numOfElemsPerLeaf - L2;

	// begin copying
	offset = isFwd ? 0 : L1 - 1;
	System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, L1);
	newSrcPos = isFwd ? newSrcPos + L1 : newSrcPos - L1;
	newDestPos = isFwd ? newDestPos + L1 : newDestPos - L1;
	length -= L1;

	// now one of newSrcPos and newDestPos is aligned on an arraylet
	// leaf boundary, so copies of L2 and L3 can be repeated until there
	// is less than a full leaf left to copy
	while (length >= numOfElemsPerLeaf) {
		offset = isFwd ? 0 : L2 - 1;
		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, L2);
		newSrcPos = isFwd ? newSrcPos + L2 : newSrcPos - L2;
		newDestPos = isFwd ? newDestPos + L2 : newDestPos - L2;

		offset = isFwd ? 0 : L3 - 1;
		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, L3);
		newSrcPos = isFwd ? newSrcPos + L3 : newSrcPos - L3;
		newDestPos = isFwd ? newDestPos + L3 : newDestPos - L3;

		length -= numOfElemsPerLeaf;
	}

	// next L2 elements of each array must be on same leaf
	if (length > L2) {
		offset = isFwd ? 0 : L2 - 1;
		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, L2);
		newSrcPos = isFwd ? newSrcPos + L2 : newSrcPos - L2;
		newDestPos = isFwd ? newDestPos + L2 : newDestPos - L2;
		length -= L2;
	}

	// whatever is left of each array must be on the same leaf
	if (length > 0) {
		offset = isFwd ? 0 : length - 1;
		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, length);
	}
}

/**
 * JIT Helper method
 */
private static void simpleMultiLeafArrayCopy(Object src, int srcPos,
		Object dest, int destPos, int length, int elementSize, int leafSize)
{
	int count = 0;

	// Check if this is a forward or backwards arraycopy
	boolean isFwd;
	if (src != dest || srcPos > destPos || srcPos+length <= destPos)
		isFwd = true;
	else
		isFwd = false;

	int newSrcPos;
	int newDestPos;
	int numOfElemsPerLeaf = leafSize / elementSize;
	int srcLeafPos;
	int destLeafPos;
	int iterLength;

	if (isFwd)
	{
		 newSrcPos = srcPos;
		 newDestPos = destPos;
	}
	else
	{
		 newSrcPos = srcPos + length - 1;
		 newDestPos = destPos + length - 1;
	}

	for (count = 0; count < length; count += iterLength)
	{
		srcLeafPos = (newSrcPos % numOfElemsPerLeaf);
		 destLeafPos = newDestPos % numOfElemsPerLeaf;
		 int offset;
		if ((isFwd && (srcLeafPos >= destLeafPos)) ||
		(!isFwd && (srcLeafPos < destLeafPos)))
		{
			if (isFwd)
				iterLength = numOfElemsPerLeaf - srcLeafPos;
			 else
				iterLength = srcLeafPos + 1;
		 }
		 else
		 {
			 if (isFwd)
				iterLength = numOfElemsPerLeaf - destLeafPos;
			 else
				iterLength = destLeafPos + 1;
		 }

		 if (length - count < iterLength)
			 iterLength = length - count;

		 if (isFwd)
			offset = 0;
		 else
			offset = iterLength - 1;

		 System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, iterLength);

		 if (isFwd)
		 {
			 newSrcPos += iterLength;
			 newDestPos += iterLength;
		 }
		 else
		 {
			 newSrcPos -= iterLength;
			 newDestPos -= iterLength;
		 }
	}
}

/**
 * JIT Helper method
 */
private static void multiLeafArrayCopy(Object src, int srcPos, Object dest,
		int destPos, int length, int elementSize, int leafSize) {
	int count = 0;

	// Check if this is a forward or backwards arraycopy
	boolean isFwd;
	if (src != dest || srcPos > destPos || srcPos+length <= destPos)
		isFwd = true;
	else
		isFwd = false;

	int newSrcPos;
	int newDestPos;
	int numOfElemsPerLeaf = leafSize / elementSize;
	int srcLeafPos;
	int destLeafPos;

	if (isFwd)
	{
		newSrcPos = srcPos;
		newDestPos = destPos;
	}
	else
	{
		 newSrcPos = srcPos + length - 1;
		 newDestPos = destPos + length - 1;
	}

	int iterLength1 = 0;
	int iterLength2 = 0;
	while (count < length)
	{
		srcLeafPos = (newSrcPos % numOfElemsPerLeaf);
		destLeafPos = (newDestPos % numOfElemsPerLeaf);

		int firstPos, secondPos;

		if ((isFwd && (srcLeafPos >= destLeafPos)) ||
				(!isFwd && (srcLeafPos < destLeafPos)))
		{
			firstPos = srcLeafPos;
			secondPos = newDestPos;
		}
		else
		{
			firstPos = destLeafPos;
			secondPos = newSrcPos;
		}

		int offset = 0;
		if (isFwd)
			iterLength1 = numOfElemsPerLeaf - firstPos;
		else
			iterLength1 = firstPos + 1;

		if (length - count < iterLength1)
			iterLength1 = length - count;

		if (isFwd)
			iterLength2 = numOfElemsPerLeaf - ((secondPos + iterLength1) % numOfElemsPerLeaf);
		else
		{
			iterLength2 = ((secondPos - iterLength1) % numOfElemsPerLeaf) + 1;
			offset = iterLength1 - 1;
		}

		if (length - count - iterLength1 < iterLength2)
			iterLength2 = length - count - iterLength1;

		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, iterLength1);

		offset = 0;
		if (isFwd)
		{
			newSrcPos += iterLength1;
			newDestPos += iterLength1;
		}
		else
		{
			newSrcPos -= iterLength1;
			newDestPos -= iterLength1;
			offset = iterLength2 - 1;
		}

		if (iterLength2 <= 0) break;

		System.arraycopy(src, newSrcPos - offset, dest, newDestPos - offset, iterLength2);

		if (isFwd)
		{
			newSrcPos += iterLength2;
			newDestPos += iterLength2;
		}
		else
		{
			newSrcPos -= iterLength2;
			newDestPos -= iterLength2;
		}

		count += iterLength1 + iterLength2;
	}
}

/**
 * Return platform specific line separator character(s).
 * Unix is \n while Windows is \r\n as per the prop set by the VM.
 *
 * @return platform specific line separator string
 */
//  Refer to Jazz 30875
public static String lineSeparator() {
	 return lineSeparator;
}

/*[IF JAVA_SPEC_VERSION >= 9]*/
/**
 * Return an instance of Logger.
 *
 * @param loggerName The name of the logger to return
 * @return An instance of the logger.
 * @throws NullPointerException if the loggerName is null
 */
@CallerSensitive
public static Logger getLogger(String loggerName) {
	Objects.requireNonNull(loggerName);
	Class<?> caller = Reflection.getCallerClass();
	return jdk.internal.logger.LazyLoggers.getLogger(loggerName, caller.getModule());
}

/**
 * Return an instance of Logger that is localized based on the ResourceBundle.
 *
 * @param loggerName The name of the logger to return
 * @param bundle The ResourceBundle to use for localization
 * @return An instance of the logger localized to 'bundle'
 * @throws NullPointerException if the loggerName or bundle is null
 */
@CallerSensitive
public static Logger getLogger(String loggerName, ResourceBundle bundle) {
	Objects.requireNonNull(loggerName);
	Objects.requireNonNull(bundle);
	Class<?> caller = Reflection.getCallerClass();
	return LoggerFinder.getLoggerFinder().getLocalizedLogger(loggerName, bundle, caller.getModule());
}

/**
 * The LoggerFinder service creates, manages and configures loggers
 * to the underlying framework it uses.
 */
public abstract static class LoggerFinder {
	private static volatile LoggerFinder loggerFinder;

	/**
	 * Checks needed runtime permissions
	 *
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	protected LoggerFinder() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		verifyPermissions();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/**
	 * Returns a localizable instance of Logger for the given module
	 *
	 * @param loggerName The name of the logger
	 * @param bundle A resource bundle; can be null
	 * @param callerModule The module for which the logger is being requested
	 * @return an instance of Logger
	 * @throws NullPointerException if loggerName or callerModule is null
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public Logger getLocalizedLogger(String loggerName, ResourceBundle bundle, Module callerModule) {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		verifyPermissions();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		Objects.requireNonNull(loggerName);
		Objects.requireNonNull(callerModule);
		Logger logger = this.getLogger(loggerName, callerModule);
		Logger localizedLogger = new jdk.internal.logger.LocalizedLoggerWrapper(logger, bundle);
		return localizedLogger;
	}

	/**
	 * Returns an instance of Logger for the given module
	 *
	 * @param loggerName The name of the logger
	 * @param callerModule The module for which the logger is being requested
	 * @return a Logger suitable for use within the given module
	 * @throws NullPointerException if loggerName or callerModule is null
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public abstract Logger getLogger(String loggerName, Module callerModule);

	/**
	 * Returns the LoggerFinder instance
	 *
	 * @return the LoggerFinder instance.
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static LoggerFinder getLoggerFinder() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		verifyPermissions();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		LoggerFinder localFinder = loggerFinder;
		if (localFinder == null) {
			/*[IF JAVA_SPEC_VERSION >= 24]*/
			localFinder = jdk.internal.logger.LoggerFinderLoader.getLoggerFinder();
			/*[ELSE] JAVA_SPEC_VERSION >= 24 */
			localFinder = AccessController.doPrivileged(
								(PrivilegedAction<LoggerFinder>) () -> jdk.internal.logger.LoggerFinderLoader.getLoggerFinder(),
								AccessController.getContext(),
								com.ibm.oti.util.RuntimePermissions.permissionLoggerFinder);
			/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
			/*[IF JAVA_SPEC_VERSION >= 11]*/
			if (localFinder instanceof jdk.internal.logger.LoggerFinderLoader.TemporaryLoggerFinder) {
				return localFinder;
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
			loggerFinder = localFinder;
		}
		return localFinder;
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private static void verifyPermissions() {
		@SuppressWarnings("removal")
		SecurityManager securityManager = System.getSecurityManager();
		if (securityManager != null) {
			securityManager.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionLoggerFinder);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
}

/**
 * Logger logs messages that will be routed to the underlying logging framework
 * that LoggerFinder uses.
 */
public interface Logger {
	/**
	 * System loggers levels
	 */
	public enum Level {
		/**
		 * Enable all logging level messages
		 */
		ALL(Integer.MIN_VALUE),
		/**
		 * Enable TRACE level messages
		 */
		TRACE(400),
		/**
		 * Enable DEBUG level messages
		 */
		DEBUG(500),
		/**
		 * Enable INFO level messages
		 */
		INFO(800),
		/**
		 * Enable WARNING level messages
		 */
		WARNING(900),
		/**
		 * Enable ERROR level messages
		 */
		ERROR(1000),
		/**
		 *
		 * Disable logging
		 */
		OFF(Integer.MAX_VALUE);

		final int severity;
		Level(int value) {
			severity = value;
		}

		/**
		 * Returns the name of this level
		 *
		 * @return name of this level
		 */
		public final java.lang.String getName() {
			return name();
		}

		/**
		 * Returns severity of this level
		 *
		 * @return severity of this level
		 */
		public final int getSeverity() {
			return severity;
		}
	}

	/**
	 * Returns the name of this logger
	 *
	 * @return the logger name
	 */
	public String getName();

	/**
	 * Checks if a message of the given level will be logged
	 *
	 * @param level The log message level
	 * @return true if the given log message level is currently being logged
	 * @throws NullPointerException if level is null
	 */
	public boolean isLoggable(Level level);

	/**
	 * Logs a message
	 *
	 * @param level The log message level
	 * @param msg The log message
	 * @throws NullPointerException if level is null
	 */
	public default void log(Level level, String msg) {
		Objects.requireNonNull(level);
		log(level, (ResourceBundle)null, msg, (Object[])null);
	}

	/**
	 * Logs a lazily supplied message
	 *
	 * @param level The log message level
	 * @param supplier Supplier function that produces a message
	 * @throws NullPointerException if level or supplier is null
	 */
	public default void log(Level level, Supplier<String> supplier) {
		Objects.requireNonNull(level);
		Objects.requireNonNull(supplier);
		if (isLoggable(level)) {
			log(level, (ResourceBundle)null, supplier.get(), (Object[])null);
		}
	}

	/**
	 * Logs a message produced from the give object
	 *
	 * @param level The log message level
	 * @param value The object to log
	 * @throws NullPointerException if level or value is null
	 */
	public default void log(Level level, Object value) {
		Objects.requireNonNull(level);
		Objects.requireNonNull(value);
		if (isLoggable(level)) {
			log(level, (ResourceBundle)null, value.toString(), (Object[])null);
		}
	}

	/**
	 * Log a message associated with a given throwable
	 *
	 * @param level The log message level
	 * @param msg The log message
	 * @param throwable Throwable associated with the log message
	 * @throws NullPointerException if level is null
	 */
	public default void log(Level level, String msg, Throwable throwable) {
		Objects.requireNonNull(level);
		log(level, (ResourceBundle)null, msg, throwable);
	}

	/**
	 * Logs a lazily supplied message associated with a given throwable
	 *
	 * @param level The log message level
	 * @param supplier Supplier function that produces a message
	 * @param throwable Throwable associated with the log message
	 * @throws NullPointerException if level or supplier is null
	 */
	public default void log(Level level, Supplier<String> supplier, Throwable throwable) {
		Objects.requireNonNull(level);
		Objects.requireNonNull(supplier);
		if (isLoggable(level)) {
			log(level, (ResourceBundle)null, supplier.get(), throwable);
		}
	}

	/**
	 * Logs a message with an optional list of parameters
	 *
	 * @param level The log message level
	 * @param msg The log message
	 * @param values Optional list of parameters
	 * @throws NullPointerException if level is null
	 */
	public default void log(Level level, String msg, Object... values) {
		Objects.requireNonNull(level);
		log(level, (ResourceBundle)null, msg, values);
	}

	/**
	 * Logs a localized message associated with a given throwable
	 *
	 * @param level The log message level
	 * @param bundle A resource bundle to localize msg
	 * @param msg The log message
	 * @param throwable Throwable associated with the log message
	 * @throws NullPointerException if level is null
	 */
	public void log(Level level, ResourceBundle bundle, String msg, Throwable throwable);

	/**
	 * Logs a message with resource bundle and an optional list of parameters
	 *
	 * @param level The log message level
	 * @param bundle A resource bundle to localize msg
	 * @param msg The log message
	 * @param values Optional list of parameters
	 * @throws NullPointerException if level is null
	 */
	public void log(Level level, ResourceBundle bundle, String msg, Object... values);
}
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
}
