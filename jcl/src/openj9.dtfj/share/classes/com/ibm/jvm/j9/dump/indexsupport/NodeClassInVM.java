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

import com.ibm.dtfj.java.j9.JavaClass;
import com.ibm.dtfj.java.j9.JavaRuntime;
import com.ibm.dtfj.image.ImagePointer;

/**
 * @author jmdisher
 *
 */
public class NodeClassInVM extends NodeAbstract implements IParserNode
{
	private JavaClass _class;
	
	public NodeClassInVM(JavaRuntime runtime, Attributes attributes)
	{
		//create the full class instance
		//<class id="0x17e998" super="0x177370" name="java/lang/Runtime" instanceSize="12" loader="0x13e7cc" modifiers="0x21" hashcodeSlot="4" source="Runtime.java">
		long id = _longFromString(attributes.getValue("id"));
		long objectID = _longFromString(attributes.getValue("object"));
		long superId = _longFromString(attributes.getValue("super"));
		String name = attributes.getValue("name");
		int instanceSize = Integer.parseInt(attributes.getValue("instanceSize"));
		long loaderID = _longFromString(attributes.getValue("loader"));
		int modifiers = (int)_longFromString(attributes.getValue("modifiers"));
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
		String fileName = attributes.getValue("source");
		
		ImagePointer classPointer = runtime.pointerInAddressSpace(id);
		ImagePointer objectPointer = (objectID == 0 ? null : runtime.pointerInAddressSpace(objectID));

		_class = new JavaClass(runtime, classPointer, superId, name, instanceSize, loaderID, modifiers, flagOffset, fileName, objectPointer, hashcodeSlot);
		runtime.addClass(_class);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("field")) {
			child = new NodeField(_class, attributes);
		} else if (qName.equals("method")) {
			child = new NodeMethod(_class, attributes);
		} else if (qName.equals("constantpool")) {
			child = new NodeConstantPool(_class, attributes);
		} else if (qName.equals("interface")) {
			child = new NodeInterface(_class, attributes);
		} else if (qName.equals("static")) {
			child = new NodeStatic(_class, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
