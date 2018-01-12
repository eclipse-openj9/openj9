/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;

import com.ibm.jvm.dtfjview.spi.IOutputChannel;

/**
 * Note, this class needs to be initialized first before it can be used.
 * <p>
 * @author Manqing Li, IBM
 *
 */
public class ToolsRegistryOutputChannels extends OutputStream {
	
	/**
	 * To initialize the output channels for the tools registry.
	 */
	public static void initialize(String charsetName) {
		if(instance == null) {
			instance = new ToolsRegistryOutputChannels(charsetName);
		}
	}
	
	/**
	 * To add an output channel.
	 * <p>
	 * @param out	The output channel to be added.
	 */
	public static void addChannel(IOutputChannel out) {
		instance.channels.add(out);
	}
	
	/**
	 * To remove an output channel.
	 * <p>
	 * @param out	The output channel to be removed.
	 */
	public static void removeChannel(IOutputChannel out) {
		if(instance.channels.contains(out)) {
			instance.channels.remove(out);
		}
	}
	
	/**
	 * To check if an output channel is already contained.
	 * <p>
	 * @param out	The output channel to be checked.
	 * <p>
	 * @return	<code>true</code> if such an output channel is found; <code>false</code> otherwise.
	 */
	public static boolean contains(IOutputChannel out) {
		for(IOutputChannel channel : instance.channels) {
			if(channel.equals(out)) {
				return true;
			}
		}
		return false;
	}
	
	public void write(int b) throws IOException {
		buffer.add(Integer.valueOf(b).byteValue());		
		if ('\n' == b || 21 == b ) { // EBCDIC systems use NL (New Line 0x15).
			writeBuffer();
		}
	}
	
	public void close() throws IOException {
		writeBuffer();
		for(IOutputChannel channel : channels) {
			channel.close();
		}
	}
	public void flush() throws IOException {
		writeBuffer();
		for(IOutputChannel channel : channels) {
			channel.flush();
		}

	}
	public static PrintStream newPrintStream() {
		return new PrintStream(instance, true);
	}

	private ToolsRegistryOutputChannels(String charsetName) {
		this.charsetName = charsetName;
		channels = new ArrayList<IOutputChannel>();
		buffer = new ArrayList<Byte>();
	}

	private void writeBuffer() throws IOException {
		if (0 == buffer.size()) {
			return;
		}
		byte [] byteArray = new byte[buffer.size()];
		for (int i = 0; i < byteArray.length; i++) {
			byteArray[i] = buffer.get(i);
		}

		for(IOutputChannel out : channels) {
			if(charsetName == null) {
				out.print(new String(byteArray));
			} else {
				out.print(new String(byteArray, charsetName));
			}
		}
	
		buffer = new ArrayList<Byte>();
	}
	private List<IOutputChannel> channels;
	private List<Byte> buffer;
	private String charsetName;
	
	private static ToolsRegistryOutputChannels instance = null;
}
