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
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

import com.ibm.j9tools.om.AsmBuilder;
import com.ibm.j9tools.om.BuildSpec;
import com.ibm.j9tools.om.ConfigDirectory;
import com.ibm.j9tools.om.DefaultSizes;
import com.ibm.j9tools.om.Feature;
import com.ibm.j9tools.om.FeatureDefinition;
import com.ibm.j9tools.om.Flag;
import com.ibm.j9tools.om.FlagDefinition;
import com.ibm.j9tools.om.FlagDefinitions;
import com.ibm.j9tools.om.InvalidBuildSpecException;
import com.ibm.j9tools.om.JclConfiguration;
import com.ibm.j9tools.om.ObjectFactory;
import com.ibm.j9tools.om.Owner;
import com.ibm.j9tools.om.Property;
import com.ibm.j9tools.om.Source;

/**
 * Parses a build spec XML definition into a Java object.
 *
 * @author 	Maciek Klimkowski
 * @author 	Gabriel Castro
 */
public class BuildSpecParser extends AbstractParser {
	private final Map<String, FeatureDefinition> featureDefinitions = new HashMap<String, FeatureDefinition>();
	private FlagDefinitions flagDefinitions;

	/** BuildSpec instance to be returned once parsed and initialized */
	private BuildSpec _buildSpec = null;

	private Vector<Owner> _owners;
	/** List of owners */
	private Vector<Feature> _features;
	/** List of features */
	private Vector<Flag> _flags;
	/** List of flags */
	private Vector<Source> _sources;
	/** List of sources */
	private Vector<Property> _properties;

	/** List of properties */

	/**
	 * Parse passed input stream. Expects well formed XML adhering to the specified
	 * schema. This method should not be called directly but rather via {@link ObjectFactory#loadBuildSpec(File)}.
	 *
	 * @param 	input		InputStream containing build spec XML to be parsed
	 * @return	initialized instance of a BuildSpec
	 *
	 * @throws InvalidBuildSpecException
	 */
	public BuildSpec parse(InputStream input, String objectId, FlagDefinitions flagDefinitions, Map<String, FeatureDefinition> featureDefinitions) throws InvalidBuildSpecException, IOException {
		this.featureDefinitions.putAll(featureDefinitions);
		this.flagDefinitions = flagDefinitions;
		_buildSpec = new BuildSpec();

		try {
			_parser.getXMLReader().parse(new InputSource(input));
		} catch (SAXException e) {
			error(new SAXParseException(e.getMessage(), "", "", 0, 0)); //$NON-NLS-1$ //$NON-NLS-2$
		} finally {
			if (hasErrors() || hasWarnings()) {
				throw new InvalidBuildSpecException(getErrors(), getWarnings(), objectId);
			}
		}

		return _buildSpec;
	}

	/**
	 * Parses the specified file. This method should not be called directly but rather via
	 * {@link ObjectFactory#loadBuildSpec(File)}.
	 *
	 * @param 	file 		the file to be loaded
	 * @return 	A new instance of a BuildSpec
	 *
	 * @throws InvalidBuildSpecException 	if an error occured while parsing the build spec contained in <code>file</code>
	 */
	public BuildSpec parse(File file, FlagDefinitions flagDefinitions, Map<String, FeatureDefinition> featureDefinitions) throws InvalidBuildSpecException, IOException {
		setDocumentLocatorFile(file);

		FileInputStream fis = new FileInputStream(file);
		BuildSpec result = parse(fis, file.getName().substring(0, file.getName().length() - ConfigDirectory.BUILD_SPEC_FILE_EXTENSION.length()), flagDefinitions, featureDefinitions);
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

		if (qName.equalsIgnoreCase("spec")) { //$NON-NLS-1$
			/* Reset all lists */
			_owners = new Vector<Owner>();
			_features = new Vector<Feature>();
			_flags = new Vector<Flag>();
			_sources = new Vector<Source>();
			_properties = new Vector<Property>();

			/* Set the build spec id */
			_buildSpec.setId(attributes.getValue("id")); //$NON-NLS-1$

			/* Remember where this build spec was defined */
			_buildSpec.setLocation(_documentLocatorFile, _documentLocator);
		}

		/* Parse owners of the build spec */
		else if (qName.equalsIgnoreCase("owners")) { //$NON-NLS-1$
			_owners = new Vector<Owner>();
		}

		/* Parse flags of the build spec */
		else if (qName.equalsIgnoreCase("flags")) { //$NON-NLS-1$
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

		/* Parse features of the build spec */
		else if (qName.equalsIgnoreCase("features")) { //$NON-NLS-1$
			_features = new Vector<Feature>();
		} else if (qName.equalsIgnoreCase("feature") && _features != null) { //$NON-NLS-1$
			String featureId = attributes.getValue("id"); //$NON-NLS-1$
			FeatureDefinition featureDefinition = featureDefinitions.get(featureId);

			Feature feature = new Feature(featureId);
			feature.setLocation(_documentLocatorFile, _documentLocator);

			if (featureDefinition != null) {
				feature.setDefinition(featureDefinition);
			}

			_features.add(feature);
		}

		/* Parse sources of the build spec */
		else if (qName.equalsIgnoreCase("source")) { //$NON-NLS-1$
			_sources = new Vector<Source>();
		} else if (qName.equalsIgnoreCase("project") && _sources != null) { //$NON-NLS-1$
			Source source = new Source(attributes.getValue("id")); //$NON-NLS-1$
			source.setLocation(_documentLocatorFile, _documentLocator);

			_sources.add(source);
		}

		/* Parse properties of the build spec */
		else if (qName.equalsIgnoreCase("properties")) { //$NON-NLS-1$
			_properties = new Vector<Property>();
		} else if (qName.equalsIgnoreCase("property") && _properties != null) { //$NON-NLS-1$
			Property property = new Property(attributes.getValue("name"), attributes.getValue("value")); //$NON-NLS-1$ //$NON-NLS-2$
			property.setLocation(_documentLocatorFile, _documentLocator);

			_properties.add(property);
		}

	}

	/**
	 * @see org.xml.sax.ContentHandler#endElement(java.lang.String, java.lang.String, java.lang.String)
	 */
	@Override
	public void endElement(java.lang.String uri, java.lang.String localName, java.lang.String qName) {
		if (qName.equalsIgnoreCase("name")) { //$NON-NLS-1$
			_buildSpec.setName(_parsedValue);
		} else if (qName.equalsIgnoreCase("asmBuilderName")) { //$NON-NLS-1$
			AsmBuilder asmBuilder = new AsmBuilder(_parsedValue);
			asmBuilder.setLocation(_documentLocatorFile, _documentLocator);

			_buildSpec.setAsmBuilder(asmBuilder);
		} else if (qName.equalsIgnoreCase("cpuArchitecture")) { //$NON-NLS-1$
			_buildSpec.setCpuArchitecture(_parsedValue);
		} else if (qName.equalsIgnoreCase("os")) { //$NON-NLS-1$
			_buildSpec.setOs(_parsedValue);
		} else if (qName.equalsIgnoreCase("defaultJCL")) { //$NON-NLS-1$
			JclConfiguration jclConfiguration = new JclConfiguration(_parsedValue);
			jclConfiguration.setLocation(_documentLocatorFile, _documentLocator);

			_buildSpec.setDefaultJCL(jclConfiguration);
		} else if (qName.equalsIgnoreCase("defaultSizes")) { //$NON-NLS-1$
			DefaultSizes defaultSizes = new DefaultSizes(_parsedValue);
			defaultSizes.setLocation(_documentLocatorFile, _documentLocator);

			_buildSpec.setDefaultSizes(defaultSizes);
		} else if (qName.equalsIgnoreCase("priority")) { //$NON-NLS-1$
			try {
				_buildSpec.setPriority(Integer.parseInt(_parsedValue));
			} catch (NumberFormatException e) {
				error(new SAXParseException("Invalid priority value, not a valid integer.", _documentLocator)); //$NON-NLS-1$
			}
		}

		else if (qName.equalsIgnoreCase("owner")) { //$NON-NLS-1$
			/* Parsed a single owner, add it to the list of Owners */
			Owner owner = new Owner(_parsedValue);
			owner.setLocation(_documentLocatorFile, _documentLocator);

			_buildSpec.addOwner(owner);
		} else if (qName.equalsIgnoreCase("owners")) { //$NON-NLS-1$
			/* Parsed all owners, add it to the BuildSpec */
			_owners.add(new Owner(_parsedValue));
		}

		/* The <flags> element is being closed. Add all parsed flags to the buildSpec */
		else if (qName.equalsIgnoreCase("flags")) { //$NON-NLS-1$
			for (Flag f : _flags) {
				_buildSpec.addFlag(f);
			}

			// Once all the flags and features have been added fill the list with the remaining
			// flags set to a default value of FALSE
			for (FlagDefinition flagDefinition : flagDefinitions.getFlagDefinitions().values()) {
				if (!_buildSpec.hasFlag(flagDefinition.getId())) {
					_buildSpec.addFlag(new Flag(flagDefinition));
				}
			}
		}

		/* The <features> element is being closed. Add all parsed features to the buildSpec */
		else if (qName.equalsIgnoreCase("features")) { //$NON-NLS-1$
			for (Feature f : _features) {
				/* Save the feature id, it is not our responsibility to load the actual feature here */
				_buildSpec.addFeature(f);
			}
		}

		/* The <sources> element is being closed. Add all parsed sources to the buildSpec */
		else if (qName.equalsIgnoreCase("source")) { //$NON-NLS-1$
			for (Source s : _sources) {
				_buildSpec.addSource(s);
			}
		}

		/* The <properties> element is being closed. Add all parsed properties to the buildSpec */
		else if (qName.equalsIgnoreCase("properties")) { //$NON-NLS-1$
			for (Property p : _properties) {
				_buildSpec.addProperty(p);
			}
		}

	}

	/**
	 * Returns the name of the schema expected from this parser.
	 *
	 * @return 	The name of the schema expected by this parser.
	 */
	@Override
	public String getSchemaName() {
		return "spec-v1.xsd"; //$NON-NLS-1$
	}
}
