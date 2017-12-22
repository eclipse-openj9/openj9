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

import com.ibm.dtfj.java.j9.JavaClassLoader;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 *
 */
public class NodeClassLoader extends NodeAbstract
{
	private JavaClassLoader _classLoader;
	
	public NodeClassLoader(JavaRuntime runtime, Attributes attributes)
	{
		//<classloader id="0x13e5e4" obj="0x482058">
		//parent will be javavm
		long id = _longFromString(attributes.getValue("id"));
		long objectID = _longFromString(attributes.getValue("obj"));
		
		_classLoader = new JavaClassLoader(runtime, runtime.pointerInAddressSpace(id), runtime.pointerInAddressSpace(objectID));
		runtime.addClassLoader(_classLoader);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("library")) {
			child = new NodeLibrary(_classLoader, attributes);
		} else if (qName.equals("class")) {
			child = new NodeClassInClassLoader(_classLoader, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
