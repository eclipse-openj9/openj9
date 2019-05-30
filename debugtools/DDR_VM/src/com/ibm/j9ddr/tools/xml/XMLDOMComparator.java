/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools.xml;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Properties;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathExpressionException;
import javax.xml.xpath.XPathFactory;

import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

/**
 * Utility class which reads in a configuration
 * properties file which defines a series of XPath
 * statements to execute against two source files. 
 * The results are then compared for equivalence.
 * This class is used to check that the output from 
 * J9DDR is the same as for JExtract. XPath is used to
 * identify the nodes to compare to allow specific node
 * testing without requiring a complete J9DDR implementation.
 * 
 * @author Adam Pilkington
 *
 */
public class XMLDOMComparator {
	protected HashMap<String, File> opts = new HashMap<String, File>();
	protected static final String OPT_J9DDR_FILE = "-ddr";
	protected static final String OPT_JEXTRACT_FILE = "-jx";
	protected static final String OPT_CONFIG_FILE = "-f";
	private Document xmlJ9DDR = null;			//parsed and loaded J9DDR XML DOM
	private Document xmlJExtract = null;		//parsed and loaded JExtract XML DOM
	private Properties xpathQueries = null;		//the set of XPath queries to be applied	
	private ArrayList<String> messages = new ArrayList<String>();
	
	public Iterator<String> getMessages() {
		return messages.iterator();
	}
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		XMLDOMComparator comp = new XMLDOMComparator();
		comp.parseArgs(args);
		comp.compareXML();
	}

	/**
	 * Compare the J9DDR and JExtract XML to check that
	 * they are equal according to the XPath selectors in
	 * the configuration file.
	 * 
	 * @return true if they are equal, false if not.
	 */
	public boolean compareXML() {
		parseXML();
		loadXPathProperties();
		return areDOMsEqual();
	}
	
	private boolean areDOMsEqual() {
		boolean result = true;			//result			
		XPathFactory factory = XPathFactory.newInstance();
		XPath xpathJ9DDR = factory.newXPath();
		XPath xpathJExtract = factory.newXPath();
		for(Iterator<Object> i = xpathQueries.keySet().iterator(); i.hasNext();) {
			String key = (String)i.next();
			String xpath = xpathQueries.getProperty(key);
			messages.add("Test : " + key + " : " + xpath);
			try {
				NodeList resultJ9DDR = (NodeList) xpathJ9DDR.evaluate(xpath, xmlJ9DDR, XPathConstants.NODESET);
				NodeList resultJExtract = (NodeList) xpathJExtract.evaluate(xpath, xmlJExtract, XPathConstants.NODESET);
				boolean testResult = areNodeListsEqual(resultJ9DDR, resultJExtract);
				if(!testResult) {
					messages.add("Test failed : node lists are not equal");
				}
				result &= testResult;
			} catch (XPathExpressionException e) {
				messages.add("Skipping test, error in XPath expression : " + e.getMessage());
			}
		}
		return result;
	}
	
	/**
	 * Tests that two node lists are the same. The check is stricter than allowed by XML
	 * in that nodes and attributes must be defined in the same order in both lists 
	 * otherwise they will be reported as different.
	 * 
	 * @param resultJ9DDR nodelist from J9DDR
	 * @param resultJExtract nodelist from JExtract
	 * @return true if the lists are the same, false if not
	 */
	private boolean areNodeListsEqual(NodeList resultJ9DDR, NodeList resultJExtract) {
		boolean result = true;
		if(resultJ9DDR.getLength() != resultJExtract.getLength()) {
			messages.add("The number of nodes returned does not match");
			return false;		//instantly fail if number of nodes does not match
		}
		for(int i = 0; i < resultJ9DDR.getLength(); i++) {
			Node nodeJ9DDR = resultJ9DDR.item(i);
			Node nodeJExtract = resultJExtract.item(i);
			result &= areNodeListsEqual(nodeJ9DDR.getChildNodes(), nodeJExtract.getChildNodes());	//recursive call to check children
			String nodeName = nodeJ9DDR.getNodeName();
			if(!nodeJ9DDR.getNodeName().equals(nodeJExtract.getNodeName())) {
				messages.add("The node names did not match : " + nodeJ9DDR.getNodeName()+ " != " + nodeJExtract.getNodeName());
				return false;		//fail if node names don't match
			}
			if(nodeJ9DDR.getNodeValue() == null) {
				if(nodeJExtract.getNodeValue() != null) {
					messages.add("One node had a null value whilst the other was non-null : Node : " + nodeName);
					return false;
				}
			} else {
				if(!nodeJ9DDR.getNodeValue().equals(nodeJExtract.getNodeValue())) {
					messages.add("The values for node " + nodeName + " do not match : " + nodeJ9DDR.getNodeValue() + " != " + nodeJExtract.getNodeValue());
					return false;		//fail if node values don't match
				}
			}
			if(nodeJ9DDR.hasAttributes() ^ nodeJExtract.hasAttributes()) {		//check attribute lists
				messages.add("The number of attributes for " + nodeName + " do not match");
				return false;		//one node has attributes the other one doesn't
			} else {				//either both have attributes or both have no attributes
				if(nodeJ9DDR.hasAttributes() && nodeJExtract.hasAttributes()) {			//examine attributes if present
					NamedNodeMap mapJ9DDR = nodeJ9DDR.getAttributes();
					NamedNodeMap mapJExtract = nodeJExtract.getAttributes();
					if(mapJ9DDR.getLength() != mapJExtract.getLength()) {
						messages.add("The number of attributes for " + nodeName + " do not match");
						return false;		//attribute count is not equal
					}
					for(int j = 0; j < mapJ9DDR.getLength(); j++) {
						Node attrJ9DDR = mapJ9DDR.item(j);
						Node attrJExtract = mapJExtract.item(j);
						if(!attrJ9DDR.getNodeName().equals(attrJExtract.getNodeName())) {
							messages.add("The attribute for " + nodeName + " does not match : " + attrJ9DDR.getNodeName() + " != " + attrJExtract.getNodeName());
							return false;		//attribute names don't match
						}
						if(!attrJ9DDR.getNodeValue().equals(attrJExtract.getNodeValue())) {
							messages.add("The attribute " + attrJ9DDR.getNodeName()+ " for node " + nodeName + " does not match : " + attrJ9DDR.getNodeValue() + " != " + attrJExtract.getNodeValue());
							return false;		//attribute values don't match
						}
					}
				}
			}
		}
		return result;			//if we get this far then everything matches
	}
	
	/**
	 * Load the xpath queries from the specified properties file
	 */
	private void loadXPathProperties() {
		try {
			xpathQueries = new Properties();
			FileInputStream fis = new FileInputStream(opts.get(OPT_CONFIG_FILE));
			xpathQueries.load(fis);
		} catch (Exception e) {
			throw new IllegalArgumentException("General failure parsing the XPath config : " + opts.get(OPT_CONFIG_FILE).getAbsolutePath() + ". " + e.getMessage());
		}
	}
	
	/**
	 * Parse the J9DDR and JExtract generated XML files
	 */
	private void parseXML() {
		try {
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			DocumentBuilder builder = factory.newDocumentBuilder();
			xmlJ9DDR = builder.parse(opts.get(OPT_J9DDR_FILE));
			xmlJExtract = builder.parse(opts.get(OPT_JEXTRACT_FILE));
		} catch (SAXException e) {
			throw new IllegalArgumentException("The XML file contains invalid XML");
		} catch (IOException e) {
			throw new IllegalArgumentException("Failed to read from XML file : " + e.getMessage());
		} catch (Exception e) {
			throw new IllegalArgumentException("General failure parsing the XML : " + e.getMessage());
		}
	}
	
	public XMLDOMComparator() {
		opts.put(OPT_J9DDR_FILE, null);
		opts.put(OPT_JEXTRACT_FILE, null);
		opts.put(OPT_CONFIG_FILE, null);
	}
	
	public void parseArgs(String[] args) {
		try {
			for(int i = 0; i < args.length; i+=2) {						
				if(opts.containsKey(args[i])) {
					File file = new File(args[i+1]);
					if(file.exists()) {
						opts.put(args[i], file);
					} else {
						throw new IllegalArgumentException("The file " + file.getAbsolutePath() + " does not exist or cannot be found");
					}
				} else {
					throw new IllegalArgumentException("Invalid option : " + args[i]);
				}
			}
		
			for(String key:opts.keySet()) {									
				File value = opts.get(key);
				if(value == null) {
					throw new IllegalArgumentException("The option " + key + " has not been set.");
				}
			}
		} catch (IllegalArgumentException e) {
			System.err.println(e.getMessage());
			printHelp();
			System.exit(1);
		}
	}
	
	/**
	 * Print usage help to stdout
	 */
	private final static void printHelp() {
		System.out.println("Usage :\n\njava XMLDOMComparator -ddr <path to J9DDR XML file> -jx <path to JExtract XML file> -f <path to config file>\n");
		System.out.println("<path to J9DDR XML file> : full path to the XML file which has been generated by J9DDR");
		System.out.println("<path to JExtract XML file> : full path to the XML file which has been generated by JExtract");
		System.out.println("<path to config file> : full path to the XPath properties file which controls the nodes to be tested");
	}
	
}
