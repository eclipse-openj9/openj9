/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.image;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.DTFJCorruptDataException;

public abstract class J9DDRBaseImageThread implements ImageThread
{
	protected final IProcess process;
	
	protected J9DDRBaseImageThread(IProcess process)
	{
		this.process = process;
	}

	public final String getID() throws CorruptDataException
	{
		try {
			//This is slightly contrived code to make the getID() behave like DTFJ/jextract
			switch (process.getPlatform()) {
			case ZOS:
				return "0x" + Long.toHexString(getThreadId());
			default:
				return Long.toString(getThreadId());
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process,e);
		} 
	}

	public abstract long getThreadId() throws com.ibm.j9ddr.CorruptDataException;
}
