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
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.HashSet;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

import com.ibm.j9tools.om.FlagDefinition;
import com.ibm.j9tools.om.FlagDefinitions;
import com.ibm.j9tools.om.InvalidFlagDefinitionsException;
import com.ibm.j9tools.om.InvalidFlagException;

public class FlagDefinitionParser extends AbstractParser {

	/** Used by the SAXParser as an intermediary between start and end element handlers. */
	private FlagDefinition _flag;

	/** List of flag definitions */
	private FlagDefinitions _flags;

	private final Collection<Throwable> flagErrors = new HashSet<Throwable>();

	/**
	 * Parse passed input stream. Expects well formed XML adhering to the specified
	 * schema. This method should not be called directly but rather via the 
	 * FlagDefinitionIO.load method.
	 * 
	 * @param 	input		InputStream containing flag definitions XML to be parsed
	 * @return	a HashMap of FlagDefinitions keyed by Flag ID Strings.
	 * 
	 * @throws InvalidFlagDefinitionsException
	 * @throws IOException 
	 */
	public FlagDefinitions parse(InputStream input) throws InvalidFlagDefinitionsException, IOException {
		_flags = new FlagDefinitions();

		try {
			_parser.getXMLReader().parse(new InputSource(input));
		} catch (SAXException e) {
			error(new SAXParseException(e.getMessage(), "", "", 0, 0)); //$NON-NLS-1$//$NON-NLS-2$
		} finally {
			if (hasErrors() || hasWarnings()) {
				throw new InvalidFlagDefinitionsException(getErrors(), getWarnings(), "flags"); //$NON-NLS-1$
			} else if (flagErrors.size() > 0) {
				throw new InvalidFlagDefinitionsException(flagErrors, _flags);
			}
		}

		return _flags;
	}

	/**
	 * Parses the specified file.
	 * 
	 * @param 	file		file to be parsed
	 * @return	the flag definitions found in the given file
	 * 
	 * @throws InvalidFlagDefinitionsException
	 * @throws IOException 
	 */
	public FlagDefinitions parse(File file) throws InvalidFlagDefinitionsException, IOException {
		setDocumentLocatorFile(file);

		FileInputStream fis = new FileInputStream(file);
		FlagDefinitions result = parse(fis);
		fis.close();

		return result;
	}

	/**
	 * @see org.xml.sax.ContentHandler#startElement(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	@Override
	public void startElement(java.lang.String uri, java.lang.String localName, java.lang.String qName, Attributes attributes) {
		/* We are starting to parse a new element, reset the parsed value */
		_parsedValue = ""; //$NON-NLS-1$

		if (qName.equalsIgnoreCase("flags")) { //$NON-NLS-1$
			/* Remember where this build spec was defined */
			_flags.setLocation(_documentLocatorFile, _documentLocator);
		}

		if (qName.equalsIgnoreCase("flag") && _flags != null) { //$NON-NLS-1$
			String id = attributes.getValue("id"); //$NON-NLS-1$
			_flag = _flags.getFlagDefinition(id);

			if (_flag == null) {
				_flag = new FlagDefinition(id, "", ""); //$NON-NLS-1$ //$NON-NLS-2$
				_flag.setLocation(_documentLocatorFile, _documentLocator);
			} else if (_flag.isComplete()) {
				InvalidFlagException error = new InvalidFlagException("Flag definition has already been defined.", _flag); //$NON-NLS-1$
				error.setLineNumber(_documentLocator.getLineNumber());
				error.setColumn(_documentLocator.getColumnNumber());

				if (_documentLocatorFile != null) {
					error.setFileName(_documentLocatorFile.getAbsolutePath());
				}

				flagErrors.add(error);

				_flag = null;
			}
		}

		/* Parse the required flags list */
		else if (qName.equalsIgnoreCase("require") && _flag != null) { //$NON-NLS-1$
			String flagId = attributes.getValue("flag"); //$NON-NLS-1$

			if (flagId.equalsIgnoreCase(_flag.getId())) {
				_flag.addRequires(_flag);
			} else {
				FlagDefinition flagDef = _flags.getFlagDefinition(flagId);

				if (flagDef == null) {
					flagDef = new FlagDefinition(flagId, "", ""); //$NON-NLS-1$ //$NON-NLS-2$
					_flags.addFlagDefinition(flagDef);
				}

				_flag.addRequires(flagDef);
			}

		}

		/* Parse the precludes flag list */
		else if (qName.equalsIgnoreCase("preclude") && _flag != null) { //$NON-NLS-1$
			String flagId = attributes.getValue("flag"); //$NON-NLS-1$

			if (flagId.equalsIgnoreCase(_flag.getId())) {
				_flag.addPrecludes(_flag);
			} else {
				FlagDefinition flagDef = _flags.getFlagDefinition(flagId);

				if (flagDef == null) {
					flagDef = new FlagDefinition(flagId, "", ""); //$NON-NLS-1$ //$NON-NLS-2$
					_flags.addFlagDefinition(flagDef);
				}

				_flag.addPrecludes(flagDef);
			}

		}

		return;
	}

	/**
	 * @see org.xml.sax.ContentHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
	 */
	@Override
	public void endElement(java.lang.String uri, java.lang.String localName, java.lang.String qName) {
		if (qName.equalsIgnoreCase("description") && !_flag.isComplete()) { //$NON-NLS-1$
			_flag.setDescription(_parsedValue);
		} else if (qName.equalsIgnoreCase("ifRemoved") && !_flag.isComplete()) { //$NON-NLS-1$
			_flag.setIfRemoved(_parsedValue);
		} else if (qName.equalsIgnoreCase("flag") && _flag != null) { //$NON-NLS-1$
			_flag.setComplete(true);
			_flags.addFlagDefinition(_flag);
		}

	}

	/**
	 * @return The name of the schema expected by this parser.
	 */
	@Override
	public String getSchemaName() {
		return "flags-v1.xsd"; //$NON-NLS-1$
	}

}
