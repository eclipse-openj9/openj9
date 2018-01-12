/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

import java.util.logging.*;
import java.util.Date;
import java.text.SimpleDateFormat;

/**
 * This is a log formatter. See the <a href="http://java.sun.com/j2se/1.4.2/docs/guide/util/logging/overview.html">logging overview</a>
 * for more details. (There is also some useful info <a href="http://www.forward.com.au/javaProgramming/javaGuiTips/javaLogging.html">here</a>).
 * I created this class mainly because I find the SimpleFormatter style a bit clunky.
 * @hidden
 */

public final class LogFormatter extends Formatter {

    private static String nl = System.getProperty("line.separator");
    private static SimpleDateFormat format = new SimpleDateFormat("dd/MM/yy H:m:s");
    private static boolean doTimestamp = LogManager.getLogManager().getProperty("com.ibm.zebedee.timestamps") != null;

    /**
     * Format the given log record. This method only gets called if a message is actually going
     * to be output. The property com.ibm.zebedee.timestamps controls whether the time stamp is
     * included in the output.
     * @param record the log record to be formatted.
     * @return a formatted log record
     */
    public String format(LogRecord record) {
        StringBuffer buf = new StringBuffer();
        if (doTimestamp) {
            buf.append(format.format(new Date(record.getMillis())) + " ");
        }
        if (record.getSourceClassName() != null) {	
            String className = record.getSourceClassName();
            if (className != null) {
                int index = className.lastIndexOf('.');
                if (index != -1) {
                    // strip package name to save space on line
                    className = className.substring(index + 1);
                }
            }
            buf.append(className);
            if (record.getSourceMethodName() != null) {	
                buf.append(".");
                buf.append(record.getSourceMethodName());
            }
            buf.append(": ");
        }
	    buf.append(formatMessage(record) + nl);
        return buf.toString();
    }
}
