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
package com.ibm.jpp.commandline;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import com.ibm.jpp.om.ConfigObject;
import com.ibm.jpp.om.ConfigurationRegistry;
import com.ibm.jpp.om.Logger;
import com.ibm.jpp.om.MetaRegistry;
import com.ibm.jpp.xml.XMLException;

/**
 * J9 JCL Preprocessor Command Line Builder
 */
public class CommandlineBuilder {

	/**
	 * Interface to the JPP command line builder
	 *
	 * @param       args        the command line arguments
	 */
	public static void main(String[] args) {
		String xml = null;
		String config = null;
		String baseDir = null;
		String srcRoot = null;
		String destDir = null;
		boolean incremental = false;
		/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
		boolean testsBootpathProject = false;
		/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
		boolean testsProject = false;
		/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
		boolean useTestBootpathJavaDoc = false;

		Map<String, String> options = new HashMap<>();
		ConfigurationRegistry registry;

		if (args.length == 0 || args[0].equalsIgnoreCase("-h") || args[0].equalsIgnoreCase("--help")) {
			Logger logger = new CommandlineLogger();
			logger.log(getArgInfo(), 1);
			System.exit(1);
		}

		for (int i = 0; i < args.length; ++i) {
			String arg = args[i];
			String nextArg = i < args.length - 1 ? args[i + 1] : null;

			if (isSupportedArg(arg)) {
				if (arg.equalsIgnoreCase("-config")) {
					if (nextArg == null) {
						missingValueFor(arg);
					}
					config = nextArg;
					i += 1;
				} else if (arg.equalsIgnoreCase("-baseDir")) {
					if (nextArg == null) {
						missingValueFor(arg);
					}
					baseDir = nextArg;
					i += 1;
				} else if (arg.equalsIgnoreCase("-srcRoot")) {
					if (nextArg == null) {
						missingValueFor(arg);
					}
					srcRoot = nextArg;
					i += 1;
				} else if (arg.equalsIgnoreCase("-dest")) {
					if (nextArg == null) {
						missingValueFor(arg);
					}
					destDir = nextArg;
					i += 1;
				} else if (arg.equalsIgnoreCase("-xml")) {
					if (nextArg == null) {
						missingValueFor(arg);
					}
					xml = nextArg;
					i += 1;
				} else if (arg.equalsIgnoreCase("-incremental")) {
					incremental = true;
					/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
				} else if (arg.equalsIgnoreCase("-isTestsBoot")) {
					testsBootpathProject = true;
					/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
				} else if (arg.equalsIgnoreCase("-isTests")) {
					testsProject = true;
					/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
				} else if (arg.equalsIgnoreCase("-useTestBootpathJavaDoc")) {
					useTestBootpathJavaDoc = true;
				} else {
					String name = arg.substring(1).toLowerCase();
					String value = "true";

					if (nextArg != null && nextArg.charAt(0) != '-') {
						value = nextArg;
						i += 1;
					}

					options.put(name, value);
				}
			} else {
				System.err.println("Unrecognized option: " + arg + "\nCould not preprocess configuration");
				System.exit(1);
			}
		}

		try {
			registry = MetaRegistry.getRegistry(baseDir, srcRoot, xml);
			ConfigObject configuration = registry.getConfiguration(config);

			if (configuration != null) {
				// Overwrites the options defined in the XML with those from the command line
				for (Map.Entry<String, String> entry : options.entrySet()) {
					configuration.addOption(entry.getKey(), entry.getValue());
				}
				/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
				// add bootpath required flag to preprocess for bootpathproject
				if (testsBootpathProject) {
					/* [PR 120411] Use a javadoc tag instead of TestBootpath preprocessor tag */
					/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
					if (!useTestBootpathJavaDoc) {
						configuration.addRequiredIncludeFlag("TestBootpath");
					}
					configuration.addFlag("TestBootpath");
				}
				/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
				// Overwrites the destDir defined in the XML with those from the command line
				configuration.setOutputPathforJCLBuilder(destDir);
				boolean result;
				/* [PR 119500] Design 955 Core.JCL : Support bootpath JCL testing */
				if (testsBootpathProject) {
					// create the output dir even if there is no file to put it in it,
					new File(destDir).mkdirs();
					result = configuration.buildTestBootpath(incremental, options.containsKey("nowarn"), useTestBootpathJavaDoc);
					/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
				} else if (testsProject) {
					// create the output dir even if there is no file to put it in it,
					new File(destDir).mkdirs();
					result = configuration.buildTests(incremental, options.containsKey("nowarn"));
				} else {
					result = configuration.build(incremental, options.containsKey("nowarn"));
				}
				if (result) {
					System.exit(0);
				}
			} else {
				StringBuilder msg = new StringBuilder("No configuration or non-existant configuration specified (Configurations are case sensitive)");
				msg.append("\nPREPROCESS WAS NOT SUCCESSFUL");
				System.err.println(msg.toString());
			}
		} catch (FileNotFoundException e) {
			System.err.println("Cannot open configuration file: " + xml);
		} catch (NullPointerException e) {
			if (destDir == null) {
				System.err.println("No destination directory defined.");
			} else if (xml == null) {
				System.err.println("Could not find configuration files. You must define valid a JPP XML configuration file.");
			} else {
				e.printStackTrace();
			}
		} catch (XMLException e) {
			System.err.println("\nERROR: " + e.getMessage());
		}
		System.exit(1);
	}

	private static void missingValueFor(String arg) {
		System.err.println("Option: " + arg + " requires a value.");
		System.exit(1);
	}

	private static final Set<String> validArgs;

	static {
		validArgs = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);

		validArgs.add("-updateAllCopyrights");
		validArgs.add("-xml");
		validArgs.add("-config");
		validArgs.add("-baseDir");
		validArgs.add("-srcRoot");
		validArgs.add("-dest");
		validArgs.add("-verdict");
		validArgs.add("-includeifunsure");
		validArgs.add("-nowarn");
		validArgs.add("-nowarninvalidflags");
		validArgs.add("-incremental");
		validArgs.add("-force");
		validArgs.add("-macro:define");
		validArgs.add("-tag:define");
		validArgs.add("-isTestsBoot");
		/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
		validArgs.add("-isTests");
		/* [PR 120038] -tag:remove preprocessor option is rejected */
		validArgs.add("-tag:remove");
		/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
		validArgs.add("-useTestBootpathJavaDoc");
	}

	/**
	 * Identifies all of the arguments supported by the command line builder.
	 *
	 * @param       arg         the argument to be tested
	 * @return      <code>true</code> if the argument is supported, <code>false</code> otherwise
	 */
	private static boolean isSupportedArg(String arg) {
		return validArgs.contains(arg);
	}

	private static String getArgInfo() {
		StringBuilder temp = new StringBuilder();
		temp.append("J9 JCL Preprocessor, Version 4.2.0");
		temp.append("\n\nUsage: CommandLineBuilder -config <config name> -xml <config file> -dest <destination> [options]");
		temp.append("\n\n[options] ");
		temp.append("\n  -baseDir <base dir>    Prepends base dir to all paths in xml");
		temp.append("\n  -srcRoot <source dir>  Appends source dir to base dir for use with local src/output paths");
		temp.append("\n  -verdict               Print information on preprocessor success of failure");
		temp.append("\n  -xmlVerbose            Print information on loaded configurations");
		temp.append("\n  -includeIfUnsure       Include in preprocessor if not sure");
		temp.append("\n  -noWarn                Do not print any preprocessor warnings");
		temp.append("\n  -force                 Unconditionally update output files");
		temp.append("\n  -incremental           Do only an incremental preprocess");
		temp.append("\n  -macro:define          Define a preprocessor macro");
		temp.append("\n  -tag:define            Define a preprocessor tag");
		temp.append("\n  -isTestsBoot           Preprocess for TestBootPath project with required tag TestBootPath");
		temp.append("\n  -updateAllCopyrights   Update copyright on all files to current year instead of the files last modified year");
		temp.append("\n");
		return temp.toString();
	}
}
