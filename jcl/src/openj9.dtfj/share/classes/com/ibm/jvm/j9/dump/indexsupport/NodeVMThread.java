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

import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.j9.JavaRuntime;
import com.ibm.dtfj.java.j9.JavaThread;

/**
 * @author jmdisher
 *
 */
public class NodeVMThread extends NodeAbstract
{
	private JavaThread _javaThread;
	
	public NodeVMThread(JavaRuntime runtime, Attributes attributes)
	{
		//<vmthread id="0x123f00" obj="0x420500" state="Blocked" monitor="0x35f20" native="0xdac">
		long id = _longFromString(attributes.getValue("id"));
		long objectID = _longFromString(attributes.getValue("obj"));
		String state = attributes.getValue("state");
		long monitorID = _longFromString(attributes.getValue("monitor"));
		long nativeID = _longFromString(attributes.getValue("native"));
		
		ImageThread imageThread = runtime.nativeThreadForID(nativeID);
		
		_javaThread = new JavaThread(runtime, runtime.pointerInAddressSpace(id), runtime.pointerInAddressSpace(objectID), state, imageThread);
		long blockedID = 0;
		long waitingID = 0;
		if (null != state) {
			//a null state is an error but we don't want to completely fail, here, since we might still have some useful data
			blockedID = state.equals("Blocked") ? monitorID : 0;
			waitingID = state.equals("Waiting") ? monitorID : 0;
		}
		runtime.addThread(_javaThread, blockedID, waitingID);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("stack")) {
			child = new NodeStack(_javaThread, attributes);
		} else {
			//the vmthread knows that an error tag here means a corrupt (or otherwise unreadable) stack but we still need to return the error token
			if (qName.equals("error")) {
				_javaThread.setStackCorrupt();
			}
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
