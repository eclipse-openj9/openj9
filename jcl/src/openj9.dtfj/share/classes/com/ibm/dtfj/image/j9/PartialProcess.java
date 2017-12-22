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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;

/**
 * @author jmdisher
 *
 */
public class PartialProcess extends ImageProcess
{
	private DataUnavailable _executableException;
	private DataUnavailable _libraryException;

	/**
	 * @param pid
	 * @param commandLine
	 * @param environment
	 * @param currentThread
	 * @param threads
	 * @param executable
	 * @param libraries
	 * @param pointerSize
	 * @param executableException
	 * @param libraryException
	 */
	public PartialProcess(String pid, String commandLine,
			Properties environment, ImageThread currentThread,
			Iterator threads, ImageModule executable, Iterator libraries,
			int pointerSize, DataUnavailable executableException, DataUnavailable libraryException)
	{
		super(pid, commandLine, environment, currentThread, threads,
				executable, libraries, pointerSize);
		_executableException = executableException;
		_libraryException = libraryException;
	}

	public ImageModule getExecutable() throws DataUnavailable, CorruptDataException
	{
		if (null == _executableException) {
			return super.getExecutable();
		} else {
			throw _executableException;
		}
	}

	public Iterator getLibraries() throws DataUnavailable, CorruptDataException
	{
		if (null == _libraryException) {
			return super.getLibraries();
		} else {
			throw _libraryException;
		}
	}

}
