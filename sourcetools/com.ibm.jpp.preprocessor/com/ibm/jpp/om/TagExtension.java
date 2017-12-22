/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.StringTokenizer;

/**
 * Tag Extension
 */
public class TagExtension extends BuilderExtension {

	/**
	 * The macros to use for this build.
	 */
	private final Set<String> addTags = new HashSet<>();
	private final Set<String> removeTags = new HashSet<>();

	/**
	 * Constructor for MacroExtension.
	 */
	public TagExtension() {
		super("tag");
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#validateOptions(Properties)
	 */
	@Override
	public void validateOptions(Properties options) {
		collectTags(options.getProperty("tag:define"), addTags);
		collectTags(options.getProperty("tag:remove"), removeTags);
	}

	private static void collectTags(String tags, Set<String> tagSet) {
		if (tags != null) {
			StringTokenizer tokenizer = new StringTokenizer(tags, ";");

			while (tokenizer.hasMoreTokens()) {
				String token = tokenizer.nextToken();

				tagSet.add(token);
			}
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyBuildBegin()
	 */
	@Override
	public void notifyBuildBegin() {
		// try loading the macros from the root of the sources
		try {
			Map<String, String> tagProperties = new HashMap<>();

			loadProperties(tagProperties, new File(builder.getSourceDir(), "tags.properties"));

			addTags.addAll(tagProperties.keySet());
		} catch (FileNotFoundException e) {
			// do nothing
		} catch (IOException e) {
			builder.getLogger().log("An exception occured while loading tags", Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyConfigurePreprocessor(JavaPreprocessor)
	 */
	@Override
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		Set<String> tags = new HashSet<>(addTags);
		tags.removeAll(removeTags);
		preprocessor.addFlags(tags);
	}

}
