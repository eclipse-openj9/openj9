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

import com.ibm.uma.om.*;

public class FlagParser {
	static public Flag parse(Node item, String containingFile) {
		Flag returnFlag = null;
		if ( item.getNodeName().equalsIgnoreCase("group") ) {
			NamedNodeMap nodeMap = item.getAttributes();
			Node groupNameNode = nodeMap.getNamedItem("name");
			String groupName = null;
			if ( groupNameNode != null ) {
				groupName = groupNameNode.getNodeValue();
			}
			//System.out.println("Adding flag group: " + groupName + " in containingFile" + containingFile);
			returnFlag = new Flag(groupName, containingFile);
		} else if ( item.getNodeName().equalsIgnoreCase("flag") ) {
			NamedNodeMap nodeMap = item.getAttributes();
			Node flagNameNode = nodeMap.getNamedItem("name");
			Node flagValueNode = nodeMap.getNamedItem("value");
			String value = null;
			if ( flagValueNode != null ) {
				value = flagValueNode.getNodeValue();
			}
			Node flagCFlagNode = nodeMap.getNamedItem("cflag");
			Node flagCXXFlagNode = nodeMap.getNamedItem("cxxflag");
			Node flagCPPFlagNode = nodeMap.getNamedItem("cppflag");
			Node flagASMFlagNode = nodeMap.getNamedItem("asmflag");
			Node flagDefinitionNode = nodeMap.getNamedItem("definition");
			boolean cflag = true;
			if ( flagCFlagNode.getNodeValue().equalsIgnoreCase("false") ) {
				cflag = false;
			}
			boolean cxxflag = true;
			if ( flagCXXFlagNode.getNodeValue().equalsIgnoreCase("false") ) {
				cxxflag = false;
			}
			boolean cppflag = true;
			if ( flagCPPFlagNode.getNodeValue().equalsIgnoreCase("false") ) {
				cppflag = false;
			}
			boolean asmflag = true;
			if ( flagASMFlagNode.getNodeValue().equalsIgnoreCase("false") ) {
				asmflag = false;
			}
			boolean definition = true;
			if ( flagDefinitionNode.getNodeValue().equalsIgnoreCase("false") ) {
				definition = false;
			}
			returnFlag = new Flag(flagNameNode.getNodeValue(), value, containingFile, cflag, cxxflag, cppflag, asmflag, definition );
		}
		Parser.populatePredicateList(item.getChildNodes(), returnFlag);
		return returnFlag;
	}
}
