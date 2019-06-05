/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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
/*[IF Sidecar19-SE]*/
import java.lang.Module;
import java.lang.ModuleLayer;
/*[ENDIF]*/
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
/*[IF !Sidecar19-SE]
import java.util.logging.LoggingMXBean;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;
/*[ENDIF]*/

import javax.management.ObjectName;
import javax.management.StandardMBean;

/**
 * Runtime type for {@link java.lang.management.PlatformLoggingMXBean}.
 *
 * @since 1.5
 */
public final class LoggingMXBeanImpl
		extends StandardMBean
		implements
/*[IF !Sidecar19-SE]
			LoggingMXBean,
/*[ENDIF]*/
			PlatformLoggingMXBean {

	private static final LoggingMXBeanImpl instance;

/*[IF Sidecar19-SE]*/
	private static final Method logManager_getLogManager;
	private static final Method logManager_getLogger;
	private static final Method logManager_getLoggerNames;
	private static final Method logger_getLevel;
	private static final Method logger_getParent;
	private static final Method logger_getName;
	private static final Method logger_setLevel;
	private static final Method level_getName;
	private static final Method level_parse;
	private static final String logManager_LOGGING_MXBEAN_NAME;
/*[ENDIF]*/	

	static {
/*[IF Sidecar19-SE]*/
		try {

			Module java_logging = ModuleLayer.boot().findModule("java.logging").get(); //$NON-NLS-1$
			Class<?>[] logClasses = AccessController.doPrivileged((PrivilegedAction<Class<?>[]>)
				() -> new Class[] {
					Class.forName(java_logging, "java.util.logging.LogManager"), //$NON-NLS-1$
					Class.forName(java_logging, "java.util.logging.Logger"), //$NON-NLS-1$
					Class.forName(java_logging, "java.util.logging.Level") //$NON-NLS-1$
				});
			Class<?> logManagerClass = logClasses[0];
			Class<?> loggerClass = logClasses[1];
			Class<?> levelClass = logClasses[2];
			
			logManager_getLogManager = logManagerClass.getMethod("getLogManager"); //$NON-NLS-1$
			logManager_getLogger = logManagerClass.getMethod("getLogger", String.class); //$NON-NLS-1$
			logManager_getLoggerNames = logManagerClass.getMethod("getLoggerNames"); //$NON-NLS-1$
			logger_getLevel = loggerClass.getMethod("getLevel"); //$NON-NLS-1$
			logger_getParent = loggerClass.getMethod("getParent"); //$NON-NLS-1$
			logger_getName = loggerClass.getMethod("getName"); //$NON-NLS-1$
			logger_setLevel = loggerClass.getMethod("setLevel", levelClass); //$NON-NLS-1$
			level_getName = levelClass.getMethod("getName"); //$NON-NLS-1$
			level_parse = levelClass.getMethod("parse", String.class); //$NON-NLS-1$
			logManager_LOGGING_MXBEAN_NAME = logManagerClass.getField("LOGGING_MXBEAN_NAME").get(null).toString(); //$NON-NLS-1$

		} catch (Exception e) {
			throw handleError(e);
		}
/*[ENDIF]*/		
		
		/* Initialize class */
		instance = new LoggingMXBeanImpl();
	}
	
	/**
	 * the object name
	 */
	private final ObjectName objectName;

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean.
	 */
	private LoggingMXBeanImpl() {
		super(PlatformLoggingMXBean.class, true);
/*[IF Sidecar19-SE]*/
		objectName = ManagementUtils.createObjectName(logManager_LOGGING_MXBEAN_NAME);
/*[ELSE]		
		objectName = ManagementUtils.createObjectName(LogManager.LOGGING_MXBEAN_NAME);
/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
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
					result = (String)level_getName.invoke(level);
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
/*[ELSE]	
		enumeration = LogManager.getLogManager().getLoggerNames();
/*[ENDIF]*/	
		if (enumeration != null) {
			while (enumeration.hasMoreElements()) {
				result.add((String)enumeration.nextElement());
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
					result = (String)logger_getName.invoke(parent);
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
/*[ELSE]	
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
	private static Object getLoggerFromName(String loggerName) throws Exception {
		Object logManagerInstance = logManager_getLogManager.invoke(null);
		return logManager_getLogger.invoke(logManagerInstance, loggerName);
	}
	
	/* Handle error thrown by method invoke as internal error */
	private static InternalError handleError(Exception cause) {
		handleInvocationTargetException(cause);
		
		InternalError err = new InternalError(cause.toString());
		err.initCause(cause);
		throw err;
	}
	
	/* invoke throws InvocationTargetException if the method it is invoking
	 * throws an error. This method unwraps that error for this class to maintain
	 * its specification.
	 */
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
