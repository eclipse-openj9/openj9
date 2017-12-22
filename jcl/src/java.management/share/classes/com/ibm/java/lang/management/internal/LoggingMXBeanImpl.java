/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2017 IBM Corp. and others
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
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;
/*[IF !Sidecar19-SE]
import java.util.logging.LoggingMXBean;
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

	private static final LoggingMXBeanImpl instance = new LoggingMXBeanImpl();

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
		objectName = ManagementUtils.createObjectName(LogManager.LOGGING_MXBEAN_NAME);
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
		Logger logger = LogManager.getLogManager().getLogger(loggerName);

		if (logger != null) {
			// The named Logger exists. Now attempt to obtain its log level.
			Level level = logger.getLevel();
			if (level != null) {
				result = level.getName();
			} else {
				// A null return from getLevel() means that the Logger
				// is inheriting its log level from an ancestor. Return an
				// empty string to the caller.
				result = ""; //$NON-NLS-1$
			}
		}
		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public List<String> getLoggerNames() {
		// By default, return an empty list to caller
		List<String> result = new ArrayList<>();
		Enumeration<String> enumeration = LogManager.getLogManager().getLoggerNames();

		if (enumeration != null) {
			while (enumeration.hasMoreElements()) {
				result.add(enumeration.nextElement());
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
		Logger logger = LogManager.getLogManager().getLogger(loggerName);

		if (logger != null) {
			// The named Logger exists. Now attempt to obtain its parent.
			Logger parent = logger.getParent();
			if (parent != null) {
				// There is a parent
				result = parent.getName();
			} else {
				// logger must be the root Logger
				result = ""; //$NON-NLS-1$
			}
		}

		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setLoggerLevel(String loggerName, String levelName) {
		final Logger logger = LogManager.getLogManager().getLogger(loggerName);

		if (logger != null) {
			// The named Logger exists. Now attempt to set its level. The
			// below attempt to parse a Level from the supplied levelName
			// will throw an IllegalArgumentException if levelName is not
			// a valid level name.
			Level newLevel = Level.parse(levelName);
			logger.setLevel(newLevel);
		} else {
			// Named Logger does not exist.
			/*[MSG "K05E7", "Unable to find Logger with name {0}."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E7", loggerName)); //$NON-NLS-1$
		}
	}

}
