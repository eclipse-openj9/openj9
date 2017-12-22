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
import java.util.Map;
import java.util.Properties;
import java.util.StringTokenizer;

/**
 * Macro Extension
 */
public class MacroExtension extends BuilderExtension {

	/**
	 * The macros to use for this build.
	 */
	private final Map<String, String> macros = new HashMap<>();

	/**
	 * Constructor for MacroExtension.
	 */
	public MacroExtension() {
		super("macro");
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#validateOptions(Properties)
	 */
	@Override
	public void validateOptions(Properties options) {
		String arg = options.getProperty("macro:define");

		if (arg != null) {
			StringTokenizer tokenizer = new StringTokenizer(arg, ";");

			while (tokenizer.hasMoreTokens()) {
				String token = tokenizer.nextToken();
				int posEquals = token.indexOf('=');

				if (posEquals > 0 && posEquals < (token.length() - 1)) {
					String identifier = token.substring(0, posEquals);
					String replacement = token.substring(posEquals + 1);

					macros.put(identifier, replacement);
				}
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
			loadProperties(macros, new File(builder.getSourceDir(), "macros.properties"));
		} catch (FileNotFoundException e) {
			// do nothing
		} catch (IOException e) {
			builder.getLogger().log("An exception occured while loading macros", Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyConfigurePreprocessor(JavaPreprocessor)
	 */
	@Override
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		preprocessor.setMacros(this.macros);
	}

}
