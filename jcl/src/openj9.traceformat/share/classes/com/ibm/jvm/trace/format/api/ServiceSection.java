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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.trace.format.api;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;

/**
 * Service section of a file header.
 * 
 * @author Tim Preece
 */
public class ServiceSection {
	TraceContext context;
	String serviceString;
	
	String textSummary;

	public ServiceSection(TraceContext context, ByteBuffer data) throws IllegalArgumentException {
		this.context = context;
		DataHeader dataHeader = new DataHeader(context, data, "UTSS");

		byte stringData[] = new byte[dataHeader.length - DataHeader.DATAHEADER_SIZE];
		data.get(stringData);
		for (int i = 0; i < stringData.length; i++) {
			if (stringData[i] == 0) {
				try {
					serviceString = new String(stringData, 0, i, "UTF-8");
				} catch (UnsupportedEncodingException e) {
					// UTF-8 will be supported
					serviceString = new String(stringData, 0, i);
				}
				break;
			}
		}

		if (context.debugStream != null) {
			context.debug(this, 1, summary());
		}
	}

	public String toString() {
		return "Service level";
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder(toString()+":"+System.getProperty("line.separator"));

			s.append("        "); /* indent the service level */
			s.append(serviceString).append(System.getProperty("line.separator"));

			textSummary = s.toString();
		}
		
		return textSummary;
	}
}
