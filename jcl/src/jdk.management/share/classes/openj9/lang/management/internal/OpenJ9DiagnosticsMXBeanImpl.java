/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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


package openj9.lang.management.internal;

/*[IF Sidecar19-SE]*/
import java.lang.Module;
import java.lang.ModuleLayer;
/*[ENDIF]*/
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import java.security.AccessController;
import java.security.PrivilegedAction;
/*[IF !Sidecar19-SE]
import com.ibm.jvm.Dump;
import com.ibm.jvm.InvalidDumpOptionException;
import com.ibm.jvm.DumpConfigurationUnavailableException;
/*[ENDIF]*/
import com.ibm.java.lang.management.internal.ManagementPermissionHelper;

import openj9.lang.management.OpenJ9DiagnosticsMXBean;
import openj9.lang.management.InvalidOptionException;
import openj9.lang.management.ConfigurationUnavailableException;

/**
 * Runtime type for {@link OpenJ9DiagnosticsMXBean}.
 * <p>
 * Implements functionality to dynamically configure dump options and to trigger supported dump agents.
 * </p>
 */
public final class OpenJ9DiagnosticsMXBeanImpl implements OpenJ9DiagnosticsMXBean {
	private final static OpenJ9DiagnosticsMXBeanImpl instance;

/*[IF Sidecar19-SE]*/
	private static final Method dump_setDumpOptions;
	private static final Method dump_resetDumpOptions;
	private static final Method dump_triggerDump;
	private static final Method dump_JavaDump;
	private static final Method dump_HeapDump;
	private static final Method dump_SystemDump;
	private static final Method dump_SnapDump;
	private static final Method dump_javaDumpToFile;
	private static final Method dump_heapDumpToFile;
	private static final Method dump_systemDumpToFile;
	private static final Method dump_snapDumpToFile;
	private static final Class<?> invalidDumpOptionExClass;
	private static final Class<?> dumpConfigurationUnavailableExClass;
/*[ENDIF]*/

	static {
/*[IF Sidecar19-SE]*/
		try {
			Module openj9_jvm = ModuleLayer.boot().findModule("openj9.jvm").get(); //$NON-NLS-1$
			Class<?>[] classes = AccessController.doPrivileged((PrivilegedAction<Class<?>[]>)
				() -> new Class[] {
					Class.forName(openj9_jvm, "com.ibm.jvm.Dump"), //$NON-NLS-1$
					Class.forName(openj9_jvm, "com.ibm.jvm.InvalidDumpOptionException"), //$NON-NLS-1$
					Class.forName(openj9_jvm, "com.ibm.jvm.DumpConfigurationUnavailableException"), //$NON-NLS-1$
				});


			Class<?> dumpClass = classes[0];
			invalidDumpOptionExClass = classes[1];
			dumpConfigurationUnavailableExClass = classes[2];

			dump_setDumpOptions = dumpClass.getMethod("setDumpOptions", String.class); //$NON-NLS-1$
			dump_resetDumpOptions = dumpClass.getMethod("resetDumpOptions"); //$NON-NLS-1$
			dump_triggerDump = dumpClass.getMethod("triggerDump", String.class); //$NON-NLS-1$
			dump_JavaDump = dumpClass.getMethod("JavaDump"); //$NON-NLS-1$
			dump_HeapDump = dumpClass.getMethod("HeapDump"); //$NON-NLS-1$
			dump_SystemDump = dumpClass.getMethod("SystemDump"); //$NON-NLS-1$
			dump_SnapDump = dumpClass.getMethod("SnapDump"); //$NON-NLS-1$
			dump_javaDumpToFile = dumpClass.getMethod("javaDumpToFile", String.class); //$NON-NLS-1$
			dump_heapDumpToFile = dumpClass.getMethod("heapDumpToFile", String.class); //$NON-NLS-1$
			dump_systemDumpToFile = dumpClass.getMethod("systemDumpToFile", String.class); //$NON-NLS-1$
			dump_snapDumpToFile = dumpClass.getMethod("snapDumpToFile", String.class); //$NON-NLS-1$
		} catch (Exception e) {
			throw handleError(e);
		}
/*[ENDIF]*/
		instance = new OpenJ9DiagnosticsMXBeanImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void resetDumpOptions() throws ConfigurationUnavailableException {
		checkManagementSecurityPermission();
		try {
/*[IF Sidecar19-SE]*/
			dump_resetDumpOptions.invoke(null);
/*[ELSE]
			Dump.resetDumpOptions();
/*[ENDIF]*/
		} catch (Exception e) {
			handleDumpConfigurationUnavailableException(e);
			throw handleError(e);
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setDumpOptions(String dumpOptions) throws InvalidOptionException, ConfigurationUnavailableException {
		checkManagementSecurityPermission();
		try {
/*[IF Sidecar19-SE]*/
			dump_setDumpOptions.invoke(null, dumpOptions);
/*[ELSE]
			Dump.setDumpOptions(dumpOptions);
/*[ENDIF]*/
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
/*[IF Sidecar19-SE]*/
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
/*[ELSE]
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
/*[ENDIF]*/
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
/*[IF Sidecar19-SE]*/
		case "java": //$NON-NLS-1$
			try {
				fileName = (String)dump_javaDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "heap": //$NON-NLS-1$
			try {
				fileName = (String)dump_heapDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "system": //$NON-NLS-1$
			try {
				fileName = (String)dump_systemDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
		case "snap": //$NON-NLS-1$
			try {
				fileName = (String)dump_snapDumpToFile.invoke(null, fileNamePattern);
			} catch (Exception e) {
				handleInvalidDumpOptionException(e);
				throw handleError(e);
			}
			break;
/*[ELSE]
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
/*[ENDIF]*/
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
/*[IF Sidecar19-SE]*/
			String fileName = (String)dump_triggerDump.invoke(null, dumpOptions);
/*[ELSE]
			String fileName = Dump.triggerDump(dumpOptions);
/*[ENDIF]*/
			return fileName;
		} catch (Exception e) {

			handleInvalidDumpOptionException(e);
			throw handleError(e);
		}
	}

	private static void checkManagementSecurityPermission() {
		/* Check the caller has Management Permission. */
		SecurityManager manager = System.getSecurityManager();
		if( manager != null ) {
			manager.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
	}

	/**
	 * Singleton accessor method. Returns an instance of {@link OpenJ9DiagnosticsMXBeanImpl}.
	 *
	 * @return a static instance of {@link OpenJ9DiagnosticsMXBeanImpl}.
	 */
	public static OpenJ9DiagnosticsMXBeanImpl getInstance() {
		return instance;
	}

	/**
	 * Empty constructor intentionally private to prevent instantiation by others.
	 */
	private OpenJ9DiagnosticsMXBeanImpl() {
		super();
	}

	/**
	 * Returns the object name of the MXBean.
	 *
	 * @return objectName representing the MXBean.
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			ObjectName name = new ObjectName("openj9.lang.management:type=OpenJ9Diagnostics");  //$NON-NLS-1$
			return name;
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}


	/* Handle error thrown by method invoke as internal error */
	private static InternalError handleError(Exception cause) {
/*[IF Sidecar19-SE]*/
		handleInvocationTargetException(cause);
/*[ENDIF]*/
		InternalError err = new InternalError(cause.toString());
		err.initCause(cause);
		throw err;
	}

	private static void handleInvalidDumpOptionException(Exception cause) throws InvalidOptionException {
/*[IF Sidecar19-SE]*/
		if (invalidDumpOptionExClass.isInstance(cause.getCause())) {
			throw new InvalidOptionException("Error in dump options specified", cause.getCause());
		}
/*[ELSE]
		if (cause instanceof InvalidDumpOptionException) {
			throw new InvalidOptionException("Error in dump options specified", cause);
		}
/*[ENDIF]*/
	}

	private static void handleDumpConfigurationUnavailableException(Exception cause) throws ConfigurationUnavailableException {
/*[IF Sidecar19-SE]*/
		if (dumpConfigurationUnavailableExClass.isInstance(cause.getCause())) {
			throw new ConfigurationUnavailableException("Dump configuration cannot be changed while a dump is in progress", cause.getCause());
		}
/*[ELSE]
		if (cause instanceof DumpConfigurationUnavailableException) {
			throw new ConfigurationUnavailableException("Dump configuration cannot be changed while a dump is in progress", cause);
		}
/*[ENDIF]*/
	}

	/* invoke throws InvocationTargetException if the method it is invoking
	 * throws an error. This method unwraps that error for this class to maintain
	 * its specification.
	 */
/*[IF Sidecar19-SE]*/
	private static void handleInvocationTargetException(Exception cause) {
		if (cause instanceof InvocationTargetException) {
			Throwable wrappedCause = cause.getCause();
			if (wrappedCause instanceof Error) {
				throw (Error)wrappedCause;
			} else if (wrappedCause instanceof RuntimeException) {
				throw (RuntimeException)wrappedCause;
			}
		}
	}
/*[ENDIF]*/
}

