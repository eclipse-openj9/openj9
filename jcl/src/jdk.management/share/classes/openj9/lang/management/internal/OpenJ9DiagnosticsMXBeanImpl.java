/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2018
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
package openj9.lang.management.internal;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
/*[IF JAVA_SPEC_VERSION >= 9]*/
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.PrivilegedAction;
import java.util.Optional;
/*[ELSE] JAVA_SPEC_VERSION >= 9 */
import com.ibm.jvm.Dump;
import com.ibm.jvm.DumpConfigurationUnavailableException;
import com.ibm.jvm.InvalidDumpOptionException;
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
import com.ibm.java.lang.management.internal.ManagementPermissionHelper;

import openj9.lang.management.ConfigurationUnavailableException;
import openj9.lang.management.InvalidOptionException;
import openj9.lang.management.OpenJ9DiagnosticsMXBean;

/**
 * Runtime type for {@link OpenJ9DiagnosticsMXBean}.
 * <p>
 * Implements functionality to dynamically configure dump options and to trigger supported dump agents.
 * </p>
 */
public final class OpenJ9DiagnosticsMXBeanImpl implements OpenJ9DiagnosticsMXBean {

	private static final OpenJ9DiagnosticsMXBean instance = createInstance();

	/*[IF JAVA_SPEC_VERSION >= 9]*/
	private final Class<?> dumpConfigurationUnavailableExClass;
	private final Class<?> invalidDumpOptionExClass;
	private final Method dump_HeapDump;
	private final Method dump_heapDumpToFile;
	private final Method dump_JavaDump;
	private final Method dump_javaDumpToFile;
	private final Method dump_queryDumpOptions;
	private final Method dump_resetDumpOptions;
	private final Method dump_setDumpOptions;
	private final Method dump_SnapDump;
	private final Method dump_snapDumpToFile;
	private final Method dump_SystemDump;
	private final Method dump_systemDumpToFile;
	private final Method dump_triggerDump;
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	@SuppressWarnings("removal")
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	private static OpenJ9DiagnosticsMXBean createInstance() {
		/*[IF JAVA_SPEC_VERSION > 8]*/
		// TODO remove the dependency on the openj9.jvm module
		final Optional<Module> openj9_jvm = ModuleLayer.boot().findModule("openj9.jvm"); //$NON-NLS-1$

		if (!openj9_jvm.isPresent()) {
			return null;
		}

		PrivilegedAction<OpenJ9DiagnosticsMXBean> action = new PrivilegedAction<OpenJ9DiagnosticsMXBean>() {
			@Override
			public OpenJ9DiagnosticsMXBean run() {
				try {
					return new OpenJ9DiagnosticsMXBeanImpl(openj9_jvm.get());
				} catch (Exception e) {
					throw handleError(e);
				}
			}
		};

		return java.security.AccessController.doPrivileged(action);
		/*[ELSE] JAVA_SPEC_VERSION > 8
		return new OpenJ9DiagnosticsMXBeanImpl();
		/*[ENDIF] JAVA_SPEC_VERSION > 8 */
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void resetDumpOptions() throws ConfigurationUnavailableException {
		checkManagementSecurityPermission();
		try {
			/*[IF JAVA_SPEC_VERSION >= 9]*/
			dump_resetDumpOptions.invoke(null);
			/*[ELSE] JAVA_SPEC_VERSION >= 9 */
			Dump.resetDumpOptions();
			/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		} catch (Exception e) {
			handleDumpConfigurationUnavailableException(e);
			throw handleError(e);
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String[] queryDumpOptions() {
		checkManagementSecurityPermission();
		try {
			/*[IF JAVA_SPEC_VERSION >= 9]*/
			return (String[])dump_queryDumpOptions.invoke(null);
			/*[ELSE] JAVA_SPEC_VERSION >= 9 */
			return Dump.queryDumpOptions();
			/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		} catch (Exception e) {
			throw handleError(e);
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getDumpOptions() {
		String[] options = queryDumpOptions();
		StringBuilder builder = new StringBuilder();
		for (int i = 0; i < options.length; i++) {
			if (i != 0) {
				builder.append('|');
			}
			builder.append(options[i]);
		}
		return builder.toString();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setDumpOptions(String dumpOptions) throws InvalidOptionException, ConfigurationUnavailableException {
		checkManagementSecurityPermission();
		try {
			/*[IF JAVA_SPEC_VERSION >= 9]*/
			dump_setDumpOptions.invoke(null, dumpOptions);
			/*[ELSE] JAVA_SPEC_VERSION >= 9 */
			Dump.setDumpOptions(dumpOptions);
			/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		} catch (Exception e) {
			handleInvalidDumpOptionException(e);
			handleDumpConfigurationUnavailableException(e);
			throw handleError(e);
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void triggerDump(String dumpAgent) throws IllegalArgumentException {
		checkManagementSecurityPermission();
		switch (dumpAgent) {
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		case "java": //$NON-NLS-1$
			try {
				dump_JavaDump.invoke(null);
			} catch (Exception e) {
				throw handleError(e);
			}
			break;
		case "heap": //$NON-NLS-1$
			try {
				dump_HeapDump.invoke(null);
			} catch (Exception e) {
				throw handleError(e);
			}
			break;
		case "system": //$NON-NLS-1$
			try {
				dump_SystemDump.invoke(null);
			} catch (Exception e) {
				throw handleError(e);
			}
			break;
		case "snap": //$NON-NLS-1$
			try {
				dump_SnapDump.invoke(null);
			} catch (Exception e) {
				throw handleError(e);
			}
			break;
		/*[ELSE] JAVA_SPEC_VERSION >= 9 */
		case "java": //$NON-NLS-1$
			Dump.JavaDump();
			break;
		case "heap": //$NON-NLS-1$
			Dump.HeapDump();
			break;
		case "system": //$NON-NLS-1$
			Dump.SystemDump();
			break;
		case "snap": //$NON-NLS-1$
			Dump.SnapDump();
			break;
		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		default:
			/*[MSG "K0663", "Invalid or Unsupported Dump Agent cannot be triggered"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0663")); //$NON-NLS-1$
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String triggerDumpToFile(String dumpAgent, String fileNamePattern) throws IllegalArgumentException, InvalidOptionException {
		String fileName = null;
		checkManagementSecurityPermission();
		switch (dumpAgent) {
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		case "java": //$NON-NLS-1$
			try {
				fileName = (String) dump_javaDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "heap": //$NON-NLS-1$
			try {
				fileName = (String) dump_heapDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "system": //$NON-NLS-1$
			try {
				fileName = (String) dump_systemDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "snap": //$NON-NLS-1$
			try {
				fileName = (String) dump_snapDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		/*[ELSE] JAVA_SPEC_VERSION >= 9 */
		case "java": //$NON-NLS-1$
			try {
				fileName = Dump.javaDumpToFile(fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "heap": //$NON-NLS-1$
			try {
				fileName = Dump.heapDumpToFile(fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "system": //$NON-NLS-1$
			try {
				fileName = Dump.systemDumpToFile(fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "snap": //$NON-NLS-1$
			try {
				fileName = Dump.snapDumpToFile(fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		default:
			/*[MSG "K0663", "Invalid or Unsupported Dump Agent cannot be triggered"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0663")); //$NON-NLS-1$
		}
		return fileName;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String triggerClassicHeapDump() throws InvalidOptionException {
		String dumpOptions = "heap:opts=CLASSIC"; //$NON-NLS-1$
		checkManagementSecurityPermission();
		try {
			/*[IF JAVA_SPEC_VERSION >= 9]*/
			String fileName = (String) dump_triggerDump.invoke(null, dumpOptions);
			/*[ELSE] JAVA_SPEC_VERSION >= 9 */
			String fileName = Dump.triggerDump(dumpOptions);
			/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
			return fileName;
		} catch (Exception e) {
			handleInvalidDumpOptionException(e);
			throw handleError(e);
		}
	}

	private static void checkManagementSecurityPermission() {
		/* Check the caller has Management Permission. */
		@SuppressWarnings("removal")
		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
	}

	/**
	 * Singleton accessor method. Returns an instance of {@link OpenJ9DiagnosticsMXBeanImpl}.
	 *
	 * @return a static instance of {@link OpenJ9DiagnosticsMXBeanImpl}.
	 */
	public static OpenJ9DiagnosticsMXBean getInstance() {
		return instance;
	}

	/**
	 * Private constructor to prevent instantiation by others.
	 */
	/*[IF JAVA_SPEC_VERSION >= 9]*/
	private OpenJ9DiagnosticsMXBeanImpl(Module openj9_jvm) throws Exception {
		super();
		dumpConfigurationUnavailableExClass = Class.forName(openj9_jvm, "com.ibm.jvm.DumpConfigurationUnavailableException"); //$NON-NLS-1$
		invalidDumpOptionExClass = Class.forName(openj9_jvm, "com.ibm.jvm.InvalidDumpOptionException"); //$NON-NLS-1$

		Class<?> dumpClass = Class.forName(openj9_jvm, "com.ibm.jvm.Dump"); //$NON-NLS-1$

		dump_HeapDump = dumpClass.getMethod("HeapDump"); //$NON-NLS-1$
		dump_heapDumpToFile = dumpClass.getMethod("heapDumpToFile", String.class); //$NON-NLS-1$
		dump_JavaDump = dumpClass.getMethod("JavaDump"); //$NON-NLS-1$
		dump_javaDumpToFile = dumpClass.getMethod("javaDumpToFile", String.class); //$NON-NLS-1$
		dump_queryDumpOptions = dumpClass.getMethod("queryDumpOptions"); //$NON-NLS-1$
		dump_resetDumpOptions = dumpClass.getMethod("resetDumpOptions"); //$NON-NLS-1$
		dump_setDumpOptions = dumpClass.getMethod("setDumpOptions", String.class); //$NON-NLS-1$
		dump_SnapDump = dumpClass.getMethod("SnapDump"); //$NON-NLS-1$
		dump_snapDumpToFile = dumpClass.getMethod("snapDumpToFile", String.class); //$NON-NLS-1$
		dump_SystemDump = dumpClass.getMethod("SystemDump"); //$NON-NLS-1$
		dump_systemDumpToFile = dumpClass.getMethod("systemDumpToFile", String.class); //$NON-NLS-1$
		dump_triggerDump = dumpClass.getMethod("triggerDump", String.class); //$NON-NLS-1$
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 9 */
	private OpenJ9DiagnosticsMXBeanImpl() {
		super();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

	/**
	 * Returns the object name of the MXBean.
	 *
	 * @return objectName representing the MXBean.
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			return ObjectName.getInstance("openj9.lang.management:type=OpenJ9Diagnostics"); //$NON-NLS-1$
		} catch (MalformedObjectNameException e) {
			throw new InternalError(e);
		}
	}

	/* Handle error thrown by method invoke as internal error */
	private static InternalError handleError(Exception error) {
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		// invoke throws InvocationTargetException if the method it is invoking throws an error.
		// Unwrap that error for this class to maintain its specification.
		if (error instanceof InvocationTargetException) {
			Throwable cause = error.getCause();

			if (cause instanceof Error) {
				throw (Error) cause;
			} else if (cause instanceof RuntimeException) {
				throw (RuntimeException) cause;
			}
		}

		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
		throw new InternalError(error.toString(), error);
	}

	private void handleInvalidDumpOptionException(Exception cause) throws InvalidOptionException {
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		if (invalidDumpOptionExClass.isInstance(cause.getCause())) {
			throw new InvalidOptionException("Error in dump options specified", cause.getCause()); //$NON-NLS-1$
		}
		/*[ELSE] JAVA_SPEC_VERSION >= 9 */
		if (cause instanceof InvalidDumpOptionException) {
			throw new InvalidOptionException("Error in dump options specified", cause); //$NON-NLS-1$
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
	}

	private void handleDumpConfigurationUnavailableException(Exception cause) throws ConfigurationUnavailableException {
		/*[IF JAVA_SPEC_VERSION >= 9]*/
		if (dumpConfigurationUnavailableExClass.isInstance(cause.getCause())) {
			throw new ConfigurationUnavailableException("Dump configuration cannot be changed while a dump is in progress", cause.getCause()); //$NON-NLS-1$
		}
		/*[ELSE] JAVA_SPEC_VERSION >= 9 */
		if (cause instanceof DumpConfigurationUnavailableException) {
			throw new ConfigurationUnavailableException("Dump configuration cannot be changed while a dump is in progress", cause); //$NON-NLS-1$
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
	}

}
