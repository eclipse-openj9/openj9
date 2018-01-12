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

import com.ibm.dtfj.java.j9.JavaHeapRegion;

/**
 * @author jmdisher
 *
 */
public class NodeRegion extends NodeAbstract
{
	private JavaHeapRegion _region;
	
	public NodeRegion(NodeHeap heapNode, Attributes attributes)
	{
		//<region name="Segment Region" id="0x806d51c" objectAlignment="8" minimumObjectSize="16" >
		String name = attributes.getValue("name");
		//note that the start, end, objectAlignment, and minimumObjectSize strings were used in the old format but in the new format they are part of the region
		// XXX: think through how to handle both formats
		long id = _longFromString(attributes.getValue("id"));
		int objectAlignment = (int)_longFromString(attributes.getValue("objectAlignment"));
		int minimumObjectSize = (int)_longFromString(attributes.getValue("minimumObjectSize"));
		
		//XXX: note that these do not currently exist but will be added and we already need the underlying support for the old XML format
		long start = _longFromString(attributes.getValue("start"));
		long end = _longFromString(attributes.getValue("end"));
		
		_region = heapNode.createJavaHeapRegion(name, id, objectAlignment, minimumObjectSize, start, end);
		heapNode.addRegion(_region);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("objects")) {
			child = new NodeObjects(_region, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
