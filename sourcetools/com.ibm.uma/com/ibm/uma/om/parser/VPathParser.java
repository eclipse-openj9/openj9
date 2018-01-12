/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.om.parser;

import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import com.ibm.uma.om.VPath;

public class VPathParser {
	static public VPath parse(Node node, String containingFile) {
		NodeList nodeList = node.getChildNodes();
		NamedNodeMap attributes = node.getAttributes();
		
		node = attributes.getNamedItem("path");
		String path = null;
		if ( node != null ) {
			path = node.getNodeValue();
		}

		node = attributes.getNamedItem("pattern");
		String file = null;
		if ( node != null ) {
			file = node.getNodeValue();
		}
		
		node = attributes.getNamedItem("augmentObjects");
		boolean augmentObjects = false;
		if ( node != null && node.getNodeValue().equalsIgnoreCase("true") ) {
			augmentObjects = true;
		}
		
		node = attributes.getNamedItem("augmentIncludes");
		boolean augmentIncludes = false;
		if ( node != null && node.getNodeValue().equalsIgnoreCase("true") ) {
			augmentIncludes = true;
		}
		
		node = attributes.getNamedItem("type");
		String type = null;
		if ( node != null ) {
			type = node.getNodeValue();
		}
		
		VPath vpath = new VPath(containingFile);
		vpath.setPath(path);
		vpath.setFile(file);
		vpath.setType(type);
		vpath.setAugmentObjects(augmentObjects);
		vpath.setAugmentIncludes(augmentIncludes);

		Parser.populatePredicateList(nodeList, vpath);
		return vpath;
	}


}
