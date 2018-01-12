/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaVMInitArgs;

/**
 * A javacore-based implementation of DTFJ JavaVMInitArgs. Note that javacores
 * contain the set of initialization options passed into the VM (in the ENVINFO
 * section of the javacore), but do not provide the JNI level or the setting of
 * the JNI 'ignoreUnrecognized' flag.
 * 
 * @see com.ibm.dtfj.java.JavaRuntime
 */
public class JCJavaVMInitArgs implements JavaVMInitArgs {
	
	private Vector fOptions = new Vector();
	private JCJavaRuntime fRuntime;
		
	public JCJavaVMInitArgs(JCJavaRuntime javaRuntime, int version, boolean ignoreUnrecognized) 
				throws JCInvalidArgumentsException 	{
		fRuntime = javaRuntime;
		fRuntime.addJavaVMInitArgs(this);
	}
	
	public int getVersion() throws DataUnavailable, CorruptDataException {
		// JNI version not available in javacore
		throw new DataUnavailable("JNI version not available");
	}

	public boolean getIgnoreUnrecognized() throws DataUnavailable, CorruptDataException {
		// JNI version not available in javacore
		throw new DataUnavailable("JNI ignoreUnrecognized field not available");
	}

	public Iterator getOptions() throws DataUnavailable {
		return fOptions.iterator();
	}
	
	/**
	 * Not in DTFJ. Used only for building purposes.
	 * 
	 */
	public void addOption(JCJavaVMOption option) throws JCInvalidArgumentsException {
		if (option == null) {
			throw new JCInvalidArgumentsException("Must pass a valid JavaVMOption");
		}
		fOptions.add(option);
	}

}
