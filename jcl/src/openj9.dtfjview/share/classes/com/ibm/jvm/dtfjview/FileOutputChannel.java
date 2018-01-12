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

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Date;

import com.ibm.jvm.dtfjview.commands.helpers.Utils;
import com.ibm.jvm.dtfjview.spi.IOutputChannel;

public class FileOutputChannel implements IOutputChannel {
	private File file; 				//the file being written to
	private FileWriter fw;			//the writer for the file

	// f must not be null
	public FileOutputChannel(FileWriter f, File file)
	{
		fw = f;
		this.file = file;		//have to have the file explicitly passed as it cannot be retrieved from the FileWriter class
	}

	public void print(String outputString) {
		try {
			fw.write(Utils.toString(outputString));
			fw.flush();
		} catch (IOException e) {
			
		}
	}

	public void printPrompt(String prompt) {
		// do nothing; we'll output the prompt when we get a printInput() call
	}

	public void println(String outputString) {
		try {
			fw.write(Utils.toString(outputString) + "\n");
			fw.flush();
		} catch (IOException e) {
			
		}
	}

	public void error(String outputString) {
		try {
			fw.write("\n");
			fw.write("\tERROR: " + Utils.toString(outputString) + "\n");
			fw.write("\n");
			fw.flush();
		} catch (IOException e) {
			
		}
	}
	
	//logs an error to the specified output channel
	public void error(String msg, Exception e) {
		try {
			fw.write("\n");
			fw.write("\tERROR: " + Utils.toString(msg) + "\n");
			fw.write("\n");
			PrintWriter writer = new PrintWriter(fw, true);
			e.printStackTrace(writer);
			fw.flush();
		} catch (IOException ioe) {
			
		}
	}

	public void printInput(long timestamp, String prompt, String outputString) {
		try {
			fw.write((new Date(timestamp)).toString() + " " +
					prompt + Utils.toString(outputString) + "\n");
			fw.flush();
		} catch (IOException e) {
			
		}
	}
	
	public void close() {
		try {
			fw.close();
		} catch (IOException e) {
			
		}
	}

	public void flush() {
		try {
			fw.flush();
		} catch (IOException e) {
			
		}
	}

	//file channels are considered equal if they are writing to the same file, test for hash and equals based on the file.
	@Override
	public boolean equals(Object o) {
		if((o == null) || !(o instanceof FileOutputChannel)) {
			return false;
		}
		return file.equals(((FileOutputChannel) o).getFile());
	}

	private Object getFile() {
		return file;
	}

	@Override
	public int hashCode() {
		return file.hashCode();
	}
	
	
}
