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
package com.ibm.jvm.dtfjview.spi;

/**
 * An output channel provides a mechanism by which text will be displayed
 * to the user.
 * 
 * @author adam
 *
 */
public interface IOutputChannel {

	/**
	 * Sends text to channel
	 * 
	 * @param outputString text to send
	 */
	public void print(String outputString);

	/**
	 * Sets the prompt which should be displayed when an interactive session
	 * is running. Typically this will display the context number in a
	 * multi-context environment.
	 * 
	 * @param prompt prompt to display.
	 */
	public void printPrompt(String prompt);

	/**
	 * Sends text to the channel with an appended \n
	 * 
	 * @param outputString text to send
	 */
	public void println(String outputString);
	
	/**
	 * Instructs this channel to close and free any resources. Once closed a channel
	 * cannot be re-opened.
	 */
	public void close();

	/**
	 * Causes the channel to flush any buffered text. Depending upon the channel 
	 * implementation this may not have any effect.
	 */
	public void flush();
}
