/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2012
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.dtfjview.tools;

import java.io.ByteArrayOutputStream;
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
 */
public class ToolsRegistryOutputChannels extends OutputStream {

	private static ToolsRegistryOutputChannels instance;

	private final List<IOutputChannel> channels;
	private ByteArrayOutputStream buffer;
	private final String charsetName;

	/**
	 * To initialize the output channels for the tools registry.
	 */
	public static void initialize(String charsetName) {
		if (instance == null) {
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
		instance.channels.remove(out);
	}

	/**
	 * To check if an output channel is already contained.
	 * <p>
	 * @param out	The output channel to be checked.
	 * <p>
	 * @return	<code>true</code> if such an output channel is found; <code>false</code> otherwise.
	 */
	public static boolean contains(IOutputChannel out) {
		return instance.channels.contains(out);
	}

	@Override
	public void write(int b) throws IOException {
		buffer.write(b);
		if ('\n' == b || 0x15 == b) { // EBCDIC systems use NL (New Line 0x15).
			writeBuffer();
		}
	}

	@Override
	public void close() throws IOException {
		writeBuffer();
		for (IOutputChannel channel : channels) {
			channel.close();
		}
	}

	@Override
	public void flush() throws IOException {
		writeBuffer();
		for (IOutputChannel channel : channels) {
			channel.flush();
		}
	}

	public static PrintStream newPrintStream() {
		return new PrintStream(instance, true);
	}

	private ToolsRegistryOutputChannels(String charsetName) {
		this.charsetName = charsetName;
		this.channels = new ArrayList<>();
		this.buffer = new ByteArrayOutputStream();
	}

	private void writeBuffer() throws IOException {
		if (0 != buffer.size()) {
			String content = (charsetName == null) ? buffer.toString() : buffer.toString(charsetName);

			buffer = new ByteArrayOutputStream();

			for (IOutputChannel out : channels) {
				out.print(content);
			}
		}
	}

}
