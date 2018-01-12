/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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
package com.ibm.j9tools.om.io;

import java.io.File;
import java.io.InputStream;
import java.text.MessageFormat;
import java.util.HashSet;
import java.util.Set;

import javax.xml.XMLConstants;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.InputSource;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;

/**
 * AbstractParser provides extensions to the standard SAX DefaultHandler for
 * error propagation, and schema lookup.
 */
public abstract class AbstractParser extends DefaultHandler {

	/** The JAXP property for schema language. */
	static final String JAXP_SCHEMA_LANGUAGE = "http://java.sun.com/xml/jaxp/properties/schemaLanguage"; //$NON-NLS-1$

	/** The JAXP property for schema language. */
	static final String JAXP_SCHEMA_SOURCE = "http://java.sun.com/xml/jaxp/properties/schemaSource"; //$NON-NLS-1$

	/**
	 * Used by the SAXParser as an intermediary between start and end element handlers. Contains the value between
	 * <element>value</element> once the end handler is invoked.
	 */
	protected String _parsedValue = ""; //$NON-NLS-1$

	/** The underlying SAX parser that does the walk. */
	protected SAXParser _parser;

	/** Parse Errors. */
	protected Set<Throwable> _warnings = new HashSet<Throwable>();
	protected Set<Throwable> _errors = new HashSet<Throwable>();

	/**
	 * The Locator instance set via setDocumentLocator handler called once when
	 * we start parsing a document. Keep track of it in order to be able to retrieve
	 * parse location data after the initial setDocumentLocator event. 
	 */
	protected Locator _documentLocator;

	/**
	 * The currently parsed File. Used as part of the SourceLocation data stored
	 * for each relevant line of parsed XML.
	 */
	protected File _documentLocatorFile;

	/**
	 * 
	 */
	public AbstractParser() {

		try {
			// Create a parser factory and configure it to enable namespaces/validation.
			SAXParserFactory saxFactory = SAXParserFactory.newInstance();
			saxFactory.setNamespaceAware(true);
			saxFactory.setValidating(true);

			// Create a new parser
			_parser = saxFactory.newSAXParser();
			_parser.setProperty(JAXP_SCHEMA_LANGUAGE, XMLConstants.W3C_XML_SCHEMA_NS_URI);
			//			_parser.setProperty(JAXP_SCHEMA_SOURCE, getSchemaName());

			// Point all handlers at this object
			XMLReader xmlReader = _parser.getXMLReader();
			xmlReader.setContentHandler(this);
			xmlReader.setErrorHandler(this);
			xmlReader.setDTDHandler(this);
			xmlReader.setEntityResolver(this);

		} catch (Exception e) {
			// Promote the exception to a fatal error.
			throw new Error(e);
		}
	}

	/**
	 * @see org.xml.sax.ContentHandler#characters(char[], int, int)
	 */
	@Override
	public void characters(char[] ch, int start, int length) {
		_parsedValue += new String(ch, start, length);
	}

	/**
	 * @return The name of the schema expected by the parser.
	 */
	public abstract String getSchemaName();

	/**
	 * @see org.xml.sax.helpers.DefaultHandler#resolveEntity(java.lang.String, java.lang.String)
	 */
	@Override
	public InputSource resolveEntity(String publicId, String systemId) throws SAXException {
		File schemaFile = new File(systemId);
		InputStream stream = getClass().getResourceAsStream("/" + schemaFile.getName()); //$NON-NLS-1$

		if (stream == null) {
			_errors.add(new SAXException(MessageFormat.format(Messages.getString("AbstractParser.0"), new Object[] { schemaFile.getName() }))); //$NON-NLS-1$
		} else {
			_parser.setProperty(JAXP_SCHEMA_SOURCE, schemaFile.getName());
		}

		return new InputSource(stream);
	}

	@Override
	public void setDocumentLocator(Locator locator) {
		this._documentLocator = locator;
	}

	public void setDocumentLocatorFile(File file) {
		this._documentLocatorFile = file;
	}

	/**
	 * @see org.xml.sax.ErrorHandler#warning(org.xml.sax.SAXParseException)
	 */
	@Override
	public void warning(SAXParseException exception) {
		_warnings.add(exception);
	}

	/**
	 * @see org.xml.sax.ErrorHandler#error(org.xml.sax.SAXParseException)
	 */
	@Override
	public void error(SAXParseException exception) {
		_errors.add(exception);
	}

	/**
	 * @see org.xml.sax.ErrorHandler#fatalError(org.xml.sax.SAXParseException)
	 */
	@Override
	public void fatalError(SAXParseException exception) {
		_errors.add(exception);
	}

	/**
	 * Determines if there has been any SAX parse errors.
	 * 
	 * @return	<code>true</code> if there are SAX errors, <code>false</code> otherwise
	 */
	public boolean hasErrors() {
		return _errors.size() != 0;
	}

	/**
	 * Determines if there has been any SAX parse warnings.
	 * 
	 * @return	<code>true</code> if there are SAX warnings, <code>false</code> otherwise
	 */
	public boolean hasWarnings() {
		return _warnings.size() != 0;
	}

	/**
	 * Retrieves the SAX parse errors.
	 * 
	 * @return	the SAX parse errors.
	 */
	public Set<Throwable> getErrors() {
		return _errors;
	}

	/**
	 * Retrieves the SAX parse warnings.
	 * 
	 * @return	the SAX parse warnings.
	 */
	public Set<Throwable> getWarnings() {
		return _warnings;
	}
}
