/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2020 IBM Corp. and others
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
package com.ibm.java.lang.management.internal;

import java.lang.management.PlatformLoggingMXBean;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
/*[IF Sidecar19-SE]*/
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Optional;
/*[ELSE]*/
import java.util.logging.LoggingMXBean;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;
/*[ENDIF]*/

import javax.management.ObjectName;

/**
 * Runtime type for {@link java.lang.management.PlatformLoggingMXBean}.
 *
 * @since 1.5
 */
public final class LoggingMXBeanImpl
/*[IF !Sidecar19-SE]*/
		extends javax.management.StandardMBean
/*[ENDIF]*/
		implements
/*[IF !Sidecar19-SE]*/
			LoggingMXBean,
/*[ENDIF]*/
			PlatformLoggingMXBean {

	private static final LoggingMXBeanImpl instance = createInstance();

	private static LoggingMXBeanImpl createInstance() {
		/*[IF Sidecar19-SE]*/
		final Optional<Module> java_logging = ModuleLayer.boot().findModule("java.logging"); //$NON-NLS-1$

		if (!java_logging.isPresent()) {
			return null;
		}

		PrivilegedAction<LoggingMXBeanImpl> action = new PrivilegedAction<LoggingMXBeanImpl>() {
			@Override
			public LoggingMXBeanImpl run() {
				try {
					return new LoggingMXBeanImpl(java_logging.get());
				} catch (Exception e) {
					throw handleError(e);
				}
			}
		};

		return AccessController.doPrivileged(action);
		/*[ELSE]
		return new LoggingMXBeanImpl();
		/*[ENDIF]*/
	}

	/*[IF Sidecar19-SE]*/
	private final Field logManager_LOGGING_MXBEAN_NAME;
	private final Method logManager_getLogManager;
	private final Method logManager_getLogger;
	private final Method logManager_getLoggerNames;

	private final Method level_getName;
	private final Method level_parse;

	private final Method logger_getLevel;
	private final Method logger_getName;
	private final Method logger_getParent;
	private final Method logger_setLevel;
	/*[ENDIF]*/

	/**
	 * the object name
	 */
	private ObjectName objectName;

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 */
	/*[IF Sidecar19-SE]*/
	private LoggingMXBeanImpl(Module logging) throws Exception {
		super();

		Class<?> managerClass = Class.forName(logging, "java.util.logging.LogManager"); //$NON-NLS-1$

		logManager_LOGGING_MXBEAN_NAME = managerClass.getField("LOGGING_MXBEAN_NAME"); //$NON-NLS-1$
		logManager_getLogManager = managerClass.getMethod("getLogManager"); //$NON-NLS-1$
		logManager_getLogger = managerClass.getMethod("getLogger", String.class); //$NON-NLS-1$
		logManager_getLoggerNames = managerClass.getMethod("getLoggerNames"); //$NON-NLS-1$

		Class<?> levelClass = Class.forName(logging, "java.util.logging.Level"); //$NON-NLS-1$

		level_getName = levelClass.getMethod("getName"); //$NON-NLS-1$
		level_parse = levelClass.getMethod("parse", String.class); //$NON-NLS-1$

		Class<?> loggerClass = Class.forName(logging, "java.util.logging.Logger"); //$NON-NLS-1$

		logger_getLevel = loggerClass.getMethod("getLevel"); //$NON-NLS-1$
		logger_getName = loggerClass.getMethod("getName"); //$NON-NLS-1$
		logger_getParent = loggerClass.getMethod("getParent"); //$NON-NLS-1$
		logger_setLevel = loggerClass.getMethod("setLevel", levelClass); //$NON-NLS-1$
	}
	/*[ELSE]*/
	private LoggingMXBeanImpl() {
		super(PlatformLoggingMXBean.class, true);
	}
	/*[ENDIF]*/

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		if (objectName == null) {
/*[IF Sidecar19-SE]*/
			try {
				objectName = ManagementUtils.createObjectName((String) logManager_LOGGING_MXBEAN_NAME.get(null));
			} catch (IllegalAccessException e) {
				/* The field is public so this should never happen. */
				throw new InternalError(e);
			}
/*[ELSE]*/
			objectName = ManagementUtils.createObjectName(LogManager.LOGGING_MXBEAN_NAME);
/*[ENDIF]*/
		}
		return objectName;
	}

	/**
	 * Singleton accessor method.
	 *
	 * @return the <code>LoggingMXBeanImpl</code> singleton.
	 */
	public static LoggingMXBeanImpl getInstance() {
		return instance;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getLoggerLevel(String loggerName) {
		String result = null;

/*[IF Sidecar19-SE]*/
		try {
			Object logger = getLoggerFromName(loggerName);
/*[ELSE]
			Logger logger = LogManager.getLogManager().getLogger(loggerName);
/*[ENDIF]*/

			if (logger != null) {
				// The named Logger exists. Now attempt to obtain its log level.
/*[IF Sidecar19-SE]*/
				Object level = logger_getLevel.invoke(logger);
/*[ELSE]
				Level level = logger.getLevel();
/*[ENDIF]*/
				if (level != null) {
/*[IF Sidecar19-SE]*/
					result = (String) level_getName.invoke(level);
/*[ELSE]
					result = level.getName();
/*[ENDIF]*/
				} else {
					// A null return from getLevel() means that the Logger
					// is inheriting its log level from an ancestor. Return an
					// empty string to the caller.
					result = ""; //$NON-NLS-1$
				}
			}
/*[IF Sidecar19-SE]*/
		} catch (Exception e) {
			throw handleError(e);
		}
/*[ENDIF]*/

		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public List<String> getLoggerNames() {
		// By default, return an empty list to caller
		List<String> result = new ArrayList<>();
		Enumeration<?> enumeration;

/*[IF Sidecar19-SE]*/
		try {
			Object logManagerInstance = logManager_getLogManager.invoke(null);
			enumeration = (Enumeration<?>) logManager_getLoggerNames.invoke(logManagerInstance);
		} catch (Exception e) {
			throw handleError(e);
		}
/*[ELSE]*/
		enumeration = LogManager.getLogManager().getLoggerNames();
/*[ENDIF]*/
		if (enumeration != null) {
			while (enumeration.hasMoreElements()) {
				result.add((String) enumeration.nextElement());
			}
		}

		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getParentLoggerName(String loggerName) {
		String result = null;

/*[IF Sidecar19-SE]*/
		try {
			Object logger = getLoggerFromName(loggerName);
/*[ELSE]
			Logger logger = LogManager.getLogManager().getLogger(loggerName);
/*[ENDIF]*/
			if (logger != null) {
				// The named Logger exists. Now attempt to obtain its parent.
/*[IF Sidecar19-SE]*/
				Object parent = logger_getParent.invoke(logger);
/*[ELSE]
				Logger parent = logger.getParent();
/*[ENDIF]*/
				if (parent != null) {
					// There is a parent
/*[IF Sidecar19-SE]*/
					result = (String) logger_getName.invoke(parent);
/*[ELSE]
					result = parent.getName();
/*[ENDIF]*/
				} else {
					// logger must be the root Logger
					result = ""; //$NON-NLS-1$
				}
			}
/*[IF Sidecar19-SE]*/
		} catch (Exception e) {
			throw handleError(e);
		}
/*[ENDIF]*/

		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setLoggerLevel(String loggerName, String levelName) {
/*[IF Sidecar19-SE]*/
		final Object logger;
		try {
			logger = getLoggerFromName(loggerName);
		} catch (Exception e) {
			throw handleError(e);
		}
/*[ELSE]
		final Logger logger = LogManager.getLogManager().getLogger(loggerName);
/*[ENDIF]*/

		if (logger != null) {
			// The named Logger exists. Now attempt to set its level. The
			// below attempt to parse a Level from the supplied levelName
			// will throw an IllegalArgumentException if levelName is not
			// a valid level name.
/*[IF Sidecar19-SE]*/
			try {
				Object newLevel = level_parse.invoke(null, levelName);
				logger_setLevel.invoke(logger, newLevel);
			} catch (Exception e) {
				throw handleError(e);
			}
/*[ELSE]*/
			Level newLevel = Level.parse(levelName);
			logger.setLevel(newLevel);
/*[ENDIF]*/
		} else {
			// Named Logger does not exist.
			/*[MSG "K05E7", "Unable to find Logger with name {0}."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E7", loggerName)); //$NON-NLS-1$
		}
	}

/*[IF Sidecar19-SE]*/
	private Object getLoggerFromName(String loggerName) throws Exception {
		Object logManagerInstance = logManager_getLogManager.invoke(null);
		return logManager_getLogger.invoke(logManagerInstance, loggerName);
	}

	/* Handle error thrown by method invoke as internal error */
	private static InternalError handleError(Exception error) {
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

		throw new InternalError(error.toString(), error);
	}
/*[ENDIF]*/

}
