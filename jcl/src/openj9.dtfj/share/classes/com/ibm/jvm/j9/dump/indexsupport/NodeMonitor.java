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
package com.ibm.jvm.j9.dump.indexsupport;

import org.xml.sax.Attributes;

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.j9.JavaMonitor;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 *
 */
public class NodeMonitor extends NodeAbstract
{
	public NodeMonitor(JavaRuntime runtime, Attributes attributes)
	{
		//<monitor id="0x35f78" name="VM GC finalize run finalization"/>
		// note that a monitor can have an "object" field, as well, which is the pointer to the object which has this monitor as its monitor
		String name = attributes.getValue("name");
		long id = _longFromString(attributes.getValue("id"));
		long objectID = _longFromString(attributes.getValue("object"));
		long threadID = _longFromString(attributes.getValue("owner"));
		
		ImagePointer encompassingObjectAddress = null;
		if (0 != objectID) {
			encompassingObjectAddress = runtime.pointerInAddressSpace(objectID);
		}
		JavaMonitor monitor = new JavaMonitor(runtime, runtime.pointerInAddressSpace(id), name, encompassingObjectAddress, threadID);
		runtime.addMonitor(monitor);
	}
}
