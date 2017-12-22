/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.xmlparser;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;
import org.xml.sax.helpers.XMLReaderFactory;


/**
 * Used to create mapping between flags ids and their equivalent cNames in the j9.flags file
 * 
 * @author lpnguyen
 *
 */
public class J9FlagsXMLParser
{	
	public static final String[] knownNames = {"JIT", "JNI", "INL", "AOT", "Arg0EA"};
	
	private final XMLHandler handler;
	
	public J9FlagsXMLParser(File xmlFile) throws SAXException, IOException
	{
		this.handler = new XMLHandler();
		
		XMLReader reader = XMLReaderFactory.createXMLReader();
		
		reader.setContentHandler(handler);
		reader.setErrorHandler(handler);
		
		reader.parse(new InputSource(new FileReader(xmlFile)));

	}
	
	public Map<String, String> getCNameToFlagIdMap() {
		return handler.getCNameToIdMap();
	}
	
	private static class XMLHandler extends DefaultHandler implements ContentHandler
	{
		final Map<String, String> cNameToFlagId = new HashMap<String, String>();
		
		public Map<String, String> getCNameToIdMap() {
			return cNameToFlagId;
		}
	
		private String cName(String id)
		{
			boolean addUnderscore = false;
			boolean knownNameFound = false;
			
			StringBuffer cName = new StringBuffer(id.length());
			
			for (int index = 0; index < id.length(); index++) {
				if (addUnderscore) {
					cName.append("_");
					addUnderscore = false;
				}
				
				knownNameFound = false;
				for (int j = 0; j < knownNames.length; j++) {
					String known = knownNames[j];
					if (id.startsWith(known, index)) {
						cName.append(known);
						index = index + known.length();
						addUnderscore = true;
						knownNameFound = true;
						break;
					}
				}
				
				if (!knownNameFound) {
					if ((!Character.isUpperCase(id.charAt(index))) && (index < id.length()-1) && Character.isUpperCase(id.charAt(index+1))) {
						addUnderscore = true;
					}
					cName.append(Character.toUpperCase(id.charAt(index)));
				}
			}
			
			return cName.toString();
		}

		@Override
		public void startElement(String uri, String localName, String qName, Attributes atts) throws SAXException
		{
			if (qName.equals("flag")) {
				String id = atts.getValue("id");
				if (null != cNameToFlagId.put(cName(id), id)) {
					throw new UnsupportedOperationException("Can't create 1-1 mapping from cName to id for id:" + id);
				}
			}
		}

		@Override
		public void endElement(String uri, String localName, String qName) throws SAXException
		{
			
		}
		
		@Override
		public void error(SAXParseException arg0) throws SAXException
		{
			throw arg0;
		}
		
		@Override
		public void warning(SAXParseException arg0) throws SAXException
		{
			System.err.println("Warning parsing XML");
			arg0.printStackTrace();
		}
	}
}
