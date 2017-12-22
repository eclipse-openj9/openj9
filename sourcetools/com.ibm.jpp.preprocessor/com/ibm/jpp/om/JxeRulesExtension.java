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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Properties;
import java.util.Set;
import java.util.TreeSet;

/**
 * JXE Rules Extension
 */
public class JxeRulesExtension extends BuilderExtension {

	/**
	 * The JXE rules found during the build.
	 */
	private final Set<String> jxeRules = new TreeSet<>();

	/**
	 * Determines whether API classes should be preserved with jxe rules.
	 */
	private boolean jxePreserveApi = false;

	/**
	 * The output path.
	 */
	private String jxeRulesPath = null;

	/**
	 * The output filename.
	 */
	private static final String jxeRulesFilename = "jxeLink.rules";

	/**
	 * Constructor for JxeRuleExtension.
	 */
	public JxeRulesExtension() {
		super("jxerules");
	}

	/**
	 * @see         com.ibm.jpp.om.BuilderExtension#notifyBuildEnd()
	 */
	@Override
	public void notifyBuildEnd() {
		if (jxeRules.size() > 0) {
			if (this.jxeRulesPath != null) {
				// add the generated jxerules to the file
				File outputFile = getOutputRulesFile();
				File parentDir = outputFile.getParentFile();

				parentDir.mkdirs();

				if (parentDir.exists()) {
					try (FileOutputStream fout = new FileOutputStream(outputFile, true);
						 PrintWriter writer = new PrintWriter(fout)) {
						for (String jxeRule : jxeRules) {
							writer.println(jxeRule);
						}
						writer.flush();
						builder.getLogger().log("Jxelink rules written to \"" + outputFile.getAbsolutePath() + "\"", Logger.SEVERITY_INFO);
					} catch (IOException e) {
						builder.getLogger().log("An exception occured while writing the jxelink rules", Logger.SEVERITY_ERROR, e);
					}
				}
			} else {
				builder.getLogger().log("No output path specified for jxe rules", Logger.SEVERITY_ERROR);
			}
		}
	}

	/**
	 * @see         com.ibm.jpp.om.BuilderExtension#notifyConfigurePreprocessor(JavaPreprocessor)
	 */
	@Override
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		preprocessor.setJxeRules(this.jxeRules);
		preprocessor.setJxePreserveApi(this.jxePreserveApi);
	}

	/**
	 * Validates the JXE rule options
	 *
	 * @param       options     jxerules options
	 *
	 * @throws      BuilderConfigurationException   if the options are invalid
	 *
	 * @see         com.ibm.jpp.om.BuilderExtension#validateOptions(Properties)
	 */
	@Override
	public void validateOptions(Properties options) throws BuilderConfigurationException {
		if (options.containsKey("jxerules:preserveapi")) {
			this.jxePreserveApi = options.getProperty("jxerules:preserveapi").equalsIgnoreCase("true");
		}
		if (options.containsKey("jxerules:outputdir")) {
			this.jxeRulesPath = options.getProperty("jxerules:outputdir");
		}
	}

	private File getOutputRulesFile() {
		return new File(new File(builder.getOutputDir(), jxeRulesPath), jxeRulesFilename);
	}

}
