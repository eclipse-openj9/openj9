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
import java.util.*;

class JarFinder {

	   private JarPersister _jarPersister;
	   private Config config() { return _jarPersister.config(); }

	   public JarFinder(JarPersister jp) {
		   _jarPersister = jp;
	   }
	   
	   public Vector searchForFiles(String rootDirectory, Vector fileTypes, Vector fileNames, boolean silent) {
		   Vector filesFound = new Vector();

		   if (config().TRACE_DEBUG()) {
			   System.out.println("Searching for files");
		   }
		   
		   String searchDir = null;
		   try {
			   searchDir = new File(rootDirectory).getCanonicalPath();
		   }
		   catch (Exception e) {
			   return filesFound;
		   }
		   
		   if ((!silent || config().TRACE_DEBUG()) && config().verbose()) {
			   if (fileNames.isEmpty())
				   System.out.println("Searching for files to convert");
		   }

		   Vector directoriesToSearch = new Vector();
		   directoriesToSearch.addElement(new File(searchDir));
		   
		   // Can't use an Iterator here because we add to the list inside the loop
		   while (!directoriesToSearch.isEmpty()) {
			   File dir = (File) directoriesToSearch.elementAt(0);
			   directoriesToSearch.removeElementAt(0);
			   
			   String dirCanonical = null;
			   try {
				   dirCanonical = dir.getCanonicalPath();
			   } catch (Exception e) {
			   }
	           if (config().TRACE_DEBUG()) {
	        	   System.out.println("File Search dir =  " + dir);
	           }
	           
	           String[] files = dir.list();
	           if (files != null) {
	        	   for (int f=0; f < files.length; f++) {
	        		   String fileName=dirCanonical + File.separator + files[f];
		               File file = new File(fileName);

		               if (config().TRACE_DEBUG()) {
		            	   System.out.println("Examining file " + fileName);
		               }
		               
		               if (config().checkValidInputFileName(file, false)) {
		            	   if (file.isDirectory() && config().recurse()) {
		            		   if (config().TRACE_DEBUG()) {
		            			   System.out.println("File Search is a directory " + fileName);
		            		   }
	            			   try {
	            				   directoriesToSearch.addElement(file);
	            			   } catch (ConcurrentModificationException e) {
	            			   }

		            		   
//		            		} else if (!fileNames.isEmpty()) {
//		            			Iterator fileIter = fileNames.iterator();
//		            			while (fileIter.hasNext()) {
//		            				String fname = (String) fileIter.next();
//		            				if (file.getName().equals(fname)) {
//		            					config().setBFoundFiles(true);
//		            				if (config().verbose())
//		            		  			System.out.println("Found " + fileCanonical);
//		            					filesFound.add(fileCanonical.toString());
//		            				}
//		            			}

		            	   } else if (fileTypes != null) {
		            		   	Iterator typeIter = fileTypes.iterator();
		            		   	while (typeIter.hasNext()) {
		            		   		String filetype = (String) typeIter.next();
		            		   		if (file.getName().endsWith(filetype)) {
		            		   			config().setBFoundFiles(true);
		            		   			if ((!silent || config().TRACE_DEBUG()) && config().verbose())
		            		   				System.out.println("Found " + fileName);
		            		   			filesFound.add(fileName);
		            		   		}
		            		   	}
		            	   }
		               }
	        	   }
	           }
			   
		   }
		   return filesFound;
	   }
}
