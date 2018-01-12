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
package com.ibm.jvm.dtfjview.spi;

import java.io.PrintStream;


/**
 * The output manager is responsible for managing a collection of registered
 * output channels. It provides methods to register and remove channels as
 * well as control buffering.  When text is written to the manager then it
 * is sent to all registered output channels.
 * 
 * This interface is an extension of IOutputChannel so as to enforce consistency
 * between the declarations for printing text to an output channel. The manager 
 * is in effect acting as a single proxy channel for all of it's individually 
 * registered channels.
 * 
 * @author adam
 *
 */
public interface IOutputManager extends IOutputChannel {

	/**
	 * Enabling buffering will cause the output from commands to be written
	 * to an internal buffer rather than immediately to the underlying output
	 * channels. This is typically used by clients to inspect or intercept the results from
	 * a command before carrying out further processing.
	 * 
	 * By default buffering is not enabled.
	 * 
	 * @param enabled true turns on buffering
	 */
	public void setBuffering(boolean enabled);
	
	/**
	 * Clears the current buffer contents.
	 */
	public void clearBuffer();

	/**
	 * Gets the current buffer contents.
	 * 
	 * @return buffer contents
	 */
	public String getBuffer();

	/**
	 * Adds a channel to the list of registered channels.
	 * 
	 * @param channel channel to add.
	 */
	public void addChannel(IOutputChannel channel);

	/**
	 * Removes all registered channels for a particular type.
	 * @param type
	 */
	public void removeChannel(Class<?> type);

	/**
	 * Removes a specific channel for a particular type. For this to be successful the underlying
	 * implementation must override hashcode and equals.
	 * @param type
	 */
	public void removeChannel(IOutputChannel channel);

	/**
	 * Remove all registered channels
	 */
	public void removeAllChannels();

	/**
	 * Creates a print stream for this output object
	 * @return
	 */
	public PrintStream getPrintStream();

}
