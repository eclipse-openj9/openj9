/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9.uma;

import com.ibm.j9.uma.configuration.ConfigurationImpl;
import com.ibm.j9tools.om.Flag;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.util.Logger;
import java.io.File;
import java.util.HashMap;

public class Main {
	static String applicationVersion = "2.5.1";
	static String applicationNameLong = "Universal Makefile Adapter - for J9";
	static String applicationNameShort = "$(J9UMA)";
	
	static String option_quiet = "-quiet";
	static String option_quiet_short = "-q";
	static String option_verbose = "-verbose";
	static String option_verbose_short = "-v";
	static String option_help = "-help";
	static String option_help_short = "-h";
	static String option_rootDir = "-rootDir";
	static String option_rootDir_short = "-r";
	static String option_configDir = "-configDir";
	static String option_configDir_short = "-c";
	static String option_buildSpecId = "-buildSpecId";
	static String option_buildSpecId_short = "-b";
	static String option_buildId = "-buildId";
	static String option_buildId_short = "-bid";
	static String option_buildDate = "-buildDate";
	static String option_buildDate_short = "-bd";
	static String option_buildTag = "-buildTag";
	static String option_buildTag_short = "-bt";
	static String option_jitVersionFile = "-jitVersionFile";
	static String option_jitVersionFile_short = "-jvf";
	static String option_excludeArtifacts = "-excludeArtifacts";
	static String option_excludeArtifacts_short = "-ea";
	static String option_define = "-D";
	static String option_undefine = "-U";
	static String option_macro = "-M";
	
	static boolean splashed = false;
	static boolean quiet = false;
	static boolean verbose = false;
	static String buildSpecId = null;
	static String configDirectory = null;
	static String rootDir = ".";
	static String buildId = "0";
	static String buildDate = "12150101";
	static String buildTag = "dev_12150101_0000_B0";
	static String jitVersionFile = "";
	static String excludeArtifacts = "";
	static HashMap<String, Boolean> overrideFlags = new HashMap<String,Boolean>();
	static HashMap<String, String> extraMacros = new HashMap<String, String>();
	static Logger logger;

	static void dumpSplash() {
		if (splashed||quiet) {
			return;
		}
		System.out.println( applicationNameLong + ", version " + applicationVersion );
		System.out.println( "" );
		splashed = true;
	}
	
	static void dumpHelp() {
		dumpSplash();
		System.err.println(applicationNameShort + " [options] " + option_rootDir + " <directory> " + option_configDir + " <directory> " + option_buildSpecId + " <specId>" );
		System.err.println("options:");
		System.err.println("\t-quiet");
		System.err.println("\t-verbose");
		System.err.println("\t-help");
		System.err.println("\t" +"{" + option_define + "|" + option_undefine + "} <flagName>");
		System.err.println("\t" + option_rootDir + " <directory>");
		System.err.println("\t" + option_configDir + " <directory>");
		System.err.println("\t" + option_buildSpecId + " <specId>");
		System.err.println("\t" + option_buildId + " <buildId>");
		System.err.println("\t" + option_buildDate + " <buildDate>");
		System.err.println("\t" + option_buildTag + " <buildTag>");
		System.err.println("\t" + option_macro + " <name>=<value>");
		System.err.println("");
	}
	
	static boolean parseOptions(String[] args) {

		for (int i=0; i<args.length; i++) {
			String arg = args[i];
			if (!arg.startsWith("-")) {
				System.err.println("Badly formatted command line.\n");
				return false;
			}
			if (arg.equalsIgnoreCase(option_quiet)||arg.equalsIgnoreCase(option_quiet_short)) {
				quiet = true;
				continue;
			} else if (arg.equalsIgnoreCase(option_verbose)||arg.equalsIgnoreCase(option_verbose_short)) {
				verbose = true;
				continue;
			} else if (arg.equalsIgnoreCase(option_help) || arg.equalsIgnoreCase(option_help_short)) {
				return false;
			} else if (arg.equalsIgnoreCase(option_rootDir) || arg.equalsIgnoreCase(option_rootDir_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				rootDir = args[++i];
				rootDir = rootDir.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_configDir) || arg.equalsIgnoreCase(option_configDir_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				configDirectory = args[++i];
				configDirectory = configDirectory.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_buildSpecId) || arg.equalsIgnoreCase(option_buildSpecId_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				buildSpecId = args[++i];
				buildSpecId = buildSpecId.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_buildDate) || arg.equalsIgnoreCase(option_buildDate_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				buildDate = args[++i];
				buildDate = buildDate.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_buildId) || arg.equalsIgnoreCase(option_buildId_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				buildId = args[++i];
				buildId = buildId.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_buildTag) || arg.equalsIgnoreCase(option_buildTag_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				buildTag = args[++i];
				buildTag = buildTag.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_jitVersionFile) || arg.equalsIgnoreCase(option_jitVersionFile_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				jitVersionFile = args[++i];
				jitVersionFile = jitVersionFile.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_excludeArtifacts) || arg.equalsIgnoreCase(option_excludeArtifacts_short)) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				excludeArtifacts = args[++i];
				excludeArtifacts = excludeArtifacts.trim();
				continue;
			} else if (arg.equalsIgnoreCase(option_define) ) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				overrideFlags.put(args[++i], true);
				continue;
			} else if (arg.equalsIgnoreCase(option_undefine) ) {
				if ( i+1 >= args.length ) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				overrideFlags.put(args[++i], false);
				continue;
			} else if (arg.equalsIgnoreCase(option_macro)) {
				i += 1;
				if (i >= args.length) {
					System.err.println("Must provide an argument for option " + arg + "\n");
					return false;
				}
				String value = args[i].trim();
				int eqIndex = value.indexOf('=');
				if (eqIndex <= 0) {
					System.err.println("Argument for option " + arg + " must be of the form 'name=value'\n");
					return false;
				}
				extraMacros.put(value.substring(0, eqIndex), value.substring(eqIndex + 1));
				continue;
			} else {
				System.err.println("Unknown command line option " + arg + "\n");
				return false;
			}
		}

		if ( rootDir == null || buildSpecId == null || configDirectory == null ) {
			System.err.println("Missing a required command line option.\n");
			return false;
		}

		return true;
	}

	public static void main(String[] args) {
		
		if ( !parseOptions(args) ) {
			System.out.print(applicationNameShort + " ");
			for ( String arg : args ) {
				System.out.print(" " + arg );
			}
			System.out.println();
			dumpHelp();
			System.exit(-1);
		}
		
		logger = Logger.initLogger(verbose?Logger.InformationL2Log:Logger.InformationL1Log);
		
		dumpSplash();
		
		if ( jitVersionFile.equals("") ) {
			File tempJitVersionFile = new File(rootDir + "/compiler/build/version.h");
			if(tempJitVersionFile.exists() && !tempJitVersionFile.isDirectory()) {
				jitVersionFile = rootDir + "/compiler/build/version.h";
				System.out.print("Using version.h as the jitVersionFile\n");
			} else {
				tempJitVersionFile = new File(rootDir + "/jit.version");
				if(tempJitVersionFile.exists() && !tempJitVersionFile.isDirectory()) {
					jitVersionFile = rootDir + "/jit.version";
					System.out.print("Using jit.version as the jitVersionFile\n");
				}
			}
		}
		
		try {
			ConfigurationImpl configuration = new ConfigurationImpl(configDirectory, buildSpecId, buildId, buildDate, buildTag, jitVersionFile, excludeArtifacts);
			for(String flagString: overrideFlags.keySet()){
				if(!configuration.isFlagValid(flagString)){
					throw new UMAException("Invalid flag override: " + flagString);
				}
				Flag flag = configuration.getBuildSpec().getFlag(flagString);
				flag.setState(overrideFlags.get(flagString));
			}

			for (java.util.Map.Entry<String, String> entry : extraMacros.entrySet()) {
				String name = entry.getKey();
				String value = entry.getValue();

				configuration.defineMacro(name, value);
			}

			// Since we may have changed some flags, we need to re-verify them
			configuration.verify();

			new UMA(configuration, configuration, rootDir).generate();
		} catch (NullPointerException e) {
			logger.println(Logger.ErrorLog, "Internal error: null pointer exception");
			e.printStackTrace();
			System.exit(-1);
		} catch (UMAException e) {
			logger.println(Logger.ErrorLog, e.getMessage());
			System.exit(-1);
		}
	}
}
