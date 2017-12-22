/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob;

import java.io.File;
import java.io.FileFilter;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.HashMap;

/**
 * Utility class to generate the Java code needed for DDR - this consists of 2 steps
 * 
 * 1. Merge the component supersets into a single c-build superset
 * 2. Generate a new c-build superset which is the combined superset for the last i-build and the current c-build
 * 
 * @author adam
 *
 */
public class GenerateSpecSuperset {
	private static final HashMap<String, String> opts = new HashMap<String, String>();
	private static final String OPT_SRCDIR = "-s";			//where to find the component supersets
	private static final String OPT_OUTPUTDIR = "-d";		//where to generate the output
	private static final String OPT_FILENAME = "-f";		//superset file name
	private static File outdir = null;
	
	static {
		opts.put(OPT_OUTPUTDIR, null);
		opts.put(OPT_SRCDIR, null);
		opts.put(OPT_FILENAME, "superset.spec.dat");		//default value
	}
	
	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception {
		if(!parseArgs(args)) {
			System.exit(1);		//abort if invalid args are encountered
		}
		createOutputDir();
		createSpecSuperset();
		System.out.println("Finished");
	}
	
	private static void createSpecSuperset() throws Exception {
		System.out.println("Creating superset");
		String[] parts = opts.get(OPT_SRCDIR).split(",");
		File out = new File(outdir, opts.get(OPT_FILENAME));
		FileWriter writer = new FileWriter(out);
		
		for(int i = 0; i < parts.length; i++) {
			System.out.println("Scanning source directory for *.superset : " + parts[i]);
			File srcdir = new File(parts[i]);
			if(!srcdir.exists() || !srcdir.isDirectory()) {
				System.err.println("Error : The specified source directory (" + srcdir.getAbsolutePath() + ") does not exist, or is not a directory.");
				System.exit(1);
			}
			File[] files = srcdir.listFiles(new FileFilter() {
				
				public boolean accept(File pathname) {
					return pathname.getName().endsWith(".superset");
				}
			});
			if(0 == files.length) {
				System.err.println("Error : The source directory (" + srcdir.getAbsolutePath() + ") does not contain any superset files (*.superset).");
				System.exit(1);
			}
			char[] buffer = new char[4096];
			int charsread = 0;
			for(File file : files) {
				System.out.println(("Processing " + file.getName()));
				FileReader reader = new FileReader(file);
				while((charsread = reader.read(buffer)) != -1) {
					writer.write(buffer, 0, charsread);
				}
				reader.close();
			}
		}
		writer.close();
	}
	
	//creates the output directory for the superset and java code
	private static void createOutputDir() {
		outdir = new File(opts.get(OPT_OUTPUTDIR));
		if(outdir.exists()) {
			if(outdir.isDirectory()) {
				System.out.println("Cleaning existing output directory " + outdir.getAbsolutePath());
				//directory exists so clean it out
				for(File file : outdir.listFiles()) {
					file.delete();
				}
			} else {
				//can't create as there is a conflict
				System.err.println("Error : The specified output directory (" + outdir.getAbsolutePath() + ") already exists as a file.");
				System.exit(1);
			}
		} else {
			//dir does not exist so create it
			System.out.println("Creating output directory " + outdir.getAbsolutePath());
			if(!outdir.mkdirs()) {
				System.err.println("Error : The  could not create the output directory (" + outdir.getAbsolutePath() + ").");
				System.exit(1);
			}
		}
	}
	
	private static boolean parseArgs(String[] args) {
		boolean validargs = true;
		//parse args
		for(int i = 0; (i + 1) < args.length; i+=2) {
			if(opts.containsKey(args[i])) {
				opts.put(args[i], args[i+1]);
			} else {
				System.err.println("Option " + args[i] + " was not recognised");
			}
		}
		//check none are missing
		for(String key : opts.keySet()) {
			if(null == opts.get(key)) {
				validargs = false;
				System.err.println("Required option " + key + " was not set on the command line");
			}
		}
		return validargs;
	}

}
