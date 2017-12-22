/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.logging;

import java.io.File;
import java.io.FileInputStream;
import java.util.HashSet;
import java.util.Properties;
import java.util.logging.Filter;
import java.util.logging.LogRecord;

public class LogFilter implements Filter {
	private static final String SYSTEM_PROP_LOGGING = "java.util.logging.config.file";
	private static final String KEY_CATEGORIES = "com.ibm.j9ddr.tools.logging.categories";
	private HashSet<String> idlist = null;		//the list of id's with which to filter log records

	public LogFilter() {
		String logFile = System.getProperty(SYSTEM_PROP_LOGGING);
		File file = new File(logFile);
		if(file.exists()) {
			Properties props = new Properties();
			try {
				props.load(new FileInputStream(file));
			} catch (Exception e) {
				e.printStackTrace();
				throw new IllegalArgumentException("The configuration file " + file.getPath() + " specified in " + SYSTEM_PROP_LOGGING + " could not be read");
			}
			if(props.containsKey(KEY_CATEGORIES)) {
				String[] categories = props.getProperty(KEY_CATEGORIES).trim().split(",");
				if(categories.length == 0) return;			//abort as there are no categories defined
				idlist = new HashSet<String>();
				for(int i = 0; i < categories.length; i++) {
					idlist.add(categories[i].trim());
				}
			}
		}
	}
	
	public boolean isLoggable(LogRecord record) {
		return ((idlist == null) || (idlist.contains(record.getLoggerName())));
	}

}
