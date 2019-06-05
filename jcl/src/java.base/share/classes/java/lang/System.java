/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
package java.lang;

import java.io.*;
import java.util.Map;
import java.util.Properties;
import java.util.PropertyPermission;
import java.security.*;
import java.lang.reflect.Method;

/*[IF Sidecar19-SE]*/
import jdk.internal.misc.Unsafe;
/*[IF Java12]*/
import jdk.internal.access.SharedSecrets;
/*[ELSE]
import jdk.internal.misc.SharedSecrets;
/*[ENDIF] Java12 */
import jdk.internal.misc.VM;
import java.lang.StackWalker.Option;
import java.lang.Module;
import jdk.internal.reflect.Reflection;
import jdk.internal.reflect.CallerSensitive;
import java.util.*;
import java.util.function.*;
/*[ELSE]
import sun.misc.Unsafe;
import sun.misc.SharedSecrets;
import sun.misc.VM;
import sun.reflect.CallerSensitive;
/*[ENDIF]*/
/*[IF Sidecar19-SE]*/
import com.ibm.gpu.spi.GPUAssist;
import com.ibm.gpu.spi.GPUAssistHolder;
/*[ENDIF] Sidecar19-SE */

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
	/**
	 * Default output stream
	 */
	public static final PrintStream out = null;
	/**
	 * Default error output stream
	 */
	public static final PrintStream err = null;

	//Get a ref to the Runtime instance for faster lookup
	private static final Runtime RUNTIME = Runtime.getRuntime();
	/**
	 * The System Properties table.
	 */
	private static Properties systemProperties;

	/**
	 * The System default SecurityManager.
	 */
	private static SecurityManager security;

	private static volatile Console console;
	private static volatile boolean consoleInitialized;

	private static String lineSeparator;
		
	private static boolean propertiesInitialized = false;
	private static boolean useLegacyVerPresents = false; 

	private static String platformEncoding;
	private static String fileEncoding;
	private static String osEncoding;

/*[IF Sidecar19-SE]*/
	static java.lang.ModuleLayer	bootLayer;
/*[ENDIF]*/
	
	// Initialize all the slots in System on first use.
	static {
		initEncodings();		
	}
	
	//	get following system properties in clinit and make it available via static variables 
	//	at early boot stage in which System is not fully initialized
	//	os.encoding, ibm.system.encoding/sun.jnu.encoding, file.encoding
	private static void initEncodings() {
		platformEncoding = getEncoding(PlatformEncoding);
		String definedFileEncoding = getEncoding(FileEncoding);
		String definedOSEncoding = getEncoding(OSEncoding);
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
		if (osEncoding == null)
			osEncoding = definedOSEncoding;
	}	
		
	static void afterClinitInitialization() {
		/*[PR CMVC 189091] Perf: EnumSet.allOf() is slow */
		/*[PR CMVC 191554] Provide access to ClassLoader methods to improve performance */
		/* Multitenancy change: only initialize VMLangAccess once as it's non-isolated */ 
		if (null == com.ibm.oti.vm.VM.getVMLangAccess()) {
			com.ibm.oti.vm.VM.setVMLangAccess(new VMAccess());
		}

		// Fill in the properties from the VM information.
		ensureProperties(true);

		/*[PR CMVC 150472] sun.misc.SharedSecrets needs access to java.lang. */
		SharedSecrets.setJavaLangAccess(new Access());
		
		/*[REM] Initialize the JITHelpers needed in J9VMInternals since the class can't do it itself */
		try {
			java.lang.reflect.Field f1 = J9VMInternals.class.getDeclaredField("jitHelpers"); //$NON-NLS-1$
			java.lang.reflect.Field f2 = String.class.getDeclaredField("helpers"); //$NON-NLS-1$
			
			Unsafe unsafe = Unsafe.getUnsafe();
			
			unsafe.putObject(unsafe.staticFieldBase(f1), unsafe.staticFieldOffset(f1), com.ibm.jit.JITHelpers.getHelpers());
			unsafe.putObject(unsafe.staticFieldBase(f2), unsafe.staticFieldOffset(f2), com.ibm.jit.JITHelpers.getHelpers());
		} catch (NoSuchFieldException e) { }
		
		/**
		 * When the System Property == true, then disable sharing (i.e. arraycopy the underlying value array) in 
		 * String.substring(int) and String.substring(int, int) methods whenever offset is zero. Otherwise, enable 
		 * sharing of the underlying value array.
		 */
		String enableSharingInSubstringWhenOffsetIsZeroProperty = internalGetProperties().getProperty("java.lang.string.substring.nocopy"); //$NON-NLS-1$
		String.enableSharingInSubstringWhenOffsetIsZero = enableSharingInSubstringWhenOffsetIsZeroProperty == null || enableSharingInSubstringWhenOffsetIsZeroProperty.equalsIgnoreCase("false"); //$NON-NLS-1$
		
		// Set up standard in, out, and err.
		/*[PR CMVC 193070] - OTT:Java 8 Test_JITHelpers test_getSuperclass NoSuchMet*/
		/*[PR JAZZ 58297] - continue with the rules defined by JAZZ 57070 - Build a Java 8 J9 JCL using the SIDECAR18 preprocessor configuration */
		// Check the default encoding
		/*[Bug 102075] J2SE Setting -Dfile.encoding=junk fails to run*/
		/*[IF Sidecar19-SE]*/
		StringCoding.encode(String.LATIN1, new byte[1]);
		/*[ELSE]*/
		StringCoding.encode(new char[1], 0, 1);
		/*[ENDIF]*/
		/*[IF Sidecar18-SE-OpenJ9|Sidecar19-SE]*/
		setErr(new PrintStream(new BufferedOutputStream(new FileOutputStream(FileDescriptor.err)), true));
		setOut(new PrintStream(new BufferedOutputStream(new FileOutputStream(FileDescriptor.out)), true));
		/*[IF Sidecar19-SE_RAWPLUSJ9]*/
		setIn(new BufferedInputStream(new FileInputStream(FileDescriptor.in)));
		/*[ENDIF]*/
		/*[ELSE]*/
		/*[PR s66168] - ConsoleInputStream initialization may write to System.err */
		/*[PR s73550, s74314] ConsolePrintStream incorrectly initialized */
		setErr(com.ibm.jvm.io.ConsolePrintStream.localize(new BufferedOutputStream(new FileOutputStream(FileDescriptor.err)), true));
		setOut(com.ibm.jvm.io.ConsolePrintStream.localize(new BufferedOutputStream(new FileOutputStream(FileDescriptor.out)), true));
		/*[ENDIF]*/
	}

native static void startSNMPAgent();
	
static void completeInitialization() {
	/*[IF !Sidecar19-SE_RAWPLUSJ9]*/	
	/*[IF !Sidecar18-SE-OpenJ9]*/
	Class<?> systemInitialization = null;
	Method hook;
	try {
		/*[PR 167854] - Please note the incorrect name of the class is correct - somewhere the spelling was wrong and we just need to use the incorrect spelling */
		/*[IF Sidecar19-SE]*/
		systemInitialization = Class.forName("com.ibm.utils.SystemIntialization"); //$NON-NLS-1$
		/*[ELSE]*/
		systemInitialization = Class.forName("com.ibm.misc.SystemIntialization"); //$NON-NLS-1$
		/*[ENDIF]*/
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
	/*[ENDIF]*/ // Sidecar18-SE-OpenJ9
	
	/*[IF Sidecar18-SE-OpenJ9|Sidecar19-SE]*/
	setIn(new BufferedInputStream(new FileInputStream(FileDescriptor.in)));
	/*[ELSE]*/
	/*[PR 100718] Initialize System.in after the main thread*/
	setIn(com.ibm.jvm.io.ConsoleInputStream.localize(new BufferedInputStream(new FileInputStream(FileDescriptor.in))));
	/*[ENDIF]*/ //Sidecar18-SE-OpenJ9|Sidecar19-SE
	/*[ENDIF]*/ //!Sidecar19-SE_RAWPLUSJ9
		
	/*[PR 102344] call Terminator.setup() after Thread init */
	Terminator.setup();

	/*[IF Sidecar19-SE]*/
	initGPUAssist();
	/*[ENDIF] Sidecar19-SE */

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
}

/*[IF Sidecar19-SE]*/
private static void initGPUAssist() {
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

	PrivilegedAction<GPUAssist> finder = new PrivilegedAction<GPUAssist>() {
		@Override
		public GPUAssist run() {
			ServiceLoader<GPUAssist.Provider> loaded = ServiceLoader.load(GPUAssist.Provider.class);

			for (GPUAssist.Provider provider : loaded) {
				GPUAssist assist = provider.getGPUAssist();

				if (assist != null) {
					return assist;
				}
			}

			return GPUAssist.NONE;
		}
	};

	GPUAssistHolder.instance = AccessController.doPrivileged(finder);
}
/*[ENDIF] Sidecar19-SE */

/**
 * Sets the value of the static slot "in" in the receiver
 * to the passed in argument.
 *
 * @param		newIn 		the new value for in.
 */
public static void setIn(InputStream newIn) {
	SecurityManager security = System.getSecurityManager();
	if (security != null)	{
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	
	setFieldImpl("in", newIn); //$NON-NLS-1$
}

/**
 * Sets the value of the static slot "out" in the receiver
 * to the passed in argument.
 *
 * @param		newOut 		the new value for out.
 */
public static void setOut(java.io.PrintStream newOut) {
	SecurityManager security = System.getSecurityManager();
	if (security != null)	{
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	setFieldImpl("out", newOut); //$NON-NLS-1$
}

/**
 * Sets the value of the static slot "err" in the receiver
 * to the passed in argument.
 *
 * @param		newErr  	the new value for err.
 */
public static void setErr(java.io.PrintStream newErr) {
	SecurityManager security = System.getSecurityManager();
	if (security != null)	{
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetIO);
	}
	
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


private static final int InitLocale = 0;
private static final int PlatformEncoding = 1;
private static final int FileEncoding = 2;
private static final int OSEncoding = 3;

/*[IF OpenJ9-RawBuild]*/
	/* This is a JCL native required only by OpenJ9 raw build.
	 * OpenJ9 raw build is a combination of OpenJ9 and OpenJDK binaries without JCL patches within extension repo.
	 * Currently OpenJ9 depends on a JCL patch to initialize platform encoding which is not available to raw build.
	 * A workaround for raw build is to invoke this JCL native which initializes platform encoding.
	 * This workaround can be removed if that JCL patch is not required.
	 */
private static native Properties initProperties(Properties props);
/*[ENDIF] OpenJ9-RawBuild */

/**
 * If systemProperties is unset, then create a new one based on the values 
 * provided by the virtual machine.
 */
@SuppressWarnings("nls")
private static void ensureProperties(boolean isInitialization) {
/*[IF OpenJ9-RawBuild]*/
	// invoke JCL native to initialize platform encoding
	initProperties(new Properties());
/*[ENDIF] OpenJ9-RawBuild */
	
/*[IF Java12]*/
	Map<String, String> initializedProperties = new Hashtable<String, String>();
/*[ELSE]
	Properties initializedProperties = new Properties();
/*[ENDIF] Java12 */

	if (osEncoding != null) {
		initializedProperties.put("os.encoding", osEncoding); //$NON-NLS-1$
	}
	/*[PR The launcher apparently needs sun.jnu.encoding property or it does not work]*/
	initializedProperties.put("ibm.system.encoding", platformEncoding); //$NON-NLS-1$
	initializedProperties.put("sun.jnu.encoding", platformEncoding); //$NON-NLS-1$
	initializedProperties.put("file.encoding", fileEncoding); //$NON-NLS-1$
	initializedProperties.put("file.encoding.pkg", "sun.io"); //$NON-NLS-1$ //$NON-NLS-2$
	/*[IF !Java12]*/
	/* System property java.specification.vendor is set via VersionProps.init(systemProperties) since JDK12 */
	initializedProperties.put("java.specification.vendor", "Oracle Corporation"); //$NON-NLS-1$ //$NON-NLS-2$
	/*[ENDIF] !Java12 */
	initializedProperties.put("java.specification.name", "Java Platform API Specification"); //$NON-NLS-1$ //$NON-NLS-2$
	initializedProperties.put("com.ibm.oti.configuration", "scar"); //$NON-NLS-1$

	String[] list = getPropertyList();
	for (int i = 0; i < list.length; i += 2) {
		String key = list[i];
		/*[PR 100209] getPropertyList should use fewer local refs */
		if (key == null) {
			break;
		}
		initializedProperties.put(key, list[i+1]);
	}
	
	/* java.lang.VersionProps.init() eventually calls into System.setProperty() where propertiesInitialized needs to be true */
	propertiesInitialized = true;

/*[IF Java12]*/
	java.lang.VersionProps.init(initializedProperties);
/*[ELSE]
	/* VersionProps.init requires systemProperties to be set */
	systemProperties = initializedProperties;

/*[IF Sidecar19-SE]*/
	java.lang.VersionProps.init();
/*[ELSE]
	sun.misc.Version.init();

	StringBuffer.initFromSystemProperties(systemProperties);
	StringBuilder.initFromSystemProperties(systemProperties);
/*[ENDIF] Sidecar19-SE */
/*[ENDIF] Java12 */

/*[IF Java12]*/
	String javaRuntimeVersion = initializedProperties.get("java.runtime.version"); //$NON-NLS-1$
/*[ELSE]
	String javaRuntimeVersion = initializedProperties.getProperty("java.runtime.version"); //$NON-NLS-1$
/*[ENDIF] Java12 */
	if (null != javaRuntimeVersion) {
	/*[IF Java12]*/
		String fullVersion = initializedProperties.get("java.fullversion"); //$NON-NLS-1$
	/*[ELSE]
		String fullVersion = initializedProperties.getProperty("java.fullversion"); //$NON-NLS-1$
	/*[ENDIF] Java12 */
		if (null != fullVersion) {
			initializedProperties.put("java.fullversion", (javaRuntimeVersion + "\n" + fullVersion)); //$NON-NLS-1$ //$NON-NLS-2$
		}
		rasInitializeVersion(javaRuntimeVersion);
	}

/*[IF Java12]*/
	lineSeparator = initializedProperties.getOrDefault("line.separator", "\n"); //$NON-NLS-1$
/*[ELSE]
	lineSeparator = initializedProperties.getProperty("line.separator", "\n"); //$NON-NLS-1$
/*[ENDIF] Java12 */
	/*[IF Sidecar19-SE]*/
	useLegacyVerPresents = initializedProperties.containsKey("use.legacy.version");
	/*[ENDIF]*/

	if (isInitialization) {
		/*[PR CMVC 179976] System.setProperties(null) throws IllegalStateException */
		/*[IF Java12]*/
		VM.saveProperties(initializedProperties);
		/*[ELSE]
		VM.saveAndRemoveProperties(initializedProperties);
		/*[ENDIF] Java12 */
	}

	/* create systemProperties from properties Map */
/*[IF Java12]*/
	initializeSystemProperties(initializedProperties);
/*[ELSE]
	systemProperties = initializedProperties;
/*[ENDIF] Java12 */
}

/* Converts a Map<String, String> to a properties object.
 * 
 * The system properties will be initialized as a Map<String, String> type to be compatible
 * with jdk.internal.misc.VM and java.lang.VersionProps APIs.
 */
private static void initializeSystemProperties(Map<String, String> mapProperties) {
	systemProperties = new Properties();
	for (Map.Entry<String, String> property : mapProperties.entrySet()) {
		systemProperties.put(property.getKey(), property.getValue());
	}
}

private static native void rasInitializeVersion(String javaRuntimeVersion);

/**
 * Causes the virtual machine to stop running, and the
 * program to exit. If runFinalizersOnExit(true) has been
 * invoked, then all finalizers will be run first.
 * 
 * @param		code		the return code.
 *
 * @throws		SecurityException 	if the running thread is not allowed to cause the vm to exit.
 *
 * @see			SecurityManager#checkExit
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
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(new RuntimePermission("getenv." + var)); //$NON-NLS-1$
	
	return ProcessEnvironment.getenv(var);
}

/**
 * Answers the system properties. Note that this is
 * not a copy, so that changes made to the returned
 * Properties object will be reflected in subsequent
 * calls to getProperty and getProperties.
 * <p>
 * Security managers should restrict access to this
 * API if possible.
 *
 * @return		the system properties
 */
public static Properties getProperties() {
	if (!propertiesInitialized) throw new Error("bootstrap error, system property access before init"); //$NON-NLS-1$
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPropertiesAccess();
	
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
 * Answers null if no such property exists, 
 * <p>
 * The properties currently provided by the virtual
 * machine are:
 * <pre>
/*[IF !Java12]
 *     java.vendor
 *     java.vendor.url
 /*[ENDIF] Java12
 *     java.class.path
 *     user.home
 *     java.class.version
 *     os.version
 *     user.dir
 *     user.timezone
 *     path.separator
 *     os.name
 *     os.arch
 *     line.separator
 *     file.separator
 *     user.name
 *     java.version
 *     java.home
 * </pre>
 *
 * @param		prop		the system property to look up
 * @return		the value of the specified system property, or null if the property doesn't exist
 */
@SuppressWarnings("nls")
public static String getProperty(String prop) {
	String propertyString = getProperty(prop, null);
	/*[IF Sidecar19-SE]*/
	if (useLegacyVerPresents && prop.equals("java.version")) {
		StackWalker walker = StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE);

		/* If jdk.internal.misc.VM.initLevel() < 4, bootstrap isn't completed yet, walker.getCallerClass().getModule()
		 * can be null, return real version in this case. 
		 */
		Module callerModule = walker.getCallerClass().getModule();
		
		if ((jdk.internal.misc.VM.initLevel() >= 4) 
			&& ((null == callerModule) || (!callerModule.isNamed()))
		) {
			propertyString = "1.9.0";
		}
	}
	/*[ENDIF]*/
	return propertyString;
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

	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPropertyAccess(prop);

	if (!propertiesInitialized 
			&& !prop.equals("com.ibm.IgnoreMalformedInput") //$NON-NLS-1$
			&& !prop.equals("file.encoding.pkg") //$NON-NLS-1$
			&& !prop.equals("sun.nio.cs.map")) { //$NON-NLS-1$
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
	
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(
			new PropertyPermission(prop, "write")); //$NON-NLS-1$
	
	return (String)systemProperties.setProperty(prop, value);
}

/**
 * Answers an array of Strings containing key..value pairs
 * (in consecutive array elements) which represent the
 * starting values for the system properties as provided
 * by the virtual machine.
 *
 * @return		the default values for the system properties.
 */
private static native String [] getPropertyList();


/**
 * Return the requested encoding.
 * 		0 - initialize locale
 * 		1 - detected platform encoding
 * 		2 - command line defined file.encoding
 * 		3 - command line defined os.encoding
 */
private static native String getEncoding(int type);

/**
 * Answers the active security manager.
 *
 * @return		the system security manager object.
 */
public static SecurityManager getSecurityManager() {
	return security;
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
	return J9VMInternals.fastIdentityHashCode(anObject);
}

/**
 * Loads the specified file as a dynamic library.
 * 
 * @param 		pathName	the path of the file to be loaded
 */
@CallerSensitive
public static void load(String pathName) {
	SecurityManager smngr = System.getSecurityManager();
	if (smngr != null)
		smngr.checkLink(pathName);
	ClassLoader.loadLibraryWithPath(pathName, ClassLoader.callerClassLoader(), null);
}

/**
 * Loads and links the library specified by the argument.
 *
 * @param		libName		the name of the library to load
 *
 * @throws		UnsatisfiedLinkError	if the library could not be loaded
 * @throws		SecurityException 		if the library was not allowed to be loaded
 */
@CallerSensitive
public static void loadLibrary(String libName) {
	ClassLoader.loadLibraryWithClassLoader(libName, ClassLoader.callerClassLoader());
}

/**
 * Provides a hint to the virtual machine that it would
 * be useful to attempt to perform any outstanding
 * object finalizations.
 */
public static void runFinalization() {
	RUNTIME.runFinalization();
}

/*[IF !Java11]*/
/**
 * Ensure that, when the virtual machine is about to exit,
 * all objects are finalized. Note that all finalization
 * which occurs when the system is exiting is performed
 * after all running threads have been terminated.
 *
 * @param 		flag 		true means finalize all on exit.
 *
 * @deprecated 	This method is unsafe.
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=true, since="1.2")
/*[ELSE]
@Deprecated
/*[ENDIF]*/
public static void runFinalizersOnExit(boolean flag) {
	Runtime.runFinalizersOnExit(flag);
}
/*[ENDIF]*/

/**
 * Answers the system properties. Note that the object
 * which is passed in not copied, so that subsequent 
 * changes made to the object will be reflected in
 * calls to getProperty and getProperties.
 * <p>
 * Security managers should restrict access to this
 * API if possible.
 * 
 * @param		p			the property to set
 */
public static void setProperties(Properties p) {
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPropertiesAccess();
	if (p == null) {
		ensureProperties(false);
	} else {
		systemProperties = p;
	}
}

/**
 * Sets the active security manager. Note that once
 * the security manager has been set, it can not be
 * changed. Attempts to do so will cause a security
 * exception.
 *
 * @param		s			the new security manager
 * 
 * @throws		SecurityException 	if the security manager has already been set and its checkPermission method doesn't allow it to be replaced.
 /*[IF Java12]
 * @throws		UnsupportedOperationException 	if s is non-null and a special token "disallow" has been set for system property "java.security.manager"
 * 												which indicates that a security manager is not allowed to be set dynamically.
 /*[ENDIF] Java12
 */
public static void setSecurityManager(final SecurityManager s) {
	/*[PR 113606] security field could be modified by another Thread */
	final SecurityManager currentSecurity = security;
	
	if (s != null) {
		/*[IF Java12]*/
		if ("disallow".equals(systemProperties.getProperty("java.security.manager"))) { //$NON-NLS-1$ //$NON-NLS-2$
			/*[MSG "K0B00", "`-Djava.security.manager=disallow` has been specified"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0B00")); //$NON-NLS-1$
		}
		/*[ENDIF] Java12 */
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
						com.ibm.oti.util.Msg.getString("K002c"); //$NON-NLS-1$
					}
					ProtectionDomain oldDomain = currentSecurity == null ?
						System.class.getPDImpl() : currentSecurity.getClass().getPDImpl();
					ProtectionDomain newDomain = s.getClass().getPDImpl();
					if (oldDomain != newDomain) {
						// initialize the protection domain, which may include preloading the
						// dynamic permissions from the policy before installing
						newDomain.implies(new AllPermission());
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
}

/**
 * Answers the platform specific file name format for the shared
 * library named by the argument.
 *
 * @param		userLibName 	the name of the library to look up.
 * @return		the platform specific filename for the library
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
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(new PropertyPermission(prop, "write")); //$NON-NLS-1$
	return (String)systemProperties.remove(prop);
}

/**
 * Returns all of the system environment variables in an unmodifiable Map.
 * 
 * @return	an unmodifiable Map containing all of the system environment variables.
 */
public static Map<String, String> getenv() {
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(new RuntimePermission("getenv.*")); //$NON-NLS-1$
		
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
 * Return platform specific line separator character(s)
 * Unix is \n while Windows is \r\n as per the prop set by the VM 
 * Refer to Jazz 30875
 *  
 * @return platform specific line separator character(s)
 */
public static String lineSeparator() {
	 return lineSeparator;
}

/*[IF Sidecar19-SE]*/
/**
 * Return an instance of Logger.
 * 
 * @param loggerName The name of the logger to return
 * @return An instance of the logger.
 * @throws NullPointerException if the loggerName is null
 */
@CallerSensitive
public static Logger getLogger(String loggerName) {
	loggerName = Objects.requireNonNull(loggerName);
	Class<?> caller = Reflection.getCallerClass();
	return jdk.internal.logger.LazyLoggers.getLogger(loggerName, caller.getModule());
}

/**
 * Return an instance of Logger that is localized based on the ResourceBundle
 * @param loggerName The name of the logger to return
 * @param bundle The ResourceBundle to use for localization
 * @return An instance of the logger localized to 'bundle'
 * @throws NullPointerException if the loggerName or bundle is null
 */
@CallerSensitive
public static Logger getLogger(String loggerName, ResourceBundle bundle) { 
	loggerName = Objects.requireNonNull(loggerName);
	bundle = Objects.requireNonNull(bundle);
	Class<?> caller = Reflection.getCallerClass();
	return LoggerFinder.getLoggerFinder().getLocalizedLogger(loggerName, bundle, caller.getModule());
}

/**
 * The LoggerFinder service creates, manages and configures loggers
 * to the underlying framework it uses.
 */
public abstract static class LoggerFinder {
	private static volatile LoggerFinder loggerFinder = null;
	
	/**
	 * Checks needed runtime permissions
	 * 
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	 */
	protected LoggerFinder() {
		verifyPermissions();
	}

	/**
	 * Returns a localizable instance of Logger for the given module
	 * 
	 * @param loggerName The name of the logger
	 * @param bundle A resource bundle; can be null
	 * @param callerModule The module for which the logger is being requested
	 * @return an instance of Logger
	 * @throws NullPointerException if loggerName or callerModule is null
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	 */
	public Logger getLocalizedLogger(String loggerName, ResourceBundle bundle, Module callerModule) {
		verifyPermissions();
		loggerName = Objects.requireNonNull(loggerName);
		callerModule = Objects.requireNonNull(callerModule);
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
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	 */
	public abstract Logger getLogger(String loggerName, Module callerModule);

	/**
	 * Returns the LoggerFinder instance
	 * 
	 * @return the LoggerFinder instance.
	 * @throws SecurityException if RuntimePermission("loggerFinder") is not allowed
	 */
	public static LoggerFinder getLoggerFinder() {
		verifyPermissions();
		if (loggerFinder == null) {
			loggerFinder = AccessController.doPrivileged(
								(PrivilegedAction<LoggerFinder>) () -> jdk.internal.logger.LoggerFinderLoader.getLoggerFinder(),
								AccessController.getContext(),
								com.ibm.oti.util.RuntimePermissions.permissionLoggerFinder);
		}
		return loggerFinder;
	}
	
	private static void verifyPermissions() {
		SecurityManager securityManager = System.getSecurityManager();
		if (securityManager != null)	{
			securityManager.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionLoggerFinder);
		}
	}
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
		level = Objects.requireNonNull(level);
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
		level = Objects.requireNonNull(level);
		supplier = Objects.requireNonNull(supplier);
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
		level = Objects.requireNonNull(level);
		value = Objects.requireNonNull(value);
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
		level = Objects.requireNonNull(level);
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
		level = Objects.requireNonNull(level);
		supplier = Objects.requireNonNull(supplier);
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
		level = Objects.requireNonNull(level);
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
/*[ENDIF]*/
}
