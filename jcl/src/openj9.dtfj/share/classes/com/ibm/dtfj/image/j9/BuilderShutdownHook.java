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

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;

import com.ibm.dtfj.corereaders.ClosingFileReader;

public class BuilderShutdownHook extends Thread
{
	List _openFiles = null;
	
	public BuilderShutdownHook()
	{
		_openFiles = new Vector();
	}
	
	public void addFile(ClosingFileReader file)
	{
		_openFiles.add(new WeakReference(file));
	}

	/* (non-Javadoc)
	 * @see java.lang.Thread#run()
	 */
	public void run()
	{
		Iterator allFiles = _openFiles.iterator();
		while (allFiles.hasNext()) {
			ClosingFileReader reader = (ClosingFileReader) ((WeakReference)(allFiles.next())).get();
			if (null != reader) {
				try {
					reader.close();
				} catch (IOException e) {
					//we are shutting down so just ignore this
				}
			}
		}
	}
	
	
}
