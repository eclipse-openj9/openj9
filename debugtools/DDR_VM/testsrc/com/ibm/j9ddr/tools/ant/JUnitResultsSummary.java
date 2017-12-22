/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ant;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathExpression;
import javax.xml.xpath.XPathExpressionException;
import javax.xml.xpath.XPathFactory;

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.DirectoryScanner;
import org.apache.tools.ant.Task;
import org.apache.tools.ant.types.FileSet;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

/**
 * Ant task for examining a set of junit results and showing only those test cases which failed.
 * This task only supports XML formatted results.
 * 
 * usage:
 * 
 * <junitresultssummary property="<name of property to store results in>" file="<path to a file in which to write the summary>"
 * 		overwrite="true/false - force overwrite of existing summary file" verbose="true/false">
 * 	<fileset ..../>
 * </junitresultssummary>
 * 
 * @author Adam Pilkington
 *
 */
public class JUnitResultsSummary extends Task {
	private String property = null;		//name of the property to store the results in
	private File file = null;			//full path to the file
	private boolean verbose = false;
	private boolean overwrite = false;
	
	private List<FileSet> fileSets = new LinkedList<FileSet>();
	
	public void setProperty(String property)
	{
		this.property = property;
	}
	
	public void setFile(File file)
	{
		this.file = file;
	}
	
	public void addFileset(FileSet fileset) {
		fileSets.add(fileset);
	}
	
	public void setVerbose(boolean verbose) {
		this.verbose = verbose;
	}
	
	public void setOverwrite(boolean enabled) {
		overwrite = enabled;
	}
	
	@Override
	public void execute() throws BuildException
	{
		StringBuilder out = new StringBuilder();
		if (fileSets.size() == 0) {
			throw new BuildException("No filesets supplied.");
		}
		
		if ((property == null) && (null == file)) {
			throw new BuildException("property or output file not specified");
		}
		
		int count = 0;
		
		for (FileSet thisFileSet : fileSets) {
			DirectoryScanner scanner = thisFileSet.getDirectoryScanner(getProject());

			String files[] = scanner.getIncludedFiles();
			
			if (files.length == 0) {
				log("Empty fileset supplied");
			}

			for (String fileName : files) {
				File thisFile = new File(scanner.getBasedir(),fileName);
				try {
					if (! thisFile.exists()) {
						log("Ignoring " + thisFile.getCanonicalPath() + ", because it doesn't exist");
						continue;
					}
					
					if(verbose) {
						log("Processing file " + thisFile.getCanonicalPath());
					}
					
					process(thisFile, out);
					
					count++;
				
				} catch (Exception ex) {
					throw new BuildException(ex);
				}
			}
		}
		
		log("Processed " + count + " test cases");
		
		if(out.length() == 0) {
			out.append("No failures detected");
		}
		
		if(property != null) {
			getProject().setNewProperty(property, out.toString());
		}
		if(file != null) {
			writeOutputFile(out);
		}
	}
	
	private void process(File xmlFile, StringBuilder out) throws IOException, SAXException, XPathExpressionException
	{		
		DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
		
		DocumentBuilder builder = null;
		try {
			builder = factory.newDocumentBuilder();
		} catch (ParserConfigurationException e) {
			throw new RuntimeException(e);
		}
		
		Document doc = builder.parse(new InputSource(new FileReader(xmlFile)));
		
		//find all test nodes which contain a child element, which can be a failure or error
		XPathFactory xpfactory = XPathFactory.newInstance();
		XPath path = xpfactory.newXPath();
		XPathExpression xpr = path.compile("//testcase[./*]");
		Object results = xpr.evaluate(doc, XPathConstants.NODESET);
		NodeList failures = (NodeList) results;
		
		if (failures.getLength() == 0) {
			//no failures were detected
			if(verbose) {
				log("No failures found in " + xmlFile.getCanonicalPath());
			}
			return;
		}
		
		writeAttribute(doc.getFirstChild(),"Test failure : ", "name", "\n", out);
		//iterate over failures writing out a summary
		for (int i=0; i < failures.getLength(); i++) {
			Node failure = failures.item(i);
			writeAttribute(failure,        "Test case    : ", "name", "\n", out);
			NodeList children = failure.getChildNodes();
			for(int j = 0; j < children.getLength(); j++) {
				Node child = children.item(j);
				if(child.getNodeName().equals("error")) {
					writeAttribute(child,  "Test error   : ", "type", "\n", out);
					out.append(child.getTextContent());
					out.append("\n");
				}
				if(child.getNodeName().equals("failure")) {
					writeAttribute(child,  "Failure type : ", "type", "\n", out);
					out.append(child.getTextContent());
					out.append("\n");
				}
			}
		}
		
	}
	
	private void writeAttribute(Node node,String prefix, String name, String suffix, StringBuilder out) {
		out.append(prefix);
		NamedNodeMap attributes = node.getAttributes();
		Attr attr = (Attr) attributes.getNamedItem(name);
		if (null == attr) {
			out.append("[missing attribute : " + name + "]");
		} else {
			out.append(attr.getValue());
		}
		out.append(suffix);
	}

	private void writeOutputFile(StringBuilder out) throws BuildException {
		try {
			if(file.exists()) {
				if(overwrite) {
					boolean fileDeleted = file.delete();
					if(!fileDeleted) {
						throw new BuildException("Could not delete existing results file " + file.getCanonicalPath());
					}
					log("Deleing existing summary file");
				} else {
					throw new BuildException("Output file " + file.getCanonicalPath() + " already exists");
				}
			}
			
			FileWriter writer = new FileWriter(file);
			writer.write(out.toString());
			writer.close();
			log("Summary written to " + file.getCanonicalPath());
		} catch (IOException e) {
			throw new BuildException("Failed to create output file", e);
		}
	}
}
