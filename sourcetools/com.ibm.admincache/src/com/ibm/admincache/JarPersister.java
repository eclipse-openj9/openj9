/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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


package com.ibm.admincache;

import java.io.File;
import java.io.IOException;
import java.util.Vector;
import java.util.Iterator;

/**
 * @author Ron Servant
 * @author Mike Fulton
 * @author Mark Stoodley
 */

abstract class JarPersister {

	protected Config _config;
	protected Config config() { return _config; }
	
	private JarFinder _jarFinder;
	public JarFinder jarFinder() { return _jarFinder; }
	public void setJarFinder(JarFinder jf) { _jarFinder = jf; }
	
    abstract public void prepareForAction();
    abstract public void updateInternalState();
	abstract public int convertJar(File origFile, String outputOptions);

	
   public int performAction() {
      prepareForAction();
      
      // TODO: decide what action needs to be done
      
      return processJarFiles();
   }
  
   public int processJarFiles() {
	  setJarFinder(new JarFinder(this));

	  // Find all the jar/zip files.
  	  boolean convert = true;
      Vector sourceFiles;
      if (config().inputFiles()) {
    	  sourceFiles = config().fileNames();
      } else {
    	  sourceFiles = new Vector();
    	  sourceFiles.addAll(jarFinder().searchForFiles(config().searchPath(), config().fileTypes(), config().fileNames(), false));
          if (!config().bFoundFiles())
        	  convert = false;
      }
      
      int rc = 0;
      if (convert) {
    	  if (config().verbose())
    		  System.out.println("Converting files");
    	  rc = convertInputFiles(sourceFiles);
          if (config().verbose())
        	  System.out.println("\nProcessing complete\n");
      } else {
    	  System.out.println("\nNo file was converted\n"); /* Files to convert not found*/
    	  rc = Main.fileToConvertError;
      }

      return rc;
   }
	
    public int convertInputFiles (Vector sourceFiles) {
    	int maxRC = 0;
    	
   		Iterator fileIter = sourceFiles.iterator();
   		while (fileIter.hasNext()) {
   			String fileName = (String) fileIter.next();

   			File searchFileDirectory = new File(config().searchPath());
   			String searchDirectory=null;
   			try {
   				searchDirectory=searchFileDirectory.getCanonicalPath();
   			} catch (IOException e) {
   			}

   			String relativePath = fileName;
   			if (fileName.startsWith(searchDirectory))
   				relativePath = fileName.substring(searchDirectory.length());

   			String outputFileName = null;
   			File outFile = null;
   			boolean convertFile = true;
   			if (config().outPath() != null) {
   				outFile = new File(config().outPath() + File.separator + relativePath);
   				try {
   					outputFileName = outFile.getCanonicalPath();
   					if (outputFileName.equals(fileName)) {
   						System.out.println("Failure, the input file " +  fileName + " will be overwritten by the new output file " + outputFileName);
   						maxRC = Main.inputFileError;
   						convertFile = false;
   					}
   				} catch (Exception e) {
   					convertFile = false;
   				}
   			}

   			if (convertFile) {
   				if (config().outPath() != null) {
   					if (config().TRACE_DEBUG()) {
   						System.out.println("searchDirectory is " + searchDirectory);
   						System.out.println("relative path is " + relativePath);
   					}
   					outFile.getParentFile().mkdirs();
   	   				if (config().TRACE_DEBUG())
   	   					System.out.println("About to process jar file: " + fileName + " into " + outputFileName);

   				} else {
   					if (config().TRACE_DEBUG())
   						System.out.println("About to process jar file: " + fileName);
   				}
   				
   				int rc = convertJar(new File(fileName), outputFileName);
   				if (rc < -20 || rc == Main.aotCodeCacheOutOfStorage) { /* greater than JIT warning/error codes */
   					System.out.println("Unable to complete processing jar file " + fileName);
   					if (rc == Main.aotCodeCacheOutOfStorage) {
   						System.out.println("Input file " + fileName + " is too big to generate optimized AOT jar file.");
   						System.out.println("Try splitting it up into several smaller jars.");
   						rc = Main.jar2JXEOutOfStorage;
   					}
   					System.out.println("No processing performed: " + rc);
   				}
   				else if (rc == 0 && config().verbose())
   					System.out.println("No errors while processing jar file " + fileName);
			   
   				/* 21-43 jxeinajar errors/warnings */
   				/* > 100    256-1 , 256-21 jar2jxe errors */
   				/* < 20 jit errors/warnings */
   				if (rc < -20 && maxRC == 0)
   					maxRC = rc;
			   
   				if (rc == 152) {
			   		System.out.println("CPU time limit exceeded ");
			   		break;
			   	} else if (rc == 153) {
			   		System.out.println("File size limit exceeded ");
			   		break;
			   	}
		   	}
   		}
    	
   	return maxRC;
    }
}
