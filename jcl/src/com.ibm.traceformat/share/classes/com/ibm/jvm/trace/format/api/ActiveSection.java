/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.trace.format.api;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.util.Vector;

/**
 * Active section of a file header.
 * 
 * @author Tim Preece
 */
public class ActiveSection {
	TraceContext context;
	String textSummary;
	
	private Vector options = new Vector();

	public ActiveSection(TraceContext context, ByteBuffer data) throws IllegalArgumentException {
		this.context = context;
		DataHeader dataHeader = new DataHeader(context, data, "UTTA");

		if (context.debugStream != null) {
			context.debug(this, 1, dataHeader);
		}
		
		byte activationData[] = new byte[dataHeader.length - DataHeader.DATAHEADER_SIZE];
		data.get(activationData);
		splitActivationData(activationData);

	}

	private void splitActivationData(byte data[]) {
		for (int i = 0, j = 0; i < data.length; i++) {
			/* check for null termination of string */
			if (data[i] == 0) {
				/* if we're not seeing multiple nulls */
				if (i != j) {
					String s;
					try {
						s = new String(data, j, i - j, "UTF-8");
					} catch (UnsupportedEncodingException e) {
						// UTF-8 will be supported
						s = new String(data, j, i - j);
					}
					options.add(s);
				}
				j = i + 1;
			}
		}
		
		if (context.debugStream != null) {
			context.debug(this, 1, summary());
		}
	}

	public String toString() {
		return "Trace activation information:";
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder(toString()+":"+System.getProperty("line.separator"));
	
			for (int i = 0; i < options.size(); i++) {
				s.append("        "); /* indent the options */
				s.append(options.get(i)).append(System.getProperty("line.separator"));
			}
			
			textSummary = s.toString();
		}
		
		return textSummary;
	}
}
