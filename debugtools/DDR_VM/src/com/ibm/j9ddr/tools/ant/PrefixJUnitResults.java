/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.DirectoryScanner;
import org.apache.tools.ant.Task;
import org.apache.tools.ant.types.FileSet;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.ls.DOMImplementationLS;
import org.w3c.dom.ls.LSOutput;
import org.w3c.dom.ls.LSSerializer;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

/**
 * Ant task for mutating the output of the Ant JUnit
 * task so we can merge the output of the same tests run
 * on multiple platforms and with multiple sets of input data
 * 
 * usage:
 * 
 * <prefixjunitresults prefix="linux_x86-32.default" outputdir="${junit.output.dir}">
 * 	<fileset ..../>
 * </prefixjunitresults>
 * 
 * @author andhall
 *
 */
public class PrefixJUnitResults extends Task
{
	private String prefix = null;
	
	private File outputDirectory = null;
	
	private List<FileSet> fileSets = new LinkedList<FileSet>();
	
	private boolean verbose = false;
	
	public void setPrefix(String prefix)
	{
		this.prefix = prefix;
	}
	
	public void setOutputDir(File outputDir)
	{
		this.outputDirectory = outputDir;
	}
	
	public void addFileset(FileSet fileset) {
		fileSets.add(fileset);
	}
	
	public void setVerbose(boolean verbose) {
		this.verbose = verbose;
	}

	@Override
	public void execute() throws BuildException
	{
		if (fileSets.size() == 0) {
			throw new BuildException("No filesets supplied.");
		}
		
		if (prefix == null) {
			throw new BuildException("prefix not specified");
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
					
					process(thisFile);
					
					count++;
				
				} catch (IOException ex) {
					throw new BuildException(ex);
				} catch (SAXException e) {
					throw new BuildException(e);
				}
			}
		}
		
		log("Processed " + count + " XML files");
	}

	private void process(File xmlFile) throws IOException, SAXException
	{
		File outputFile = getOutputFile(xmlFile);
		
		if (verbose) {
			log("Mapping " + xmlFile.getAbsolutePath() + " to " + outputFile.getAbsolutePath());
		}
		
		DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
		
		DocumentBuilder builder = null;
		try {
			builder = factory.newDocumentBuilder();
		} catch (ParserConfigurationException e) {
			throw new RuntimeException(e);
		}
		
		Document doc = builder.parse(new InputSource(new FileReader(xmlFile)));
		
		/* Find the testsuite nodes and prefix the name element */
		NodeList testSuiteNodes = doc.getElementsByTagName("testsuite");
		
		if (testSuiteNodes.getLength() == 0) {
			log("Couldn't find testsuite node in " + xmlFile.getAbsolutePath());
		}
		
		for (int i=0; i < testSuiteNodes.getLength(); i++) {
			Node thisTestSuite = testSuiteNodes.item(i);
			
			NamedNodeMap attributes = thisTestSuite.getAttributes();
			
			Attr nameNode = (Attr) attributes.getNamedItem("name");
			
			if (null == nameNode) {
				log("Couldn't find name attribute on testsuite element in " + xmlFile.getAbsolutePath());
				continue;
			}
			
			String oldValue = nameNode.getValue();
			nameNode.setValue(prefix + "." + oldValue);
		}
		
		/* Do the same for the testcase nodes */
		NodeList testCaseNodes = doc.getElementsByTagName("testcase");
		
		for (int i=0; i < testCaseNodes.getLength(); i++) {
			Node thisTestCase = testCaseNodes.item(i);
			
			NamedNodeMap attributes = thisTestCase.getAttributes();
			
			Attr classNameNode = (Attr) attributes.getNamedItem("classname");
			
			if (null == classNameNode) {
				log("Couldn't find classname attribute on testcase element in " + xmlFile.getAbsolutePath());
				continue;
			}
			
			String oldValue = classNameNode.getValue();
			classNameNode.setValue(prefix + "." + oldValue);
		}
		
		/* Write out to new location */
		DOMImplementationLS ls = (DOMImplementationLS) builder.getDOMImplementation();
		
		LSSerializer serializer = ls.createLSSerializer();
		
		LSOutput xmlSink = ls.createLSOutput();
		xmlSink.setCharacterStream(new FileWriter(outputFile));
		
		serializer.write(doc, xmlSink);
	}

	private File getOutputFile(File inputFile)
	{
		String fileName = inputFile.getName();
		
		if (fileName.startsWith("TEST-")) {
			fileName = "TEST-" + prefix + "." + fileName.substring(5);
		} else {
			fileName = prefix + "." + fileName;
		}
		
		File outputFile = new File(outputDirectory,fileName);
		return outputFile;
	}
}
