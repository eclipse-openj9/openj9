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
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

import com.ibm.j9tools.om.BuildInfo;
import com.ibm.j9tools.om.InvalidBuildInfoException;

public class BuildInfoParser extends AbstractParser {

	/** Build Information */
	private BuildInfo _buildInfo;

	/**
	 * Parse passed input stream. Expects well formed XML adhering to the specified
	 * schema. This method should not be called directly but rather via the 
	 * FlagDefinitionIO.load method.
	 * 
	 * @param 	input		InputStream containing flag definitions XML to be parsed
	 * @return	a HashMap of FlagDefinitions keyed by Flag ID Strings.
	 * 
	 * @throws InvalidBuildInfoException
	 */
	public BuildInfo parse(InputStream input) throws InvalidBuildInfoException, IOException {
		_buildInfo = new BuildInfo();

		try {
			_parser.getXMLReader().parse(new InputSource(input));
		} catch (SAXException e) {
			error(new SAXParseException(e.getMessage(), "", "", 0, 0)); //$NON-NLS-1$ //$NON-NLS-2$
		} finally {
			if (hasErrors() || hasWarnings()) {
				throw new InvalidBuildInfoException(getErrors(), getWarnings(), "buildInfo"); //$NON-NLS-1$
			}
		}

		return _buildInfo;
	}

	/**
	 * Parses the specified file.
	 * 
	 * @param 	file		file to be parsed
	 * @return	the flag definitions found in the given file
	 * 
	 * @throws InvalidBuildInfoException
	 */
	public BuildInfo parse(File file) throws InvalidBuildInfoException, IOException {
		setDocumentLocatorFile(file);

		FileInputStream fis = new FileInputStream(file);
		BuildInfo result = parse(fis);
		fis.close();

		return result;
	}

	/**
	 * @see org.xml.sax.ContentHandler#startElement(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	@Override
	@SuppressWarnings("nls")
	public void startElement(String uri, String localName, String qName, Attributes attributes) throws SAXException {
		/* We are starting to parse a new element, reset the parsed value */
		_parsedValue = "";

		if (qName.equalsIgnoreCase("build")) {
			_buildInfo = new BuildInfo();

			/* Remember where this build spec was defined */
			_buildInfo.setLocation(_documentLocatorFile, _documentLocator);

			String schemaSource = _parser.getProperty(AbstractParser.JAXP_SCHEMA_SOURCE).toString();
			int schemaVersion = Integer.parseInt(schemaSource.substring(schemaSource.length() - 5, schemaSource.length() - 4));

			if (schemaVersion >= 4) {
				_buildInfo.setValidateDefaultSizes(true);
			}
		} else if (qName.equalsIgnoreCase("product")) {
			_buildInfo.setProductName(attributes.getValue("name"));
			_buildInfo.setProductRelease(attributes.getValue("release"));
		} else if (qName.equalsIgnoreCase("repositoryBranch")) {
			_buildInfo.addRepositoryBranch(attributes.getValue("id"), attributes.getValue("name"));
		} else if (qName.equalsIgnoreCase("fsroot")) {
			_buildInfo.addFSRoot(attributes.getValue("id"), attributes.getValue("root"));
		} else if (qName.equalsIgnoreCase("defaultSize")) {
			_buildInfo.addDefaultSize(attributes.getValue("id"), attributes.getValue("value"));
		} else if (qName.equalsIgnoreCase("jcl")) {
			_buildInfo.addJCL(attributes.getValue("id"));
		} else if (qName.equalsIgnoreCase("project")) {
			_buildInfo.addSource(attributes.getValue("id"));
		} else if (qName.equalsIgnoreCase("builder")) {
			_buildInfo.addASMBuilder(attributes.getValue("id"));
		}
	}

	/**
	 * @see org.xml.sax.ContentHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
	 */
	@Override
	public void endElement(String uri, String localName, String qName) {
		if (qName.equalsIgnoreCase("major")) { //$NON-NLS-1$
			_buildInfo.setMajorVersion(_parsedValue);
		} else if (qName.equalsIgnoreCase("minor")) { //$NON-NLS-1$
			_buildInfo.setMinorVersion(_parsedValue);
		} else if (qName.equalsIgnoreCase("prefix")) { //$NON-NLS-1$
			_buildInfo.setPrefix(_parsedValue);
		} else if (qName.equalsIgnoreCase("parentStream")) { //$NON-NLS-1$
			_buildInfo.setParentStream(_parsedValue);
		} else if (qName.equalsIgnoreCase("streamSplitDate")) { //$NON-NLS-1$
			try {
				SimpleDateFormat formater = new SimpleDateFormat(BuildInfo.DATE_FORMAT_PATTERN);
				StringBuilder sb = new StringBuilder();

				// Switch sign since Etc/GMT notation is inversed GMT-0500 is TimeZone of ID = Etc/GMT+5
				char sign = _parsedValue.charAt(_parsedValue.length() - 6);
				if (sign == '-') {
					sb.append("+"); //$NON-NLS-1$
				} else {
					sb.append("-"); //$NON-NLS-1$
				}

				if (_parsedValue.charAt(_parsedValue.length() - 5) == '0') {
					sb.append(_parsedValue.charAt(_parsedValue.length() - 4));
				} else {
					sb.append(_parsedValue.substring(_parsedValue.length() - 5, _parsedValue.length() - 4));
				}

				// Get generic timezone by ID
				TimeZone tz = TimeZone.getTimeZone("Etc/GMT" + sb.toString()); //$NON-NLS-1$
				// Set formater timezone to ensure date is not converted to local time
				formater.setTimeZone(tz);

				// Create calendar date
				Date date = formater.parse(_parsedValue);
				Calendar calendar = Calendar.getInstance(tz);
				calendar.setTime(date);

				_buildInfo.setStreamSplitDate(calendar);
			} catch (ParseException e) {
				// No need to throw error because XML schema already validates date format
			}
		} else if (qName.equalsIgnoreCase("runtime")) { //$NON-NLS-1$
			_buildInfo.setRuntime(_parsedValue);
		}
	}

	/**
	 * @return The name of the schema expected by this parser.
	 */
	@Override
	public String getSchemaName() {
		return "build-v6.xsd"; //$NON-NLS-1$
	}

}
