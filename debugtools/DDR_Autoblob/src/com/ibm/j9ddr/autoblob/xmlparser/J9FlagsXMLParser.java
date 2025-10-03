/*
 * Copyright IBM Corp. and others 2011
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.autoblob.xmlparser;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

/**
 * Used to create mapping between flags ids and their equivalent cNames in the j9.flags file.
 *
 * @author lpnguyen
 */
public final class J9FlagsXMLParser {

	private static final String[] knownNames = { "JIT", "JNI", "INL", "AOT", "Arg0EA" };

	private static final class XMLHandler extends DefaultHandler implements ContentHandler {

		private static String cName(String id) {
			boolean addUnderscore = false;
			int idLength = id.length();
			StringBuilder cName = new StringBuilder(idLength);

			outer: for (int index = 0; index < idLength; index++) {
				if (addUnderscore) {
					cName.append("_");
					addUnderscore = false;
				}

				for (String known : knownNames) {
					if (id.startsWith(known, index)) {
						cName.append(known);
						index += known.length();
						addUnderscore = true;
						continue outer;
					}
				}

				char nextChar = id.charAt(index);
				if ((index < idLength - 1) && !Character.isUpperCase(nextChar)) {
					addUnderscore = Character.isUpperCase(id.charAt(index + 1));
				}
				cName.append(Character.toUpperCase(nextChar));
			}

			return cName.toString();
		}

		private final Map<String, String> cNameToFlagId;

		XMLHandler() {
			super();
			this.cNameToFlagId = new HashMap<>();
		}

		Map<String, String> getCNameToIdMap() {
			return cNameToFlagId;
		}

		@Override
		public void startElement(String uri, String localName, String qName, Attributes atts) throws SAXException {
			if (qName.equals("flag")) {
				String id = atts.getValue("id");
				if (null != cNameToFlagId.put(cName(id), id)) {
					throw new UnsupportedOperationException("Can't create 1-1 mapping from cName to id for id:" + id);
				}
			}
		}

		@Override
		public void endElement(String uri, String localName, String qName) throws SAXException {
			// do nothing
		}

		@Override
		public void error(SAXParseException exception) throws SAXException {
			throw exception;
		}

		@Override
		public void warning(SAXParseException exception) throws SAXException {
			System.err.println("Warning parsing XML");
			exception.printStackTrace();
		}
	}

	private final XMLHandler handler;

	public J9FlagsXMLParser(File xmlFile) throws SAXException, IOException {
		SAXParser parser;

		try {
			parser = SAXParserFactory.newInstance().newSAXParser();
		} catch (ParserConfigurationException e) {
			throw new SAXException(e);
		}

		this.handler = new XMLHandler();

		parser.parse(new InputSource(new FileReader(xmlFile)), handler);
	}

	public Map<String, String> getCNameToFlagIdMap() {
		return handler.getCNameToIdMap();
	}

}
