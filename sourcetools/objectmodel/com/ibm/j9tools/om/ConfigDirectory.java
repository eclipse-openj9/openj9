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
package com.ibm.j9tools.om;

import java.io.File;
import java.io.FileNotFoundException;
import java.text.MessageFormat;

import com.ibm.j9tools.om.io.FileExtensionFilter;

/**
 * The ConfigDirectory class keeps track of various files (or rather full filenames)
 * found in the a configuration directory. This includes all the Build Spec, 
 * Feature, Build Info, Flag Definition files. Use this class in order to find
 * files describing desired build spec or a feature XML etc.  
 */
public class ConfigDirectory {

	public static final String FEATURE_FILE_EXTENSION = ".feature"; //$NON-NLS-1$
	public static final String BUILD_SPEC_FILE_EXTENSION = ".spec"; //$NON-NLS-1$
	public static final String FLAG_DEFINITIONS_FILE_EXTENSION = ".flags"; //$NON-NLS-1$
	public static final String BUILD_INFO_FILE_EXTENSION = ".build-info"; //$NON-NLS-1$

	public static final String BUILD_SPEC_SCHEMA_FILENAME = "spec-v1.xsd"; //$NON-NLS-1$
	public static final String FEATURE_SCHEMA_FILENAME = "feature-v1.xsd"; //$NON-NLS-1$
	public static final String FLAGS_SCHEMA_FILENAME = "flags-v1.xsd"; //$NON-NLS-1$
	public static final String BUILD_INFO_SCHEMA_FILENAME = "build-v4.xsd"; //$NON-NLS-1$

	public String configDir = null; /* Configuration file directory */
	private final File[] featureFiles; /* A list of feature files */
	private final File[] buildSpecFiles; /* A list of build spec files */
	private final File[] flagDefinitionFiles; /* A list of flag definition files. */
	private final File[] buildInformationFiles; /* A list of build information files. */
	private final File flagDefinitionsFile; /* The singleton flag definitions file. */
	private final File buildInfoFile; /* The singleton build information file. */

	/**
	 * Creates a new ConfigurationDirectory object on the directory called <code>directoryName</code>.
	 * 
	 * @param 	directoryName 	The relative or absolute path to scan.
	 * 
	 * @throws InvalidConfigurationException 
	 */
	public ConfigDirectory(String directoryName) throws InvalidConfigurationException {
		File configDirectory = new File(directoryName);

		if (!configDirectory.exists()) {
			throw new InvalidConfigurationException(MessageFormat.format(Messages.getString("ConfigDirectory.0"), new Object[] { configDirectory.getAbsolutePath() })); //$NON-NLS-1$
		}

		if (!configDirectory.isDirectory()) {
			throw new InvalidConfigurationException(Messages.getString("ConfigDirectory.1")); //$NON-NLS-1$
		}

		// Use absolute paths to simplify debugging
		this.configDir = configDirectory.getAbsolutePath();

		/* Look for and remember full path of each feature file in the config directory */
		this.featureFiles = initializeFileSet(configDirectory, FEATURE_FILE_EXTENSION);

		/* Look for and remember full path of each build spec file in the config directory */
		this.buildSpecFiles = initializeFileSet(configDirectory, BUILD_SPEC_FILE_EXTENSION);

		/* Look for and remember full path of flag definitions file in the config directory */
		this.flagDefinitionFiles = initializeFileSet(configDirectory, FLAG_DEFINITIONS_FILE_EXTENSION);
		int definitionCount = flagDefinitionFiles.length;
		if (definitionCount != 1) {
			throw new InvalidConfigurationException(MessageFormat.format(Messages.getString("ConfigDirectory.2"), new Object[] { definitionCount })); //$NON-NLS-1$
		}

		flagDefinitionsFile = flagDefinitionFiles[0];

		/* Look for and remember full path of build information file in the config directory */
		this.buildInformationFiles = initializeFileSet(configDirectory, BUILD_INFO_FILE_EXTENSION);
		int buildInfoCount = buildInformationFiles.length;
		if (buildInfoCount != 1) {
			throw new InvalidConfigurationException(MessageFormat.format(Messages.getString("ConfigDirectory.3"), new Object[] { buildInfoCount })); //$NON-NLS-1$
		}

		buildInfoFile = buildInformationFiles[0];
	}

	/**
	 * Get a set of feature ids that have been found in the config directory and the user
	 * is allowed to load. 
	 * 
	 * @return	a set of Feature IDs
	 */
	public File[] getFeatureFiles() {
		return featureFiles;
	}

	/**
	 * Retrieves the file for the feature with the given ID.
	 * 
	 * @param 	featureID
	 * @return	the file for the feature with the given ID
	 * 
	 * @throws FileNotFoundException
	 */
	public File getFeatureFileByID(String featureID) throws FileNotFoundException {
		File featureFile = new File(configDir, featureID + FEATURE_FILE_EXTENSION);
		if (featureFile.exists()) {
			throw new FileNotFoundException(MessageFormat.format(Messages.getString("ConfigDirectory.4"), new Object[] { featureFile.getAbsolutePath() })); //$NON-NLS-1$
		}

		if (featureFile.isFile()) {
			throw new FileNotFoundException(MessageFormat.format(Messages.getString("ConfigDirectory.5"), new Object[] { featureFile.getAbsolutePath() })); //$NON-NLS-1$
		}
		return featureFile;
	}

	/**
	 * Get a set of build spec ids that have been found in the config directory and the user
	 * is allowed to load. 
	 * 
	 * @return	a set of BuildSpec IDs
	 */
	public File[] getBuildSpecFiles() {
		return buildSpecFiles;
	}

	/**
	 * Get the flag definitions filename as present in the config directory defined for this class
	 * 
	 * @return	A full filename of the requested flag definitions file, null if not found
	 */
	public File getFlagDefinitionsFile() {
		return flagDefinitionsFile;
	}

	/**
	 * Get the build information filename as present in the config directory defined for this class
	 * 
	 * @return	A full filename of the requested build information file, null if not found
	 */
	public File getBuildInfoFile() {
		return buildInfoFile;
	}

	/**
	 * Find all the files in configDirectory that match the passed extension.
	 * 
	 * @param 	configDirectory		Directory containing build configuration and definition files
	 * @param 	extension			File extensions to be looked for
	 */
	private File[] initializeFileSet(File configDirectory, String extension) {
		FileExtensionFilter ext = new FileExtensionFilter(extension);
		return configDirectory.listFiles(ext);
	}

}
