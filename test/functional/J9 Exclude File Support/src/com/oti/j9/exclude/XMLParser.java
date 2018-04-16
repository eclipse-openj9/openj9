/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.oti.j9.exclude;

import java.io.*;
import java.util.*;
/*[IF CLDC]
import javax.microedition.io.*;
/*[ENDIF]*/

public class XMLParser {

	static final boolean THROW_ON_EOF    = true;
	static final boolean EOF_ENCOUNTERED = true;
	static final boolean VERBOSE         = false;

	private String              _fURI;
	private Reader              _fInput;
	private char                _fScan;
	private boolean             _fSuccess;
	private long                _fElapsedTime;
	private int                 _fLine = 1;
	private int                 _fColumn = 1;
	private int                 _fLevel;
	private IXMLDocumentHandler _fDocumentHandler;
	private char[]              _fbuf = new char[256];
	private int                 _fcount, _fpos;

	void assertTrue( boolean condition, String detail) throws XMLException
	{
		if (!condition)
			parseError(detail);
	}

	void parseError( String detail) throws XMLException
	{
		throw new XMLException(_fURI + "(" + _fLine + "):C" + _fColumn + " : " + detail);
	}

	boolean scan_char() throws XMLException
	{
		return scan_char(THROW_ON_EOF);
	}

	// Read a single character
	boolean scan_char(boolean throwOnEOF) throws XMLException
	{
		try {
			int i = read();
			if (i==-1) {
				if (throwOnEOF)
					parseError("Unexpected EOF");
				else
					return false;
			}
			_fScan = (char) i;
		} catch (IOException e) {
			throw new XMLException(e);
		}

		if (XMLSpec.isLineDelimiter(_fScan)) {
			_fLine++;
			_fColumn=1;
		} else
			_fColumn++;

		if (VERBOSE)
			System.out.print(String.valueOf(_fScan));
		return true;
	}

	void _scan_for_all(String expected) throws XMLException
	{
		for (int i=0; i < expected.length(); i++) {
			char expectedChar = expected.charAt(i);
			if (_fScan != expectedChar)
				parseError("Expected '" +  expectedChar + "'");
			scan_char();
		}
	}

	// Eat and discard whitespace
	private void _skip_whitespace() throws XMLException
	{
		while ( XMLSpec.isWhitespace(_fScan) )
			scan_char();
	}

	// Primitive: Scan a name, current scan position is the first char
	private String _scan_name() throws XMLException
	{
		assertTrue( XMLSpec.isNameStartChar(_fScan), "Expected beginning of a name" );
		XMLStringBuffer buffer = new XMLStringBuffer();

		do {
			buffer.append(_fScan);
			scan_char();
		} while (XMLSpec.isNameChar(_fScan));

		return buffer.toString();
	}

	// Primitive: Scan escaped character reference
	// We have read a &
	private String _scan_escaped_char() throws XMLException
	{
		assertTrue( _fScan=='&' , "Expected beginning of an escaped character" );

		scan_char();  // advance past the ampersand

		boolean scanningDigits = (_fScan == '#');

		if (scanningDigits)
			scan_char();  // advance past the sharp

		XMLStringBuffer buffer = new XMLStringBuffer();
		while ( XMLSpec.isNameChar(_fScan) ) {
			buffer.append(_fScan);
			scan_char();
		}

		String escape = buffer.toString();

		if (scanningDigits) {
			Integer i = decode(escape);
			return String.valueOf((char) i.intValue());
		} else {
			String equivalent = XMLSpec.namedEscapeToString(escape);
			assertTrue( equivalent!= null, "Unrecognized escape -->" + escape);
			return equivalent;
		}
	}

	int read() throws IOException
	{
		if (_fbuf==null) throw new IOException();
		/* Are there buffered bytes available? */
		if (_fpos >= _fcount && fillbuf() == -1) return -1; /* no, fill buffer */

		/* Did filling the buffer fail with -1 (EOF)? */
		if (_fcount - _fpos <= 0) return -1;
		return _fbuf[_fpos++] & 0xFF;
	}

	private int fillbuf() throws IOException
	{
		_fpos = 0;
		int result = _fInput.read(_fbuf);
		_fcount = result == -1 ? 0:result;
		return result;
	}

	Integer decode(String string) throws NumberFormatException
	{
		int length = string.length(), i = 0;
		if (length == 0) throw new NumberFormatException();
		char firstDigit = string.charAt(i++);
		boolean negative = firstDigit == '-';
		if (negative) {
			if (length == 1) throw new NumberFormatException(string);
			firstDigit = string.charAt(i++);
		}

		int base = 10;
		if (firstDigit == '0') {
			if (i == length) return new Integer(0);
			if ((firstDigit = string.charAt(i++)) == 'x' || firstDigit == 'X') {
				if (i == length) throw new NumberFormatException(string);
				firstDigit = string.charAt(i++);
				base = 16;
			} else {
				base = 8;
			}
		} else if (firstDigit == '#') {
			if (i == length) throw new NumberFormatException(string);
			firstDigit = string.charAt(i++);
			base = 16;
		}

		int result = Character.digit(firstDigit, base);
		if (result == -1) throw new NumberFormatException(string);
		result = -result;
		while (i < length) {
			int digit = Character.digit(string.charAt(i++), base);
			if (digit == -1) throw new NumberFormatException(string);
			int next = result * base - digit;
			if (next > result) throw new NumberFormatException(string);
			result = next;
		}
		if (!negative) {
			result = -result;
			if (result < 0) throw new NumberFormatException(string);
		}
		return new Integer(result);
	}

	// Primitive: Scan character data
	private boolean _scan_cdata_or_eof() throws XMLException
	{
		XMLStringBuffer buffer = new XMLStringBuffer();

		while (_fScan != '<') {
			switch (_fScan) {
				case '&':
					buffer.append(_scan_escaped_char());
					break;

				default:
					if (!XMLSpec.isWhitespace(_fScan))
						buffer.append(_fScan);
					else if (XMLSpec.isLineDelimiter(_fScan))
						buffer.append(" ");  // convert to a regular space
					else
						buffer.append(" ");  // convert to a regular space
					break;
			}

			if (!scan_char(!THROW_ON_EOF)) {
				// Failed to read
				if (_fLevel==0)
					return EOF_ENCOUNTERED;
				else
					parseError("Character data ended prematurely");
			}
		}

		_fDocumentHandler.xmlCharacters(buffer.toString());
		return !EOF_ENCOUNTERED;
	}

	// Primitive: Scan a name, current scan position is the open quote
	private String _scan_attribute_value() throws XMLException
	{
		assertTrue( _fScan == '"', "Expected quoted attribute value" );
		scan_char();

		XMLStringBuffer buffer = new XMLStringBuffer();

		do {
			buffer.append(_fScan);
			scan_char();
		} while ( _fScan != '"' );

		// Advance past the final quote
		scan_char();

		return buffer.toString();
	}

	// Scan attributes into a hashtable
	private Hashtable _scan_attributes() throws XMLException
	{
		Hashtable attributes = new Hashtable();

		_skip_whitespace();

		while ( XMLSpec.isNameStartChar(_fScan) ) {
			String key = _scan_name();
			_scan_for_all("=");
			String val = _scan_attribute_value();
			_skip_whitespace();

			// Store the key-value pair in the hashtable
			attributes.put(key,val);
		}

		return attributes;
	}

	// Perform very cursory verification
	private void _scan_xml_header() throws XMLException
	{
		_scan_for_all("<?xml");
		Hashtable attributes = _scan_attributes();

		String encoding = (String) attributes.get("encoding");
		assertTrue( "UTF-8".equals(encoding), "Unsupported encoding" );
		_scan_for_all("?>");

		// Notify the content handler
		_fDocumentHandler.xmlStartDocument();
	}

	// <? already scanned, continue ==> <?xml:stylesheet type="text/xsl" href="excludes.xsl" ?>
	private void _scan_processing_instruction() throws XMLException
	{
		scan_char();  // advance past '?'
		String directiveName = _scan_name();
		_skip_whitespace();
		Hashtable attributes = _scan_attributes();

		while (_fScan != '>') scan_char();

	}

	// <!- already scanned, continue
	private void _scan_comment() throws XMLException
	{
		_scan_for_all("-"); // get the second '-'

		XMLStringBuffer comment = new XMLStringBuffer();
		while (true) {
			scan_char();
			comment.append(_fScan);
			if (comment.endsWith("-->"))
				break;
		}
		return;
	}

	// <!D already scanned, continue
	// '<!DOCTYPE' S Name (S ExternalID)? S? ('[' (markupdecl | DeclSep)* ']' S?)? '>'
	private void _scan_doctype() throws XMLException
	{
		_scan_for_all("DOCTYPE");
		_skip_whitespace();
		String name = _scan_name();

		while (_fScan != '>') scan_char();
		return;
	}

	// <! already scanned, continue
	private void _scan_doctype_or_comment() throws XMLException
	{
		scan_char();  // advance past '!'

		switch (_fScan) {
			case 'D':
				_scan_doctype();
				return;

			case '-':
				_scan_comment();
				return;

			default:
				parseError("Unknown <! type -->" + _fScan);
		}
	}

	// </ already scanned,
	private void _scan_element_close() throws XMLException
	{
		scan_char();  // advance past '/'
		String closedTag = _scan_name();
		while (_fScan != '>')
			scan_char();

		// Notify the content handler
		_fDocumentHandler.xmlEndElement(closedTag);
		_fLevel--;
	}

	// Perform very cursory verification
	private void _scan_element_or_instruction() throws XMLException
	{
		assertTrue( _fScan == '<' , "Expected entity or directive" );
		scan_char();

		if (_fScan == '?') {
			_scan_processing_instruction();
			return;
		}

		if (_fScan == '!') {
			_scan_doctype_or_comment();
			return;
		}

		if (_fScan == '/') {
			_scan_element_close();
			return;
		}

		String elementName = _scan_name();
		Hashtable attributes = _scan_attributes();

		// Notify the content handler
		_fDocumentHandler.xmlStartElement(elementName, attributes);
		_fLevel++;

		_skip_whitespace();
		if (_fScan == '/') {
			// Special case for empty elements
			_fDocumentHandler.xmlEndElement(elementName);
			_fLevel--;
		}

		while (_fScan != '>') scan_char();
		scan_char(); // eat the element close

	}

	boolean checkForCompletion() throws XMLException
	{
		while (true) {
			if (!scan_char(!THROW_ON_EOF))
				return false; // out of data

			if (!XMLSpec.isWhitespace(_fScan))
				return true;
		}
	}

	void parseXML() throws XMLException, IOException
	{
		_fLevel = 0;

		int line =0;
		int column =0;

		// Fetch the first character
		scan_char();

		// Consume any leading whitespace
		_skip_whitespace();
		_scan_xml_header();

		// Begin scanning body
		while (true) {
			if (_fScan == '<')
				_scan_element_or_instruction();
			else
				if (_scan_cdata_or_eof() == EOF_ENCOUNTERED)
					break;
		}

		// Notify the content handler
		_fDocumentHandler.xmlEndDocument();
	}

	public boolean parse(String uri, IXMLDocumentHandler handler) {
		InputStream uriStream = null;
		try {
			uriStream = new FileInputStream(uri);
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
		return parse( uriStream, handler );
	}

	public boolean parse(InputStream uriStream, IXMLDocumentHandler handler)
	{
		_fSuccess = false;
		_fDocumentHandler = handler;

		try {
			_fInput = new InputStreamReader(uriStream);
			long before = System.currentTimeMillis();
			parseXML();
			long after = System.currentTimeMillis();
			_fElapsedTime = after - before;
		}
		catch (Exception genericException)
		{
			genericException.printStackTrace();
        	}

        	return _fSuccess;
	}


}
