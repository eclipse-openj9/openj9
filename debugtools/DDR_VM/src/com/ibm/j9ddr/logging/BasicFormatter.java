/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.logging;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.MessageFormat;
import java.util.logging.Formatter;
import java.util.logging.LogRecord;

/**
 * Simple java.util.logging formatter than minimises the amount of java.util.logging meta-data
 * that is printed.
 * 
 * @author andhall
 *
 */
public class BasicFormatter extends Formatter
{
	
	/* (non-Javadoc)
	 * @see java.util.logging.Formatter#format(java.util.logging.LogRecord)
	 */
	@Override
	public String format(LogRecord record)
	{
		MessageFormat format = new MessageFormat(record.getMessage());
		
		StringWriter writer = new StringWriter();
		PrintWriter pw = new PrintWriter(writer);
		
		String sourceName = record.getSourceClassName();
		String methodName = record.getSourceMethodName();
		if (sourceName != null && methodName != null) {
			pw.print(sourceName);
			pw.print(".");
			pw.print(methodName);
			pw.print(": ");
		} else if (sourceName != null) {
			pw.print(sourceName);
			pw.print(": ");
		} else if (methodName != null) {
			pw.print(methodName);
			pw.print(": ");
		}
		
		format.format(record.getParameters(),writer.getBuffer(),null);
		
		Throwable thrown = record.getThrown();
		
		if (thrown != null) {
			pw.println(". Throwable stack:");
			thrown.printStackTrace(pw);
		}
		
		pw.println();

		pw.flush();
		
		return writer.toString();
	}

}
