/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.util;


public class Logger {
	static public final int ErrorLog = 1;
	static public final int InformationL1Log = 2;
	static public final int WarningLog = 3;
	static public final int InformationL2Log = 4;
	
	static Logger logger = null;
	
	static synchronized public Logger initLogger(int level) {
		if ( logger == null ) {
			logger = new Logger(level);
		}
		return logger;
	}
	
	static public Logger getLogger() {
		return logger;
	}
	
	int level;
	
	public Logger(int level) {
		this.level = level;
	}
	
	public void println(int level, String log) {
		if ( level <= this.level ) {
			if ( level == ErrorLog ) {
				System.err.println(log);
			} else {
				System.out.println(log);
			}
		}
	}
}
