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

import com.ibm.uma.UMA;
import com.ibm.uma.UMABadPhaseNameException;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Command;
import com.ibm.uma.om.Dependency;
import com.ibm.uma.om.Export;
import com.ibm.uma.om.Flag;
import com.ibm.uma.om.Include;
import com.ibm.uma.om.Library;
import com.ibm.uma.om.MakefileStub;
import com.ibm.uma.om.Module;
import com.ibm.uma.om.Object;
import com.ibm.uma.om.Option;
import com.ibm.uma.om.VPath;

public class ArtifactParser {
	static public Artifact parse(Node artifactNode, Module module) throws UMAException {
		Artifact artifact = new Artifact(module);
			
			NodeList nodeList = artifactNode.getChildNodes();
			for (int i = 0; i < nodeList.getLength(); i++) {
				Node node = nodeList.item(i);
				String nodeName = node.getLocalName();
				if ( nodeName == null ) continue;
				if ( nodeName.equalsIgnoreCase("description")) {
					NodeList descChildren = node.getChildNodes();
					Node descChild = descChildren.item(0);
					artifact.setDescription(descChild.getNodeValue());
				} else  if ( nodeName.equalsIgnoreCase("options") ) {
					NodeList optionsNodeList = node.getChildNodes();
					for ( int j=0; j<optionsNodeList.getLength(); j++ ) {
						Node optionItem = optionsNodeList.item(j);
						NamedNodeMap attributes = optionItem.getAttributes();
						if (attributes == null) continue;
						Option option = OptionParser.parse(optionItem, artifact.getContainingFile());
						artifact.addOption(option);
					}
				} else if ( nodeName.equalsIgnoreCase("phase")) {
					NodeList phaseChildren = node.getChildNodes();
					for (int j=0; j<phaseChildren.getLength(); j++) {
						Node phasesNode = phaseChildren.item(j);
						String phaselist = phasesNode.getNodeValue();
						String[] phases = phaselist.split("\\s");
						for ( int k=0; k<phases.length; k++ ) {
							try {
								artifact.setPhase(UMA.getUma().getConfiguration().phaseFromString(phases[k]), true);
							} catch (UMABadPhaseNameException e) {
								throw new UMAException("in file: " + artifact.getContainingFile(), e);
							}
						}
					}
				} else  if ( nodeName.equalsIgnoreCase("includes") ) {
					NodeList includesNodeList = node.getChildNodes();
					for ( int j=0; j<includesNodeList.getLength(); j++ ) {
						Node includeItem = includesNodeList.item(j);
						NamedNodeMap attributes = includeItem.getAttributes();
						if (attributes == null) continue;
						Include include = new Include(artifact.getContainingFile());
						include.setPath(attributes.getNamedItem("path").getNodeValue());
						include.setType(attributes.getNamedItem("type").getNodeValue());
						artifact.addInclude(include);
						Parser.populatePredicateList(includeItem.getChildNodes(), include);
					}
				} else  if ( nodeName.equalsIgnoreCase("commands") ) {
					NodeList commandsNodeList = node.getChildNodes();
					for ( int j=0; j<commandsNodeList.getLength(); j++ ) {
						Node commandItem = commandsNodeList.item(j);
						NamedNodeMap attributes = commandItem.getAttributes();
						if (attributes == null) continue;
						Command command = new Command(artifact.getContainingFile());
						command.setCommand(attributes.getNamedItem("line").getNodeValue());
						command.setType(attributes.getNamedItem("type").getNodeValue());
						artifact.addCommand(command);
						Parser.populatePredicateList(commandItem.getChildNodes(), command);
					}
				} else  if ( nodeName.equalsIgnoreCase("dependencies") ) {
					NodeList dependenciesNodeList = node.getChildNodes();
					for ( int j=0; j<dependenciesNodeList.getLength(); j++ ) {
						Node dependencyItem = dependenciesNodeList.item(j);
						NamedNodeMap attributes = dependencyItem.getAttributes();
						if (attributes == null) continue;
						Dependency dependency = new Dependency(artifact.getContainingFile());
						dependency.setDependency(attributes.getNamedItem("name").getNodeValue());
						artifact.addDependency(dependency);
						Parser.populatePredicateList(dependencyItem.getChildNodes(), dependency);
					}
				} else if ( nodeName.equalsIgnoreCase("libraries") ) {
					NodeList linkNodeList = node.getChildNodes();
					for ( int j=0; j<linkNodeList.getLength(); j++ ) {
						Node linkItem = linkNodeList.item(j);
						if ( linkItem.getNodeName().equalsIgnoreCase("library") ) {
							Library library = LibraryParser.parse(linkItem, artifact.getContainingFile());
							artifact.addLibrary(library);
						}
					}
				} else if ( nodeName.equalsIgnoreCase("static-link-libraries") ) {
					NodeList linkNodeList = node.getChildNodes();
					for ( int j=0; j<linkNodeList.getLength(); j++ ) {
						Node linkItem = linkNodeList.item(j);
						if ( linkItem.getNodeName().equalsIgnoreCase("library") ) {
							Library library = LibraryParser.parse(linkItem, artifact.getContainingFile());
							artifact.addStaticLinkLibrary(library);
						}
					}
				} else if ( nodeName.equalsIgnoreCase("objects") ) {
					NodeList linkNodeList = node.getChildNodes();
					for ( int j=0; j<linkNodeList.getLength(); j++ ) {
						Node linkItem = linkNodeList.item(j);
						if ( linkItem.getNodeName().equalsIgnoreCase("group") ||
								linkItem.getNodeName().equalsIgnoreCase("object") ) {
							Object object = ObjectParser.parse(linkNodeList.item(j), artifact.getContainingFile());
							artifact.addObject(object);				
						}
					}
				} else if ( nodeName.equalsIgnoreCase("vpaths") ) {
					NodeList vpathsNodeList = node.getChildNodes();
					for ( int j=0; j<vpathsNodeList.getLength(); j++ ) {
						Node vpathItem = vpathsNodeList.item(j);
						if ( vpathItem.getNodeName().equalsIgnoreCase("vpath") ) {
							VPath vpath = VPathParser.parse(vpathItem, artifact.getContainingFile());
							artifact.addVPath(vpath);
						}
					}
				}  else if ( nodeName.equalsIgnoreCase("makefilestubs") ) {
					NodeList mfsNodeList = node.getChildNodes();
					for ( int j=0;j<mfsNodeList.getLength(); j++ ) {
						Node mfsItem = mfsNodeList.item(j);
						if ( mfsItem.getNodeName().equalsIgnoreCase("makefilestub") ) {
							NamedNodeMap attributes = mfsItem.getAttributes();
							Node dataNode = attributes.getNamedItem("data");
							MakefileStub makefileStub = new MakefileStub(dataNode.getNodeValue(), artifact.getContainingFile());
							artifact.addMakefileStub(makefileStub);
							Parser.populatePredicateList(mfsItem.getChildNodes(), makefileStub);
						}
					}
				} else if ( nodeName.equalsIgnoreCase("exports") ) {
					NodeList exportsNodeList = node.getChildNodes();
					for ( int j=0; j<exportsNodeList.getLength(); j++ ) {
						Node exportNode = exportsNodeList.item(j);
						if ( exportNode.getNodeName().equalsIgnoreCase("group") ) {
							Export group = ExportParser.parse(exportNode, artifact.getContainingFile());
							artifact.addExport(group);
							group.setType(Export.TYPE_GROUP);
						} else if ( exportNode.getNodeName().equalsIgnoreCase("export") ) {
							Export group = ExportParser.parse(exportNode, artifact.getContainingFile());
							artifact.addExport(group);
							group.setType(Export.TYPE_FUNCTION);
						}
					}
				} else if (nodeName.equalsIgnoreCase("flags") ) {
					NodeList flagsNodeList = node.getChildNodes();
					for ( int j=0; j<flagsNodeList.getLength(); j++ ) {
						Node item = flagsNodeList.item(j);
						Flag f = FlagParser.parse(item, artifact.getContainingFile());
						if (f != null) {
							artifact.addFlag(f);
						}
					}
				}
			}
			
			NamedNodeMap attributes = artifactNode.getAttributes();
			
			Node nameNode = attributes.getNamedItem("name");
			artifact.setName(nameNode.getNodeValue());
			
			Node typeNode = attributes.getNamedItem("type");
			artifact.setType(typeNode.getNodeValue());
			
			Node bundleNode = attributes.getNamedItem("bundle");
			if ( bundleNode != null ) {
				artifact.setBundle(bundleNode.getNodeValue());
			}
			
			Node scopeNode = attributes.getNamedItem("scope");
			if ( scopeNode != null ) {
				artifact.setScope(scopeNode.getNodeValue());
			}
			
			Node includeInAllTargetNode = attributes.getNamedItem("all");
			if ( includeInAllTargetNode.getNodeValue().equalsIgnoreCase("true") ) {
				artifact.setIncludeInAllTarget(true);
			} else {
				artifact.setIncludeInAllTarget(false);
			}
			
			Node loadgroupNode = attributes.getNamedItem("loadgroup");
			if ( loadgroupNode != null ) {
				artifact.setLoadgroup(loadgroupNode.getNodeValue());
			}
			
			Node buildLocal = attributes.getNamedItem("buildlocal");
			if ( buildLocal.getNodeValue().equalsIgnoreCase("true") ) {
				artifact.setBuildLocal(true); 
			}
			
			Node appendReleaseNode = attributes.getNamedItem("appendrelease");
			if ( appendReleaseNode.getNodeValue().equalsIgnoreCase("true") ) {
				artifact.setAppendRelease(true);
			}

			Node consoleNode = attributes.getNamedItem("console");
			if ( consoleNode.getNodeValue().equalsIgnoreCase("false") ) {
				artifact.setConsoleApplication(false);
			} else {
				artifact.setConsoleApplication(true);
			}
			
			Parser.populatePredicateList(nodeList, artifact);

		return artifact;
	}


}
