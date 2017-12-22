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
import java.util.Vector;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

import com.ibm.j9tools.om.ConfigDirectory;
import com.ibm.j9tools.om.FeatureDefinition;
import com.ibm.j9tools.om.Flag;
import com.ibm.j9tools.om.FlagDefinition;
import com.ibm.j9tools.om.FlagDefinitions;
import com.ibm.j9tools.om.InvalidFeatureDefinitionException;
import com.ibm.j9tools.om.Property;
import com.ibm.j9tools.om.Source;

/**
 * Parses a feature file given an input stream. An initialized instance of Feature
 * is returned. The feature xml must adhere to the feature schema. 
 * 
 * @author mac
 */
public class FeatureParser extends AbstractParser {
	private FlagDefinitions flagDefinitions;

	private Vector<Property> _properties; // List of properties
	private Vector<Flag> _flags; // List of flags
	private Vector<Source> _sources; // List of sources

	private FeatureDefinition _feature = null; // Feature instance to be returned once parsed and initialized

	/**
	 * Parse passed input stream. Expects well formed XML adhering to the specified
	 * schema. This method should not be called directly but rather via the FeatureIO.load
	 * method.
	 * 
	 * @param 	input		InputStream containing build spec XML to be parsed
	 * @return	initialized instance of a Feature
	 * 
	 * @throws 			InvalidFeatureDefinitionException
	 */
	public FeatureDefinition parse(InputStream input, String objectId, FlagDefinitions flagDefinitions) throws InvalidFeatureDefinitionException, IOException {
		this.flagDefinitions = flagDefinitions;
		this._feature = new FeatureDefinition();

		try {
			_parser.getXMLReader().parse(new InputSource(input));
		} catch (SAXException e) {
			error(new SAXParseException(e.getMessage(), "", "", 0, 0)); //$NON-NLS-1$ //$NON-NLS-2$
		} finally {
			if (hasErrors() || hasWarnings()) {
				throw new InvalidFeatureDefinitionException(getErrors(), getWarnings(), objectId);
			}
		}

		return _feature;
	}

	/**
	 * Parses the specified file.
	 * 
	 * @param 	file	the feature file to be parsed
	 * @return	the parsed feature
	 * 
	 * @throws InvalidFeatureDefinitionException
	 */
	public FeatureDefinition parse(File file, FlagDefinitions flagDefinitions) throws InvalidFeatureDefinitionException, IOException {
		setDocumentLocatorFile(file);

		FileInputStream fis = new FileInputStream(file);
		FeatureDefinition result = parse(fis, file.getName().substring(0, file.getName().length() - ConfigDirectory.FEATURE_FILE_EXTENSION.length()), flagDefinitions);
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

		if (qName.equalsIgnoreCase("feature")) { //$NON-NLS-1$
			/* Reset the flags list */
			_flags = new Vector<Flag>();

			/* Set the build spec id */
			_feature.setId(attributes.getValue("id")); //$NON-NLS-1$
			_feature.setLocation(_documentLocatorFile, _documentLocator);
		}

		else if (qName.equalsIgnoreCase("properties")) { //$NON-NLS-1$
			_properties = new Vector<Property>();
		} else if (qName.equalsIgnoreCase("requiredProperty") && _properties != null) { //$NON-NLS-1$
			Property property = new Property(attributes.getValue("name"), null); //$NON-NLS-1$
			property.setLocation(_documentLocatorFile, _documentLocator);

			_properties.add(property);
		}

		/* Parse sources of the feature */
		else if (qName.equalsIgnoreCase("source")) { //$NON-NLS-1$
			_sources = new Vector<Source>();
		} else if (qName.equalsIgnoreCase("project") && _sources != null) { //$NON-NLS-1$
			Source source = new Source(attributes.getValue("id")); //$NON-NLS-1$
			source.setLocation(_documentLocatorFile, _documentLocator);

			_sources.add(source);
		}

		/* Parse flags of the feature */
		else if (qName.equalsIgnoreCase("flags")) { //$NON-NLS-1$
			/* Reset the flags list */
			_flags = new Vector<Flag>();
		} else if (qName.equalsIgnoreCase("flag") && _flags != null) { //$NON-NLS-1$
			String flagName = attributes.getValue("id"); //$NON-NLS-1$
			Boolean flagState = Boolean.valueOf(attributes.getValue("value")); //$NON-NLS-1$
			FlagDefinition flagDefinition = flagDefinitions.getFlagDefinition(flagName);

			Flag flag = new Flag(flagName, flagState);
			flag.setLocation(_documentLocatorFile, _documentLocator);

			if (flagDefinition != null) {
				flag.setDefinition(flagDefinition);
			}

			_flags.add(flag);
		}
	}

	/**
	 * @see org.xml.sax.ContentHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
	 */
	@Override
	public void endElement(java.lang.String uri, java.lang.String localName, java.lang.String qName) {
		if (qName.equalsIgnoreCase("id")) { //$NON-NLS-1$
			_feature.setId(_parsedValue);
		} else if (qName.equalsIgnoreCase("name")) { //$NON-NLS-1$
			_feature.setName(_parsedValue);
		} else if (qName.equalsIgnoreCase("description")) { //$NON-NLS-1$
			_feature.setDescription(_parsedValue);
		} else if (qName.equalsIgnoreCase("properties")) { //$NON-NLS-1$
			for (Property p : _properties) {
				_feature.addProperty(p);
			}
		} else if (qName.equalsIgnoreCase("source")) { //$NON-NLS-1$
			for (Source s : _sources) {
				_feature.addSource(s);
			}
		} else if (qName.equalsIgnoreCase("flags")) { //$NON-NLS-1$
			for (Flag f : _flags) {
				_feature.addFlag(f);
			}
		}
	}

	/** 
	 * @return 	The name of the schema expected by this parser.
	 */
	@Override
	public String getSchemaName() {
		return "feature-v2.xsd"; //$NON-NLS-1$
	}
}
