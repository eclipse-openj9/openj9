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
package com.ibm.jvm.format;

public class IncompleteTracePoint{
	private byte[] data;
	private long threadId;

	public static final int INVALID_TYPE = -1;
	public static final int START_TYPE = 1;
	public static final int END_TYPE = 2;
	
	private int type = INVALID_TYPE;
	private int bytesMissing = 0;
	
	public IncompleteTracePoint( byte[] data, int length, int type, long threadId ){
		this.type = type;
		this.threadId = threadId;		
		this.data = new byte[ length ];
		System.arraycopy(data, 0, this.data, 0,	length);
	}
	
	public int setBytesRequired(int bytes){
		this.bytesMissing = bytes;
		return bytes;
	}

	public int getBytesRequired(){
		return bytesMissing;
	}

	public byte[] getData(){
		return data;
	}

	public long getThreadID(){
		return threadId;
	}
	
	public int getType(){
		return type;
	}
}
