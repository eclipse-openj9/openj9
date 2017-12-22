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

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Hashtable;
import java.util.Vector;

import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.validation.Schema;
import javax.xml.validation.SchemaFactory;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import com.ibm.uma.IConfiguration;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Module;
import com.ibm.uma.om.Predicate;
import com.ibm.uma.om.PredicateList;
import com.ibm.uma.om.SubdirArtifact;
import com.ibm.uma.util.Logger;

public class Parser implements EntityResolver {
	
	static String fileCurrentlyBeingParsed;
	static public String getFileCurrentlyBeingParsed() {
		return fileCurrentlyBeingParsed;
	}
	
	UMA generator;
	Vector<Module> modules = new Vector<Module>();
	Hashtable<String,Module> modulesByFullName = new Hashtable<String,Module>();
	DocumentBuilder domParser;
	
	public Parser(UMA generator) {
		this.generator = generator;

		try {
			URL schemaURL = getClass().getResource("module.xsd");
			SchemaFactory schemaFactory = SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI);
			Schema schema = schemaFactory.newSchema(schemaURL);
			
			
			DocumentBuilderFactory domParserFactory = DocumentBuilderFactory.newInstance();
			domParserFactory.setNamespaceAware(true);
			domParserFactory.setSchema(schema);
			domParserFactory.setXIncludeAware(true);

			domParser = domParserFactory.newDocumentBuilder();
			//domParser.setEntityResolver(this);
			domParser.setErrorHandler(new ParserErrorHandler());
		} catch (ParserConfigurationException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (SAXException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}


	}
	
	IConfiguration getConfiguration() {
		return generator.getConfiguration();
	}
	
	public Vector<Module> getModules() { 
		return modules;
	}

	Document parseFile(String xmlFile) throws SAXException, IOException {
		fileCurrentlyBeingParsed = xmlFile;
		Document retval = domParser.parse(xmlFile);
		fileCurrentlyBeingParsed = null;	
		return retval;
	}
	
	void resolveModuleHierarchy() throws UMAException {

		generator.validateArtifactLocations();

		// Make sure that all modules have parents all the way to the root.
		// If a hole is found and a module with artifacts of type subdir.
		//System.out.println("resolveModuleHierarchy");
		boolean modulesToBeChecked = true;
		while (modulesToBeChecked) {
			//System.out.println("resolveModuleHierarchy - modulesToBeChecked");
			// Store modules in a hashtable by full name.  
			// e.g., root/gc/module.xml is 'gc'
			//       root/gc/modron/base/module.xml is 'gc/modron/base'
			for ( Module module : modules ) {
				if ( !module.evaluate() ) continue;
				addModuleByLogicalFullname(module);
			}
			Vector<Module> newModules = new Vector<Module>();
			modulesToBeChecked = false;
			
			for ( Module module : modules) {
				if ( !module.evaluate() ) continue;
				// ensure each parent of a module exists.
				if ( module.isTopLevel() ) continue;
				String[] parents = module.getParents();
				for ( int depth=0; depth<module.getModuleDepth(); depth++ ) {
					Module parentMod = getModuleByLogicalFullname(parents[depth]);
					if ( parentMod == null ) {
						// need to create the parent
						String moduleXmlFilename = UMA.getUma().getRootDirectory();
						for ( int d=1; d<=depth; d++ ) {
							moduleXmlFilename = moduleXmlFilename + module.moduleNameAtDepth(d) + "/";
						}
						moduleXmlFilename = moduleXmlFilename + getConfiguration().getMetadataFilename();
						parentMod = new Module(moduleXmlFilename);
						addModuleByLogicalFullname(parentMod);
						newModules.add(parentMod);
						modulesToBeChecked = true;
					}
				}
			}
			modules.addAll(newModules);
			for ( Module module : modules) {
				if ( !module.evaluate() ) continue;
				// ensure each parent of a module exists.
				if ( module.isTopLevel()) continue;
				String[] parents = module.getParents();
				for ( int depth=0; depth<module.getModuleDepth(); depth++ ) {
					Module parentMod = getModuleByLogicalFullname(parents[depth]);
					boolean found = false;
					String child = module.moduleNameAtDepth(depth+1);
					for ( Artifact artifact : parentMod.getArtifacts() ){
						if ( !artifact.evaluate()) continue;
						if ( artifact.getType() == Artifact.TYPE_SUBDIR ) {
							if ( artifact.getTargetName().equalsIgnoreCase(child) ) {
								found = true;
							}
						}
					}
					if ( !found && !child.equalsIgnoreCase(UMA.getUma().getConfiguration().getMetadataFilename())) {
						Module subdirModule = getModuleByLogicalFullname(parentMod.getLogicalFullName()+"/"+child);
						Artifact artifact = new SubdirArtifact(child, subdirModule, parentMod);
						parentMod.addArtifact(artifact);
					}
				}
			}
		}
		
	}
	
	void addAllModulesFoundInDiretory( File dir, Vector<String> moduleFilenames ) {
		String rootDir = generator.getRootDirectory();
		File [] dirListing = dir.listFiles();
		Vector<File> directories = new Vector<File>();
		for ( File file : dirListing ) {
			if ( file.isDirectory() ) {
				directories.add(file);
			} else if ( file.getName().equalsIgnoreCase(generator.getConfiguration().getMetadataFilename()) ) {
				String modulePath = file.getParent();
				if ( modulePath.equalsIgnoreCase(rootDir) || rootDir.equalsIgnoreCase(modulePath+File.separator)) { 
					modulePath = "";
				} else {
					modulePath = file.getParent().substring(rootDir.length());
				}
				modulePath = modulePath.replace(File.separator, "/");
				moduleFilenames.add(modulePath);
				Logger.getLogger().println(Logger.InformationL2Log, "Adding " + modulePath);
			}
		}
		for ( File file : directories ) {
			addAllModulesFoundInDiretory(file, moduleFilenames);
		}
	}
	
	public Vector<String> getModuleFilenames(Vector<String> moduleFilenames) {
		String rootDir = generator.getRootDirectory();
		String metaDataFileName = generator.getConfiguration().getMetadataFilename();
		Vector<String> filenames = new Vector<String>();
		for ( String name : moduleFilenames ) {
			filenames.add( rootDir + name + "/" + metaDataFileName);
		}
		return filenames;
	}

	
	public boolean parse() throws UMAException {
		Vector<String> moduleFilenames = new Vector<String>();
		File rootDirectory = new File(generator.getRootDirectory());
		addAllModulesFoundInDiretory(rootDirectory, moduleFilenames);

		// parse it to determine directories to search for module.xml files.
		moduleFilenames = getModuleFilenames(moduleFilenames);
		
		// parse all found module.xml files.
		for (String moduleXmlFilename : moduleFilenames) {
			try {
				Logger.getLogger().println(Logger.InformationL2Log, "Parsing " + moduleXmlFilename);
				Document doc = parseFile(moduleXmlFilename);
				Module module = ModuleParser.parse(doc, moduleXmlFilename);
				modules.add( module );
			} catch (SAXException e) {
				throw new UMAException("Error: Module " + moduleXmlFilename + " failed to parse.", e);
			} catch (IOException e) {
				throw new UMAException("Error: " + e.getMessage(), e);
			}
		}
		
		resolveModuleHierarchy();
		
		return true;
	}

	static public void populatePredicateList(NodeList nodeList, PredicateList predicates) {
		for ( int j=0; j<nodeList.getLength(); j++ ) {
			Node item = nodeList.item(j);
			NamedNodeMap attributes = item.getAttributes();
			if ( attributes == null ) continue;
			Node conditionNode = attributes.getNamedItem("condition");
			int predicateType = Predicate.EXCLUDE_IF;
			if ( item.getNodeName().equalsIgnoreCase("include-if") ) {
				predicateType = Predicate.INCLUDE_IF;
			} else if ( item.getNodeName().equalsIgnoreCase("exclude-if") ) {
				predicateType = Predicate.EXCLUDE_IF;
			} else continue;
			predicates.add(new Predicate(predicateType,conditionNode.getNodeValue()));
		}
	}



	public InputSource resolveEntity(String publicId, String systemId) throws SAXException, IOException {
		if (!systemId.endsWith("module.xsd")) {
			throw new SAXException("Unable to resolve schema named [" + systemId + "]");
		}

		InputStream stream = getClass().getResourceAsStream("module.xsd");
		return new InputSource(stream);
	}
	
	public Module getModuleByLogicalFullname(String name) {
		return modulesByFullName.get(name);
	}
	
	public void addModuleByLogicalFullname(Module module ) {
		//System.out.println("adding logical name: " + module.getLogicalFullName());
		modulesByFullName.put(module.getLogicalFullName(), module);
	}
	

	
}
