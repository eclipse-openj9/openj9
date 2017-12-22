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

import java.util.Vector;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Exports;
import com.ibm.uma.om.Flag;
import com.ibm.uma.om.Flags;
import com.ibm.uma.om.Module;
import com.ibm.uma.om.Objects;


public class ModuleParser {
	
	static public Module parse(Document moduleXml, String modulePath) throws UMAException {
		Module module = new Module(modulePath);

		// Initialize the Module based on the XML document
		Element elem = moduleXml.getDocumentElement();
		NodeList nodeList = elem.getChildNodes();
		for (int i = 0; i < nodeList.getLength(); i++) {
			Node node = nodeList.item(i);
			String nodeName = node.getLocalName();
			if ( nodeName == null ) continue;
			if ( nodeName.equalsIgnoreCase("artifact")) {
				Artifact artifact = ArtifactParser.parse(node, module);
				module.addArtifact(artifact);
				UMA.getUma().addArtifact(artifact);
			} else if ( nodeName.equalsIgnoreCase("exports")) {
				Exports exps = ExportsParser.parse(node, module.getContainingFile());
				module.addExports(exps.getGroup(), exps);
			} else if ( nodeName.equalsIgnoreCase("objects")) {
				Objects objs = ObjectsParser.parse(node, module.getContainingFile());
				module.addObjects(objs.getGroup(), objs);
			} else if ( nodeName.equalsIgnoreCase("exportlists")) {
				Vector<Exports> exports = ExportlistsParser.parse(node, module.getContainingFile());
				for (Exports exps : exports) {
					module.addExports(exps.getGroup(), exps);
				}
			} else if ( nodeName.equalsIgnoreCase("objectlists")) {
				Vector<Objects> objects = ObjectlistsParser.parse(node, module.getContainingFile());
				for ( Objects objs : objects ) {
					module.addObjects(objs.getGroup(), objs);
				}
			} else if ( nodeName.equalsIgnoreCase("flaglists")) {
				Vector<Flags> flags = FlaglistsParser.parse(node, module.getContainingFile());
				for ( Flags f : flags ) {
					//System.out.println("Flag: " + (f == null ? "null" : f.getGroup()));
					module.addFlags(f.getGroup(), f);
				}
			} else if ( nodeName.equalsIgnoreCase("flags")) {
				Flags flags = FlagsParser.parse(node, module.getContainingFile());
				//System.out.println("GROUP: " + flags.getGroup());
				for (Flag f : flags.getFlags()) {
					/*
					if (f != null) {
						System.out.println("FLAG: "+ f.getName());
					} else {
						System.out.println("NULL FLAG");
					}
					*/
				}
				module.addFlags(flags.getGroup(), flags);
			}
		}
		Parser.populatePredicateList(nodeList, module);
		return module;
	}
	
}
	
	

