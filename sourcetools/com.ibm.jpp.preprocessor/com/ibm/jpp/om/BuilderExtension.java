/*******************************************************************************
 * Copyright (c) 1999, 2019 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;

/**
 * Represents the generalization of a Preprocessor builder extension.
 */
public abstract class BuilderExtension {

	public static void loadProperties(Map<String, String> propertyMap, File propertiesFile) throws IOException {
		try (FileInputStream input = new FileInputStream(propertiesFile)) {
			Properties properties = new Properties();

			properties.load(input);

			for (Entry<?, ?> entry : properties.entrySet()) {
				propertyMap.put((String) entry.getKey(), (String) entry.getValue());
			}
		}
	}

	private final String name;
	protected Builder builder;

	protected BuilderExtension(String name) {
		if (name == null) {
			throw new NullPointerException();
		}
		this.name = name;
	}

	/**
	 * Returns this extension's name.
	 *
	 * @return      the extension name
	 */
	public String getName() {
		return this.name;
	}

	/**
	 * Sets this extension's builder
	 *
	 * @param       builder     the builder
	 */
	public void setBuilder(Builder builder) {
		this.builder = builder;
	}

	/**
	 * Validates the build options.
	 *
	 * @param       options     the options to validate
	 *
	 * @throws      BuilderConfigurationException
	 */
	public void validateOptions(Properties options) throws BuilderConfigurationException {
		// nop
	}

	/**
	 * Determines if the source file should be built as part of the preprocess job.
	 *
	 * @param       sourceFile      the source file
	 * @param       outputFile      the destination for the preprocessed file
	 * @param       relativePath    the files' relative path
	 * @return      <code>true</code> if the file should be built, <code>false</code> otherwise
	 */
	public boolean shouldBuild(File sourceFile, File outputFile, String relativePath) {
		return true;
	}

	/*[PR 118220] Incremental builder is not called when file is deleted in base library*/
	/**
	 * Determines if the source file should be deleted as part of the preprocess job.
	 *
	 * @param       sourceDir       the source directory
	 * @return      returns vector of deleted files
	 */
	public List<String> getDeleteFiles(File sourceDir) {
		return null;
	}

	/**
	 * gets changed resources from IcrementalFilterExtension
	 *
	 * @param       sourceDir       the source directory
	 * @return      returns vector of changed resources
	 */
	/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
	public List<String> getBuildResources(File sourceDir) {
		return null;
	}

	/**
	 * Notifies listeners that the preprocess job has begun.
	 */
	public void notifyBuildBegin() {
		// nop    	
	}

	/**
	 * Notifies listeners that the preprocess job has ended.
	 */
	public void notifyBuildEnd() {
		// nop    	
	}

	/**
	 * Notifies listeners that sourceFile is being built.
	 *
	 * @param       sourceFile      the source file
	 * @param       outputFile      the destination for the preprocessed file
	 * @param       relativePath    the files' relative path
	 */
	public void notifyBuildFileBegin(File sourceFile, File outputFile, String relativePath) {
		// nop    	
	}

	/**
	 * Notifies listeners that that sourceFile has been built.
	 *
	 * @param       sourceFile      the source file
	 * @param       outputFile      the destination for the preprocessed file
	 * @param       relativePath    the files' relative path
	 */
	public void notifyBuildFileEnd(File sourceFile, File outputFile, String relativePath) {
		// nop    	
	}

	/**
	 * Notifies that the preprocessor is being configured.
	 *
	 * @param       preprocessor    the preprocessor to be used
	 */
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		// nop    	
	}

}
