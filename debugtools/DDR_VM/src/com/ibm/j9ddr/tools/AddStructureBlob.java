/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Properties;

import com.ibm.j9ddr.tools.store.J9DDRStructureStore;
import com.ibm.j9ddr.tools.store.StructureKey;
import com.ibm.j9ddr.tools.store.StructureMismatchError;

public class AddStructureBlob {
	private static final String PROP_DDR_ORDER = "ddr.order";		//property to control the order in which ddr processes blobs/supersets

	HashMap<String, String> opts = new HashMap<String, String>();

	public AddStructureBlob() {
		super();
		initArgs();
	}
	
	private void initArgs() {
		opts.put("-d", null);	// database directory
		opts.put("-f", null);	// structure file or directory of structure files to add
		opts.put("-k", null);	// key
		opts.put("-s", null);   // optional superset filename
		opts.put("-c", "");			//optional value to provide a cache properties file
	}

	public static void main(String[] args) {
		AddStructureBlob app = new AddStructureBlob();
		app.parseArgs(args);
		app.run();		
	}

	//loads the order (if specified) in which blobs/supersets are to be processed
	private String[] loadSpecNames() {
		String path = opts.get("-c"); 
		if((null == path) || (path.length() == 0)) {
			//path for properties file not specified so return an empty array
			return new String[0];
		}
		Properties props = new Properties();
		File file = new File(path);
		if(file.exists()) {
			try {
				FileInputStream in = new FileInputStream(file);
				props.load(in);
				String specs = props.getProperty(PROP_DDR_ORDER);
				if(null == specs) {
					String msg = String.format("The cache properties file [%s] specified by the -c option does not specify a %s key", file.getAbsolutePath(), PROP_DDR_ORDER);
					throw new IllegalArgumentException(msg);
				}
				return specs.split(",");
			} catch (Exception e) {
				String msg = String.format("The cache properties file [%s] specified by the -c option could not be read", file.getAbsolutePath());
				throw new IllegalArgumentException(msg, e);
			}
		} else {
			String msg = String.format("The cache properties file [%s] specified by the -c option does not exist", file.getAbsolutePath());
			throw new IllegalArgumentException(msg);
		}
	}
	
	private void run() {
		String directoryName = opts.get("-d");
		String supersetName = opts.get("-s");
		
		if( supersetName ==  null )  {
			supersetName = J9DDRStructureStore.DEFAULT_SUPERSET_FILE_NAME;
		}		
		
		try {
			J9DDRStructureStore store = new J9DDRStructureStore(directoryName, supersetName);
			String path = opts.get("-c"); 
			if((null == path) || (path.length() == 0)) {
				//path for properties file not specified so default to adding from a single file or directory
				addFromFile(store);
			} else {
				//a property file controls the order in which blobs are processed
				addFromPropertiesFile(store);
			}
		} catch (IOException e) {
			e.printStackTrace();
		} catch (StructureMismatchError e) {
			e.printStackTrace();
		}
	}
	
	//add a structure from a specified path
	private void addFromFile(J9DDRStructureStore store) throws IOException, StructureMismatchError {
		String directoryName = opts.get("-d");
		String structureName = opts.get("-f");
		String keyString = opts.get("-k");
		String supersetName = opts.get("-s");
		
		if (keyString == null) {
			File structurePath = new File(structureName);
			File[] structureFiles;
			if( structurePath.isDirectory() ) {
				structureFiles = structurePath.listFiles();
			} else {
				structureFiles = new File[] {structurePath};
				structurePath = structurePath.getParentFile();
			}
			for( File file: structureFiles ) {
				if(file.exists()) {
					store.add(null, file.getPath(), false);
					store.updateSuperset();
					System.out.println("Added " + file.getName() + " to superset " + supersetName + " in " + directoryName);
				} else {
					System.out.println("WARNING : The specified structure file " + file.getName() + " does not exist and was ignored");
				}
			}
		} else {
			StructureKey key = new StructureKey(keyString);
			store.add(key, structureName, true);
			store.updateSuperset();
			System.out.println(String.format("Added %s to %s/%s as %s", structureName, directoryName, store.getSuperSetFileName(), keyString));
		}		
	}
	
	//the order in which files are added is controlled by a properties file, it expects the -f parameter to point to a directory containing
	//either blobs or supersets
	private void addFromPropertiesFile(J9DDRStructureStore store) throws IOException, StructureMismatchError {
		String directoryName = opts.get("-d");
		String keyString = opts.get("-k");
		File structurePath = new File(opts.get("-f"));
		String[] specs = loadSpecNames();	//load the spec order (if specified on the command line)
		File[] structureFiles = new File[specs.length];
		for(int i = 0; i < specs.length; i++) {
			structureFiles[i] = new File(structurePath, specs[i]);
		}
		for( File file: structureFiles ) {
			if(file.exists()) {
				StructureKey key = new StructureKey(file.getName() + "." + keyString);
				store.add(key, file.getPath(), false);
				store.updateSuperset();
				System.out.println(String.format("Added %s to %s/%s as %s", file.getAbsolutePath(), directoryName, store.getSuperSetFileName(), key));
			} else {
				System.out.println("WARNING : The specified structure file " + file.getName() + " does not exist and was ignored");
			}
		}
	}

	private void parseArgs(String[] args) {

		if((args.length == 0) || ((args.length % 2) != 0)) {
			System.out.println("Not enough options specified as program arguments");
			System.out.println(" args [" + args.length + "]");
			printHelp();
			System.exit(1);
		}
		
		for(int i = 0; i < args.length; i+=2) {						
			if(opts.containsKey(args[i])) {
				opts.put(args[i], args[i+1]);
			} else {
				System.out.println("Invalid option : " + args[i]);
				printHelp();
				System.exit(1);
			}
		}
		
		for(String key:opts.keySet()) {									
			String value = opts.get(key);
			if(value == null) {
				if (!(key.equals("-k") || key.equals("-s"))) {
					System.err.println("The option " + key + " has not been set.\n");
					printHelp();
					System.exit(1);
				}
			}
		}
	}
	
	/**
	 * Print usage help to stdout
	 */
	private static void printHelp() {
		System.out.println("Usage :\n\njava AddStructureBlob -d <directory name> -f <structure file name> [-s <superset file name>] [-k <structure key>]\n");
	}
}
