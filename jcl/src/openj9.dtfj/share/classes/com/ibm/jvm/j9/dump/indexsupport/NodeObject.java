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

import com.ibm.dtfj.java.j9.JavaStackFrame;
import com.ibm.dtfj.java.j9.JavaClass;

/**
 * @author jmdisher
 *
 */
public class NodeObject extends NodeAbstract
{
	private JavaStackFrame _frame = null;

	public NodeObject(JavaClass theClass, Attributes attributes)
	{
		// objects show up under the constantpool
		// <object id="0x421188" address="0x109ae190"/>
		// here, "id" refers to <unknown> while "address" refers to the location in the constant pool where the reference occurs
		long id = _longFromString(attributes.getValue("id"));
		
		theClass.createConstantPoolObjectRef(id);
	}

	public NodeObject(JavaStackFrame frame, Attributes attributes) {
		// Construct a NodeObject for an object in a stack frame.
		_frame = frame;
		long id = _longFromString(attributes.getValue("id"));
		_frame.createObjectRef(id);
	}
}
