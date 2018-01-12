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
import com.ibm.dtfj.java.j9.JavaArrayClass;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 *
 */
public class NodeArrayClass extends NodeAbstract
{
	private JavaArrayClass _theClass;
	
	public NodeArrayClass(JavaRuntime runtime, Attributes attributes)
	{
		//<arrayclass id="0x10a58068" leaf="0x10a57f78" arity="1" modifiers="0x80010411" firstElementOffset="16" sizeOffset="12" sizeBytes="4" sizeInElements="true" hashcodeSlot="4">
		long id = _longFromString(attributes.getValue("id"));
		long objectID = _longFromString(attributes.getValue("object"));
		long leafClass = _longFromString(attributes.getValue("leaf"));
		int arity = Integer.parseInt(attributes.getValue("arity"));
		int modifiers = (int)_longFromString(attributes.getValue("modifiers"));
		int firstElementOffset = Integer.parseInt(attributes.getValue("firstElementOffset"));
		int sizeOffset = Integer.parseInt(attributes.getValue("sizeOffset"));
		//note that sizeOffset is the offset into object header where the number of elements in the instance is stored
		int sizeBytes = Integer.parseInt(attributes.getValue("sizeBytes"));
		//note that sizeBytes is the number of bytes we are using to describe the size.  Currently, that number is always 4
		long loaderID = _longFromString(attributes.getValue("loader"));
		int flagOffset = (int)_longFromString(attributes.getValue("flagOffset"), runtime.bytesPerPointer());
		//NOTE: monitor data is currently never read directly from the object header but this information is provided in case we need to
		//CMVC 181215 : -ve numbers can now be used as special values for the monitorOffset, so this must be taken into account if the calculations below are used
//		int monitorOffset = (int)_longFromString(attributes.getValue("monitorOffset"), 2 * runtime.bytesPerPointer());
//		int monitorSize = (int)_longFromString(attributes.getValue("monitorSize"), runtime.bytesPerPointer());

		// JVMs built with J9VM_OPT_NEW_OBJECT_HASH provide an extra class attribute for the hashcode slot size
		String hashcodeSlotString = attributes.getValue("hashcodeSlot");
		int hashcodeSlot;
		if (hashcodeSlotString == null) {
			hashcodeSlot = 0;
		} else {
			hashcodeSlot = Integer.parseInt(hashcodeSlotString);
		}

		ImagePointer classPointer = runtime.pointerInAddressSpace(id);
		ImagePointer objectPointer = (objectID == 0 ? null : runtime.pointerInAddressSpace(objectID));

		_theClass = new JavaArrayClass(runtime, classPointer, modifiers, flagOffset, sizeOffset, sizeBytes, firstElementOffset, leafClass, arity, loaderID, objectPointer, hashcodeSlot);
		runtime.addClass(_theClass);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("interface")) {
			child = new NodeInterface(_theClass, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
