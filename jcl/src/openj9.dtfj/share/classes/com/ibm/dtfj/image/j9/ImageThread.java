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
package com.ibm.dtfj.image.j9;

import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

public class ImageThread implements com.ibm.dtfj.image.ImageThread
{
	private String _systemThreadID;
	private Properties _properties;
	private Vector _registers = new Vector();
	private Vector _stackSections = new Vector();
	private Vector _stackFrames = new Vector();
	/**
	 * If the thread is currently processing or has a pending signal, it will be non-zero
	 */
	private int _signalNumber;
	
	public ImageThread(String threadID, Iterator registers, Iterator stackSections, Iterator stackFrames, Properties properties, int signalNumber)
	{
		_systemThreadID = threadID;
		while (registers.hasNext()) {
			_registers.add(registers.next());
		}
		while (stackSections.hasNext()) {
			_stackSections.add(stackSections.next());
		}
		while (stackFrames.hasNext()) {
			_stackFrames.add(stackFrames.next());
		}
		_signalNumber = signalNumber;
		_properties = properties;
	}
	
	public String getID() throws CorruptDataException
	{
		return _systemThreadID;
	}

	public Iterator getStackFrames() throws DataUnavailable
	{
		if (_stackFrames.isEmpty()) {
			throw new DataUnavailable("no stack frames");
		}
		return _stackFrames.iterator();
	}

	public Iterator getStackSections()
	{
		return _stackSections.iterator();
	}

	public Iterator getRegisters()
	{
		return _registers.iterator();
	}

	public Properties getProperties()
	{
		return _properties;
	}

	/**
	 * Called by the process if this is the current thread to see what signal it is stopped on
	 * @return The signal number that was pending for this thread
	 */
	public int getSignal()
	{
		return _signalNumber;
	}
}
