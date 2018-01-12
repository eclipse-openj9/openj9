/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.util.Vector;

import com.ibm.jvm.dtfjview.spi.IOutputManager;
import com.ibm.jvm.dtfjview.spi.IOutputChannel;

@SuppressWarnings({"rawtypes","unchecked"})
public class Output extends OutputStream implements IOutputManager {
	private ByteArrayOutputStream bos = new ByteArrayOutputStream();		//used to internally buffer data
	private PrintStream internalStream = new PrintStream(bos);
	private boolean isBufferingEnabled = false;								//default of no buffering
	
	Vector outputChannels;
	long lastTimestamp = 0;
	String lastPrompt = "";
	String lastInput = "";

	public Output(){
		outputChannels = new Vector();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#clearBuffer()
	 */
	public void clearBuffer() {
		bos.reset();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#setBuffering(boolean)
	 */
	public void setBuffering(boolean enabled) {
		isBufferingEnabled = enabled;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#getBuffer()
	 */
	public String getBuffer() {
		return bos.toString();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#print(java.lang.String)
	 */
	public void print(String outputString){
		if(isBufferingEnabled) {
			internalStream.print(outputString);
		} else {
			for (int i = 0; i < outputChannels.size(); i++){
				((IOutputChannel)outputChannels.elementAt(i)).print(outputString);
			}
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#printPrompt(java.lang.String)
	 */
	public void printPrompt(String prompt){
		if(isBufferingEnabled) {
			internalStream.print(prompt);
		} else {
			for (int i = 0; i < outputChannels.size(); i++){
				((IOutputChannel)outputChannels.elementAt(i)).printPrompt(prompt);
			}
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#println(java.lang.String)
	 */
	public void println(String outputString){
		if(isBufferingEnabled) {
			internalStream.println(outputString);
		} else {
			for (int i = 0; i < outputChannels.size(); i++){
				((IOutputChannel)outputChannels.elementAt(i)).println(outputString);
			}
		}
	}
	
		
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#addChannel(com.ibm.jvm.dtfjview.spi.OutputChannel)
	 */
	public void addChannel(IOutputChannel channel){
		addChannel(channel, false);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#addChannel(com.ibm.jvm.dtfjview.spi.OutputChannel, boolean)
	 */
	public void addChannel(IOutputChannel channel, boolean printLastInput){
		outputChannels.add(channel);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#removeChannel(java.lang.Class)
	 */
	public void removeChannel(Class<?> type) {
		for (int i = 0; i < outputChannels.size(); i++){
			IOutputChannel channel = (IOutputChannel)outputChannels.elementAt(i);
			if (channel.getClass().isAssignableFrom(type))
			{
				((IOutputChannel)outputChannels.elementAt(i)).close();
				outputChannels.removeElementAt(i);
			}
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#removeChannel(com.ibm.jvm.dtfjview.spi.IOutputChannel)
	 */
	public void removeChannel(IOutputChannel channel) {
		outputChannels.remove(channel);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#removeAllChannels()
	 */
	public void removeAllChannels() {
		outputChannels.clear();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#removeFileChannel()
	 */
	public void removeFileChannel(){
		for (int i = 0; i < outputChannels.size(); i++){
			if ((IOutputChannel)outputChannels.elementAt(i) instanceof FileOutputChannel)
			{
				((IOutputChannel)outputChannels.elementAt(i)).close();
				outputChannels.removeElementAt(i);
			}
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#setConsoleNoPrint(boolean)
	 */
	public void setConsoleNoPrint(boolean noPrint){
		for (int i = 0; i < outputChannels.size(); i++){
			if ((IOutputChannel)outputChannels.elementAt(i) instanceof ConsoleOutputChannel)
			{
				((ConsoleOutputChannel)outputChannels.elementAt(i)).setNoPrint(noPrint);
			}
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#close()
	 */
	public void close() {
		for (int i = 0; i < outputChannels.size(); i++){
			((IOutputChannel)outputChannels.elementAt(i)).close();
		}
	}

	/* (non-Javadoc)
	 * @see java.io.OutputStream#write(byte[], int, int)
	 */
	public void write(byte[] b, int off, int len) throws IOException {
		String msg = new String(b, off, len);
		print(msg);
	}

	/* (non-Javadoc)
	 * @see java.io.OutputStream#write(byte[])
	 */
	public void write(byte[] b) throws IOException {
		String msg = new String(b);
		print(msg);
	}

	/* (non-Javadoc)
	 * @see java.io.OutputStream#write(int)
	 */
	public void write(int b) throws IOException {
		char[] chars = new char[]{ (char)b };
		String msg = new String (chars);
		print(msg);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#flush()
	 */
	public void flush() {
		//this has no equivalent
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.IOutput#getPrintStream()
	 */
	public PrintStream getPrintStream() {
		PrintStream stream = new JdmpviewPrintStream(this);
		return stream;
	}

}
