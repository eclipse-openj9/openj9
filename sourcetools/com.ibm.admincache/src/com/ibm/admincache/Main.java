/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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


/*
 * Created on Jan 18, 2005 by Ron Servant
 * Modified on April 6->29th, 2005 by Mike Fulton
 * Modified on July  14th,    2005 by Marius Lut
 * - pass to parameters to convertJar
 * - insert -precompileOptions before -outputFormat
 * - codeSize and dataSize JIT options replaced with code and data
 * - insert a space between precompileOptions and data
 * Modified on September 16th, 2005 by Marius Lut
 * - replace backupFile() with file.renameTo(origFile) call
 *   in order to preserve the original file attributes
 * Modified on September 22th, 2005 by Marius Lut
 * - fix CMVC 95820 to accept noprecompileMethod
 * - delete the temporary hack from parseArguments, jar2jxe can handle
 *   methods with signatures now
 * - add logic in convert() that dont backup the orig file if already
 *   exists
 * Modified on November 15th, 2005 by Marius Lut
 * - add mandatory option -outPath
 * - delete the old logic dealing with backup orig files
 * - add logic that accommodates relative/canonical full path file name
 *   passed to jxeinajar
 * - fix the searchPath option problems
 * - if the input file is .zip file make the output file extension .zip too
 * Modified on December 14th, 2005 by Marius Lut
 * - changed -outDir to -outPath
 * - add new methods to deal with file searching
 * - add new methods to convert found jar files
 * - fixed bug related to output directory validation
 * Modified on December 20th, 2005 by Marius Lut
 * - outPath, create an absolute path from a relative path
 * Modified on January 6, 2006 by Marius Lut
 * - enable -optFile option
 * - fix bug that added an input file twice to the list
 * Modified on January 10, 2006 by Marius Lut
 * - fixes to correctly generate the paths of the output files
 * - fixes for the error codes from JIT
 * Modified on January 13, 2006 by Marius Lut
 * - add "Succeeded to JXE jar file" message
 * Modified on March 17, 2006 by Marius Lut
 * - added jxeinajarShutDownHook
 * - delete the tmp file in jxeinajarShutDownHook
 * - changed jxeinajar's error codes to be > 20
 * - changed logic for jar2jxe's return codes
 * Modified on March 20, 2006 by Marius Lut
 * - stop jxeinajar for signals 24 (SIGXCPU) and 25 (SIGXFSZ)
 * Modified on March 26, 2006 by Marius Lut
 * - check if jxeinajar runs on a real time environment
 * Modified on April 26, 2006 by Marius Lut
 * - several changes to fix problems related to optFile
 * - several changes to fix problems related to files that are symbolic linked
 * Modified on May 16, 2006 by Marius Lut
 * - several changes to fix problems related to input files
 * Modified on June 06, 2006 by Marius Lut
 * - reverse the default file searching behavior from recurse to norecurse
 * - fix for checking if output directory is not a subdirectory of searchPath directory
 * - add -Xrealtime disabled option
 * Modified on June 09, 2006 by Marius Lut
 * - improve error messages
 * Modified on July 09, 2006 by Marius Lut
 * - error message when aot code cache overflows
 * Modified on July 10, 2006 by Marius Lut
 * - make sure -Xrealtime is specified for a real time env
 * Modified on July 31, 2006 by Marius Lut
 * - Update year in copyright info
 */

package com.ibm.admincache;
import java.io.*;

/**
 * @author Ronald Servant
 * @author Mike Fulton
 * @author Marius Lut
 */

public class Main {

   private JarPersister _jarPersister;
   public JarPersister jarPersister() { return _jarPersister; }
   public void setJarPersister(JarPersister jp) { _jarPersister = jp; }
   
   private Config config() { return jarPersister().config(); }
   
   public static final int aotCodeCacheOutOfStorage = -8;
   public static final int jar2JXEOutOfStorage = -107;
   public static final int fileToConvertError = -21;
   public static int parseArgumentsError = -22;
   public static final int inputFileError = -23;
   public static final int sNonRealtimeEnv = -24;
   public static final int optFileNotFound = -25;
   public static final int optFileIOException = -26;
   public static final int optFileEmpty = -27;
   public static final int optFileInvalid = -28;
   public static final int cantFindJar2jxe = -29;
   public static final int aotCodeCompilationFailure = -30;
   public static final int invalidFile = -31;
   public static final int cantInvokeAdmincacheError = -32;
   public static final int interruptedDuringAdmincacheError = -33;
   public static final int invalidCacheSize = -34;
   public static final int aotLimitFileProblem = -35;
   public static final int errorRunningSharedCacheUtility = -36;
   public static final int interruptedRunningSharedCacheUtility = -37;
   public static final int populateNeedsRealTimeEnv = -38;
   public static final int onlyOneAOTFilter = -39;
   public static final int errorOpeningClassFilterFile = -40;
   public static final int errorReadingClassFilterFile = -41;
   public static final int badClassFilterFile = -42;
   public static final int badFilterString = -43;
   public static final int sharedCacheFull = -44;
   public static final int unableToGrowCache = -45;
   public static final int cacheSizeWarning = -46;
   public static final int populateNeedsClassPath = -47;
   public static final int jxeinajarNoLongerSupported = -48;

   private int outPathDirectory(String outPath) {
      if (outPath == null) {
         System.out.println("\n-outPath is mandatory\n");
         jarPersister().config().usage();
         return parseArgumentsError;
         }

      if (!checkOutDirectory(outPath)) {
         return parseArgumentsError;
      }

      boolean bSuccess = createOutDirectory(outPath);
      if (bSuccess == false) {
         return parseArgumentsError;
      }
      return 0;
   }

   private boolean checkOutDirectory(String outPath) {
      String searchDir = null;
      String targetDir = null;

      try {
        searchDir = (new File(new File(config().searchPath()).getCanonicalPath())).getPath();
        if (config().TRACE_DEBUG())
           System.out.println("searchPath " + searchDir);
      } catch (IOException e) {
      }

      try {
        File outPathDirectory = new File(outPath);
        if (!outPathDirectory.isAbsolute()) {
           System.out.println("The output directory " + outPath + " should be specified with an absolute path");
           return false;
        }
        targetDir = outPathDirectory.getCanonicalPath();
        if (config().TRACE_DEBUG())
           System.out.println("outPath " + targetDir);
      } catch (IOException e) {
      }


      /* If file names are passed on with tricky paths */
      /* then when the files are about to be AOTed a   */
      /* new check is performed for inputFile == outputFile */
      if ( !config().inputFiles() ) {
         if ( searchDir.equals(targetDir) ) {
            System.out.println("Output Directory " + targetDir + " cannot be the same as the search path directory\n");
            return false;
         } else if (searchDir.length() <= targetDir.length()) {
            if (searchDir.equals(targetDir.substring(0,searchDir.length()))) {
               File fs = new File(searchDir);
               File ft = new File(targetDir);
               try {
                 if (fs.getCanonicalPath().startsWith(ft.getCanonicalPath(),0)) {
                    System.out.println("Output Directory " + targetDir + " cannot be a subdirectory of search path directory " + searchDir);
                    return false;
                 }
               } catch (IOException e) {
               }
            }
         }
      }

      return true;
   }

   private boolean createOutDirectory(String outPath) {
      File dir = (new File(outPath));

      if (dir == null) {
         System.out.println("Failed to create directory" + outPath + " directory\n");
         return false;
      } else if (!dir.exists()) {
         try {
           dir.mkdirs();
         } catch (SecurityException e) {
           System.out.println("Security Exception - Failed to create directory " + outPath + "\n");
           e.printStackTrace();
           return false;
         }
      } else {
         if (dir.isFile()) {
            System.out.println("The output directory " + outPath + " is an existing file");
            return false;
         } else if (!dir.isDirectory()) {
               System.out.println("The output " + outPath + " is not a directory");
               return false;
         }
      }
      return true;
   }

   // helper routine to update our internal state after all command-line argument parsing is complete
   private void updateInternalState() {
      jarPersister().updateInternalState();
   }

   // print out the version of this application
   private void showVersion() {
      if (config().version()) {
         System.out.println(config().myName() + " " + config().myVersion());
      }
   }


   // main routine, which does the following:
   //  parse the arguments, and if invalid, leave
   //  update the internal state of the object based on the results of processing the command line arguments
   //  print out the version of this application
   //  build up the list of jar and zip files to process by calling searchForFiles on all the types of jar files (.jar and .zip)
   //  for each jar/zip file, call convertJar

   public int processArgumentsAndCheckEnvironment(String[] args) {
	   
	   config().preliminaryCheck(args);
	   
	   int rc = Config.checkRealTimeEnvironment();
	   if (rc != 0) {
		   if (config().requiresRealTimeEnvironment()) {
			   System.err.println(config().myName() + " must be run with the -Xrealtime flag on a real-time environment");
			   System.err.println(config().myName() + " doesn't run in a non-realtime environment\n");
			   config().usage();
			   System.exit(Main.sNonRealtimeEnv);
		   }
	   }
	   
	   rc = config().parseArgumentsForCorrectness(args);
	   if (rc != 0) {
		   return rc;
	   }

      if (config().verbose() || config().version()) {
         showVersion();
      }

      // Parse the arguments
      if (config().parseArguments(args) != 0) {
          return parseArgumentsError;
      }

      /*check outPath directory */
      if ( jarPersister().config().needOutPath() && outPathDirectory(config().outPath()) != 0 )
         return parseArgumentsError;

      updateInternalState();
      return 0;
   }

   private void mainLogic(String[] args) {
	   int rc = processArgumentsAndCheckEnvironment(args);
	   if (rc == 0) {
	      rc = jarPersister().performAction();
	   } else  if (config().bJustHelp()) {
		   rc = 0;
	   }
	   
	   if (config().noisy())
	      System.out.println("Return code is " + rc);
	   
	   System.exit(rc);
   }
   
   public Main() {
	   
   }
   public Main(String[] args, JarPersister jp) {
	   setJarPersister(jp);

	   mainLogic(args);
   }
   
}

