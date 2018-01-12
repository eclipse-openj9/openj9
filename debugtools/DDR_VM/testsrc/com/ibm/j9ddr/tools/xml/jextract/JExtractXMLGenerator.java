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
package com.ibm.j9ddr.tools.xml.jextract;

import java.io.File;
import java.io.FileWriter;
import java.util.HashMap;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

/**
 * Utility class to generate a compatible
 * JExtract XML file using DTFJ so that the
 * outputs can be compared for equivalence
 * testing.
 * 
 * @author Adam Pilkington
 *
 */
public class JExtractXMLGenerator {
	private HashMap<String, String> opts = new HashMap<String, String>();
	private static final String OPT_CORE_FILE = "-f";
	private static final String OPT_OUTPUT_FILE = "-o";
	private StringBuffer xml = new StringBuffer();					//the generated XML
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		JExtractXMLGenerator xmlgen = new JExtractXMLGenerator();
		xmlgen.parseArgs(args);
		xmlgen.generate();
	}
	
	public JExtractXMLGenerator() {
		opts.put(OPT_CORE_FILE, null);
		opts.put(OPT_OUTPUT_FILE, null);
		xml.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	}
	
	public void parseArgs(String[] args) {
		try {
			for(int i = 0; i < args.length; i+=2) {						
				if(opts.containsKey(args[i])) {
					opts.put(args[i], args[i+1]);
				} else {
					throw new IllegalArgumentException("Invalid option : " + args[i]);
				}
			}
		
			for(String key:opts.keySet()) {									
				String value = opts.get(key);
				if(value == null) {
					throw new IllegalArgumentException("The option " + key + " has not been set.");
				}
			}
		} catch (IllegalArgumentException e) {
			System.err.println(e.getMessage());
			printHelp();
			System.exit(1);
		}
	}
	
	/**
	 * Generate a JExtract compatible XML file
	 */
	public void generate() {
		try {
			xml.append("<j9dump>\n");
			JavaRuntime rt = getRuntime();
			generateJavaVM(rt);
			xml.append("</j9dump>\n");
		} catch (Exception e) {		//catch all errors and write out what data we have so far
			System.err.println("Error generating XML : " + e.getMessage());
			e.printStackTrace();
		}
		createOutputFile();
	}
	
	private void generateJavaVM(JavaRuntime rt) throws Exception {
		xml.append("<javavm id=\"0x");
		xml.append(Long.toHexString(rt.getJavaVM().getAddress()));
		xml.append("\">\n");
		generateClassloaders(rt);
		xml.append("</javavm>\n");
	}
	
	@SuppressWarnings("unchecked")
	private JavaRuntime getRuntime() throws Exception {
		ImageFactory factory = new J9DDRImageFactory();
		File core = new File(opts.get(OPT_CORE_FILE)); 
		if(core.exists()) {
			Image image = factory.getImage(core);
			for(Iterator spaces = image.getAddressSpaces(); spaces.hasNext(); ){
				Object space = spaces.next();
				if(!(space instanceof CorruptData)) {
					for(Iterator processes = ((ImageAddressSpace)space).getProcesses(); processes.hasNext();) {
						Object process = processes.next();
						if(!(process instanceof CorruptData)) {
							for(Iterator runtimes = ((ImageProcess)process).getRuntimes(); runtimes.hasNext();) {
								Object runtime = runtimes.next();
								if(runtime instanceof JavaRuntime) {
									return (JavaRuntime)runtime;
								}
							}
						}
					}
				}
			}
			writeComment("Could not find Java runtime in core file");
			throw new IllegalArgumentException("Could not find Java runtime");
		} else {
			throw new IllegalArgumentException("The specified core file : " + core.getAbsolutePath() + " does not exist");
		}
	}
	
	@SuppressWarnings("unchecked")
	private void generateClassloaders(JavaRuntime rt) throws Exception {
		for(Iterator loaders = rt.getJavaClassLoaders(); loaders.hasNext();) {
			JavaClassLoader loader = (JavaClassLoader) loaders.next();
			xml.append("<classloader id=\"");
			//xml.append(loader.getID());
			xml.append("\" obj=\"0x");
			xml.append(Long.toHexString(loader.getObject().getID().getAddress()));
			xml.append("\">\n");
			for(Iterator classes = loader.getCachedClasses(); classes.hasNext(); ){
				JavaClass clazz = (JavaClass) classes.next();
				xml.append("<class id=\"0x");
				xml.append(Long.toHexString(clazz.getID().getAddress()));
				xml.append("\" />\n");
				
			}
			xml.append("</classloader>\n");
		}
	}
	
	private void writeComment(String comment) {
		xml.append("<!-- ");
		xml.append(comment);
		xml.append(" -->\n");
	}
	
	private void createOutputFile() {
		try {
			File core = new File(opts.get(OPT_OUTPUT_FILE));
			if(core.exists()) {
				core.delete();			//delete any existing file
			} else {
				core.getParentFile().mkdirs();			//make sure that the directory structure exists
			}
			FileWriter out = new FileWriter(core);
			out.write(xml.toString());
			out.close();
			System.out.println("XML file generated : " + core.getAbsolutePath());
		} catch (Exception e) {			//write to stdout so that we might still be able to get the generated XML
			System.err.println("Error creating XML output file : " + e.getMessage());
			System.out.println("Writing XML to stdout instead");
			System.out.print(xml.toString());
		}
	}
	
	/**
	 * Print usage help to stdout
	 */
	private static void printHelp() {
		System.out.println("Usage :\n\njava JExtractXMLGenerator -f <path to core file> -o <path for xml file>\n");
		System.out.println("<path to core file> : full path to the core file for which the XML will be generated");
		System.out.println("<path for xml file> : full path of the file to create which contains the XML");
	}

}
