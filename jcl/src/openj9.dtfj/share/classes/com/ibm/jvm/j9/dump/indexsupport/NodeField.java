/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

/**
 * @author jmdisher
 *
 */
public class NodeField extends NodeAbstract
{
	public NodeField(JavaClass theClass, Attributes attributes)
	{
		//an instance field which is descendant of JavaClass in the XML
		//<field class="0x17be60" name="path" sig="Ljava/lang/String;" modifiers="0x20082" offset="16"/>
		long classID = _longFromString(attributes.getValue("class"));	//the ID of the class where the field is declared
		String name = attributes.getValue("name");
		String sig = attributes.getValue("sig");
		int modifiers = (int)_longFromString(attributes.getValue("modifiers"));
		int offset = Integer.parseInt(attributes.getValue("offset"));
		
		theClass.createNewField(name, sig, modifiers, offset, classID);
	}
}
