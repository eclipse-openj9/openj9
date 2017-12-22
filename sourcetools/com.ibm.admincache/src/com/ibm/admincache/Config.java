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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;

/**
 * @author Ronald Servant
 * @author Mike Fulton
 * @author Marius Lut
 * @author Mark Stoodley
 */

abstract class Config {
   private static boolean _TRACE_DEBUG = false;
   public boolean TRACE_DEBUG() { return _TRACE_DEBUG; }
   public void setTRACE_DEBUG(boolean b) { _TRACE_DEBUG = b; }
   
   private static boolean _realTimeEnvironment = false;
   public static boolean realTimeEnvironment() { return _realTimeEnvironment; }
   public static void setRealTimeEnvironment(boolean b) { _realTimeEnvironment = b; }
   
   private static boolean _requiresRealTimeEnvironment = true;
   public boolean requiresRealTimeEnvironment() { return _requiresRealTimeEnvironment; }
   public void setRequiresRealTimeEnvironment(boolean b) { _requiresRealTimeEnvironment = b; }
   
   private static boolean _populateAction = false;
   public boolean populateAction() { return _populateAction; }
   public void setPopulateAction(boolean b) { _populateAction = b; }
   
   private static String _outPath = null;
   public String outPath() { return _outPath; }
   public void setOutPath(String s) { _outPath = s; }

   // myName - name of the program as reported in the logo
   private static String _myName;
   public String myName() { return _myName; }
   public void setMyName(String s) { _myName = s; }
   
   // myVersion - version of the program as report in the version
   private static String _myVersion;
   public String myVersion() { return _myVersion; }
   public void setMyVersion(String s) { _myVersion = s; }

   // optFile - remember if -optFile option was set
   private static boolean _isOptFile = false;
   public boolean isOptFile() { return _isOptFile; }
   public void setIsOptFile(boolean b) { _isOptFile = b; }
   
   // searchPath - default place to look for jars/zips to convert
   private static String _searchPath = ".";
   public String searchPath() { return _searchPath; }
   public void setSearchPath(String s) { _searchPath = s; }

   // true if search path was specified, false if individual files passed
   private static boolean _bSearchPath = true; // default since searchPath default is "."
   public boolean bSearchPath() { return _bSearchPath; }
   public void setBSearchPath(boolean b) { _bSearchPath = b; }

   // recurse - whether directory recursion should be performed or not when looking for jars and zips (only matters with -searchPath)
   private static boolean _recurse = false;
   public boolean recurse() { return _recurse; };
   public void setRecurse(boolean b) { _recurse = b; };

   // fileTypes - list of file types to look for (currently .zip and .jar)
   private static Vector<String> _fileTypes;
   {
	   _fileTypes = new Vector<String>();
	   _fileTypes.add(".jar");
	   _fileTypes.add(".zip");
   }
   public Vector<String> fileTypes() { return _fileTypes; }

   // fileNames - list of file names to look for (default is all)
   private static Vector<String> _fileNames = new Vector<String>();
   public Vector<String> fileNames() { return _fileNames; }

   private static boolean _inputFiles = false;
   public boolean inputFiles() { return _inputFiles; }
   public void setInputFiles(boolean b) { _inputFiles = b; }

   private static boolean _absoluteFilePath = false;
   public boolean absoluteFilePath() { return _absoluteFilePath; }
   public void setAbsoluteFilePath(boolean b) { _absoluteFilePath = b; }

   // verbose - whether verbose output is written to the terminal or not (default is verbose)
   private static boolean _verbose = true;
   public boolean verbose() { return _verbose; }
   public void setVerbose(boolean b) { _verbose = b; }

   // verbose - whether verbose output from conversion process is written to the terminal or not (default is not noisy)
   private static boolean _noisy = false;
   public boolean noisy() { return _noisy; }
   public void setNoisy(boolean b) { _noisy = b; }

   // version - whether to print the program version to the terminal or not (default is to write it)
   private static boolean _version = true;
   public boolean version() { return _version; }
   public void setVersion(boolean b) { _version = b; }

   // verify - whether to verify bytecode on startup or not
   private static boolean _verify = true;
   public boolean verify() { return _verify; }
   public void setVerify(boolean b) { _verify = b; }

   // debug (internal option) - whether to print debug info to the terminal or not
   private static boolean _debug = false;
   public boolean debug() { return _debug; }
   public void setDebug(boolean b) { _debug = b; }


   // By default, precompile everything, but if precompileMethod directives encountered,
   // just compile what the user asks for
   private static boolean _fullCompile = true;
   public boolean fullCompile() { return _fullCompile; }
   public void setFullCompile(boolean b) { _fullCompile = b; }

   private static boolean _bFoundFiles = false;
   public boolean bFoundFiles() { return _bFoundFiles; }
   public void setBFoundFiles(boolean b) { _bFoundFiles = b; }

   private static boolean _bJustHelp = false;
   public boolean bJustHelp() { return _bJustHelp; }
   public void setBJustHelp(boolean b) { _bJustHelp = b; }

   private static String _jitOpts = null;
   public String jitOpts() { return _jitOpts; }
   public void setJitOpts(String s) { _jitOpts = s; }
   
   private boolean _needOutPath = false;
   public boolean needOutPath() { return _needOutPath; }
   public void setNeedOutPath(boolean b) { _needOutPath = b; }

   private boolean _badFileEncountered = false;
   public boolean badFileEncountered() { return _badFileEncountered; }
   public void setBadFileEncountered(boolean b) { _badFileEncountered = true; }
   
   abstract public void usage();
   
   
   public static int checkRealTimeEnvironment() {/*check real time environment */
	   try {
		   Class.forName("javax.realtime.ThrowBoundaryError");
		   setRealTimeEnvironment(true);
	   }
	   catch (Exception e) {
		   return Main.sNonRealtimeEnv;
	   }
	   return 0;
   }
   
   public File checkReadableFileName(String fileName) {
	   File file = new File(fileName);
	   if (file.canRead())
		   return file;
	   else
		   return null;
   }
   
   
   // print out the underlying usage, along with an error message about an invalid option
   // if, indeed, the option was invalid
   public void printUsage(String arg) {
      if (arg.equalsIgnoreCase("-help") || arg.equals("-?")) {
         ; // do not print out a msg about an invalid option
      } else {
         System.out.println(arg + " is not a recognized option");
      }
      usage();
   }

   
	// helper routine to read in arguments from an options file, returning an array of options to
	// be processed
   private String[] readArgs(String optFile) {
	  StringBuffer options = new StringBuffer();
	  /* change the procedure of reading the optFile to avoid OOM */
	  BufferedReader readBuffer = null;
	  boolean varOptFileEmpty = true;
	  try {
	      readBuffer = new BufferedReader( new FileReader(optFile) );
	      String readLine = null;

	      while (( readLine = readBuffer.readLine()) != null){
	    	  String trimmedLine = readLine.trim();
	    	  /* skip empty lines */
	    	  if (trimmedLine.length() != 0) {
	    		  varOptFileEmpty=false;
	    		  if (readLine.startsWith("-")) {
	    			  /* create the string options from one line */
	    			  StringTokenizer tok = new StringTokenizer(new String(readLine), "\n\t \r");
	    			  String[] checkOptions = new String[tok.countTokens()];
	    			  int entry = 0;
	    			  while (tok.hasMoreTokens()) {
	    				  checkOptions[entry++] = (String) tok.nextToken();
	    			  }
	    			  /* check the string options from one line */
	    			  if (parseArgumentsForCorrectness(checkOptions) != 0) {
	    				  System.out.println("Read invalid line from optFile: " + readLine);
	    				  Main.parseArgumentsError = Main.optFileInvalid;
	    				  return null;
	    			  }

	    			  options.append(readLine);
	    			  options.append(System.getProperty("line.separator"));
	    		  } else {
	    			  System.out.println("Read invalid line from optFile: " + readLine);
	    			  Main.parseArgumentsError = Main.optFileInvalid;
	    			  return null;
	    		  }
	    	  }
	      }
	  }
	  catch (FileNotFoundException ex) {
	      Main.parseArgumentsError = Main.optFileNotFound;
	      return null;
	  }
	  catch (IOException ex){
	      Main.parseArgumentsError = Main.optFileIOException;
	      return null;
	  }
	  finally {
	      try {
	    	  if (readBuffer != null) {
	    		  readBuffer.close();
	    	  }
	      }
	      catch (IOException ex) {
	    	  ex.printStackTrace();
	      }
	  }

	  if (varOptFileEmpty) {
		  Main.parseArgumentsError = Main.optFileEmpty;
	  }
	  /* create the string options to be added to the temp file */
	  StringTokenizer tok = new StringTokenizer(new String(options), "\n\t \r");
	  String[] optionArray = new String[tok.countTokens()];
	  int entry = 0;
	  while (tok.hasMoreTokens()) {
		  optionArray[entry++] = (String) tok.nextToken();
	  }

	  return optionArray;
   }
  
   protected int acceptableArgumentWithParameters(String arg) {
	   if ((arg.equalsIgnoreCase("-debug"))         ||
           (arg.equalsIgnoreCase("-detaileddebug")) ||
           (arg.startsWith("-Xjit:"))               ||
           (arg.equalsIgnoreCase("-noversion"))     ||
           (arg.equalsIgnoreCase("-version"))       ||
           (arg.equalsIgnoreCase("-nologo"))        ||	// for backwards compatibility
           (arg.equalsIgnoreCase("-logo"))) {			// for backwards compatibility
		   return 0;
	   } else if((arg.equalsIgnoreCase("-norecurse"))     ||
                 (arg.equalsIgnoreCase("-recurse"))       ||
                 (arg.equalsIgnoreCase("-verify"))        ||
                 (arg.equalsIgnoreCase("-noverify"))      ||
                 (arg.equalsIgnoreCase("-verbose"))       ||
                 (arg.equalsIgnoreCase("-noisy"))         ||
                 (arg.equalsIgnoreCase("-quiet"))         ||
                 (arg.equalsIgnoreCase("-precompile"))    ||
                 (arg.equalsIgnoreCase("-noprecompile"))) {
		   if (!realTimeEnvironment())
			   return -1;
		   
		   return 0;
       } else if ((arg.equalsIgnoreCase("-searchPath"))         ||
                  (arg.equalsIgnoreCase("-precompileMethod"))   ||
                  (arg.equalsIgnoreCase("-noprecompileMethod")) ||
                  (arg.equalsIgnoreCase("-jar2JxeOpt"))         ||
                  (arg.equalsIgnoreCase("-outPath"))            ||
                  (arg.equalsIgnoreCase("-optFile"))) {
    	   if (!realTimeEnvironment())
    		   return -1;
    	   
		   return 1;
	   }
	   return -1;
   }
	 
   public void preliminaryCheck(String[] args) {
	   // look for some debug and verbose arguments
	   for (int i = 0; i < args.length; i++) {
		   args[i] = args[i].trim();
		   String arg = args[i];
		   if (arg.equalsIgnoreCase("-debug")) {				// undocumented option - for printing out debug info
			   setDebug(true);
		   } else if (arg.equalsIgnoreCase("-detaileddebug")) { // undocumented option - for printing out more debug info
			   setDebug(true);
			   setTRACE_DEBUG(true);
		   } else if (arg.equalsIgnoreCase("-version")) {
			   setVersion(true);
		   } else if (arg.equalsIgnoreCase("-noversion")) {
			   setVersion(false);
		   } else if (arg.equalsIgnoreCase("-verbose")) {
			   setVerbose(true);
			   setNoisy(false);
		   } else if (arg.equalsIgnoreCase("-noisy")) {
			   setVerbose(true);
			   setNoisy(true);
		   } else if (arg.equalsIgnoreCase("-quiet")) {
			   setVerbose(false);
			   setNoisy(false);
		   }
	   }
   }
   public int startArgumentParsing(String[] args) {
	   for (int i = 0; i < args.length; i++) {
		   String arg = args[i];
	       if (arg.equalsIgnoreCase("-help") || arg.equalsIgnoreCase("-?")) {
	             usage();
	             setBJustHelp(true);
	             return Main.parseArgumentsError;
	       }
	   }
	   return 0;
   }

   private void unrecognizedArgument(String arg) {
   	   System.out.println("\nUnrecognized option " + arg + "\n");
   	   usage();
   }
	   
   private void unrecognizedPopulateArgument(String arg) {
	   if (!realTimeEnvironment()) {
		   System.out.println("\nUnrecognized option " + arg + "\n");
	   } else {
		   System.out.println("\nOnly valid in combination with -populate : " + arg + "\n");
	   }
	   usage();
   }
   
   public int parseArgumentsForCorrectness(String[] args) {
	   int rc = 0;
	   if ((rc = startArgumentParsing(args)) != 0)
		   return rc;
	   
	   for (int i = 0; i < args.length; i++) {
	       args[i] = args[i].trim();
	       String arg=args[i];
	       if (TRACE_DEBUG())
	           System.out.println("Passed ARGS " + arg + " length " + args.length);

	       int numParams;
	       if (arg.length() > 0 && arg.charAt(0) != '-') {
	           /*file not option, nothing to check*/
	       } 
	       else
	    	   {
	    	   numParams = acceptableArgumentWithParameters(arg);
	    	   if (numParams >= 0 &&
	       	         i+numParams < args.length) {
	    	   i += numParams;
//	       } else if (arg.equalsIgnoreCase("-optFile")) {
	           //System.out.println("Nested -optFile is not supported ");
	           //return Main.parseArgumentsError;
	       } else if (numParams == -1){
	    	   unrecognizedArgument(arg);
	    	   return Main.parseArgumentsError;
    	   }
	       }
	   }
	   return 0;
	}

   	public boolean checkValidInputFileName(File inputFile, boolean listedOnCmdLine) {
       	File inputFileCanonical=null;
       	try {
       		inputFileCanonical = new File(inputFile.getCanonicalPath());
       	} catch (IOException e) {
       		if (TRACE_DEBUG()) {
       			System.out.println("Got unexpected exception: ");
       			e.printStackTrace();
       		}
       		return false;
       	}
 
       	String inputFileNameCanonical = inputFileCanonical.toString();
       	if (TRACE_DEBUG())
       		System.out.println("inputFileNameCanonical is " + inputFileNameCanonical);

   		if (!inputFileCanonical.exists()) {
   			if (listedOnCmdLine || TRACE_DEBUG())
   				System.out.println("Input File " + inputFileNameCanonical + " not found.");
   			
   			if (listedOnCmdLine) {
   				setBadFileEncountered(true);
   				Main.parseArgumentsError = Main.inputFileError;
                if (isOptFile()) {
                	System.out.println("\tPlease check your command line and options file for errors");
                } else {
                	System.out.println("\tPlease check your command line");
                }
   			}
   			
   			return false;
   		} else if (TRACE_DEBUG()) {
   			System.out.println("File exists");
   		}


   		if (inputFileCanonical.isDirectory()) {
   			if (listedOnCmdLine) {
   				System.out.println("Skipping processing input file " + inputFileNameCanonical + " because it is a directory.");
   				if (isOptFile()) {
   					System.out.println(" Check your command line and options file for errors");
   				} else {
   					System.out.println(" Check your command line");
   				}
   				return false;
   			} else {
   				if (TRACE_DEBUG()) {
   					System.out.println("Found valid directory");
   	   			}
   				return true;
   			}
   		} else if (TRACE_DEBUG()) {
   			System.out.println("Is not a directory");
   		}

   		boolean matchesSomeType = false;
   		for (String fileType: fileTypes()) {
   			if (inputFile.getName().endsWith(fileType)) {
   				matchesSomeType = true;
   				break;
   			}	
   		}
   
   		if (!matchesSomeType) {
   			if (listedOnCmdLine && TRACE_DEBUG()) {
            	System.out.println("Input File " + inputFileNameCanonical + " should have .jar or .zip file extension.");
            	if (isOptFile()) {
            		System.out.println(" Check your command line and options file for errors");
            	} else {
            		System.out.println(" Check your command line");
            	}
   			}
   			return false;
   		} else if (TRACE_DEBUG()){
   			System.out.println("Matched some type");
   		}

   		if (!inputFileCanonical.isFile()) {
   			if (TRACE_DEBUG())
   				System.out.println("Input file " + inputFileNameCanonical + " isn't a file!");
   			return false;
   		} else if (TRACE_DEBUG()) {
   			System.out.println("Is a file");
   		}

   		if (!inputFileCanonical.exists()) {
   			if (listedOnCmdLine || TRACE_DEBUG())
   				System.out.println("Input File " + inputFileNameCanonical + " not found.");
   			
   			if (listedOnCmdLine) {
   				setBadFileEncountered(true);
   				System.out.println("GOT HERE");
                if (isOptFile()) {
                	System.out.println(" Check your command line and options file for errors");
                } else {
                	System.out.println(" Check your command line");
                }
   			}
   			
   			return false;
   		} else if (TRACE_DEBUG()) {
   			System.out.println("File exists");
   		}

   		if (outPath() != null) {
   			if (inputFileNameCanonical.startsWith(outPath())) {
   				if (inputFileCanonical.getPath().equals(outPath())) {
   					System.out.println("Skipping processing input file " + inputFileNameCanonical + " because it is located in or under the output path");
   					if (isOptFile()) {
   						System.out.println(" Check your command line and options file for errors");
   					} else {
   						System.out.println(" Check your command line");
   					}
   					return false;
   				} else if (TRACE_DEBUG()) {
   					System.out.println("In outpath, but not in same directory");
   				}
   			}
			if (bSearchPath()) {
				String relativePath = inputFileCanonical.toString().substring(searchPath().length());
				String outputFileName = outPath() + File.separator + relativePath;
				File outputFile = new File(outputFileName);
				try {
					outputFileName = outputFile.getCanonicalPath();
   					if (outputFileName.equals(inputFileNameCanonical)) {
   						System.out.println("Skipping processing input file " + inputFileNameCanonical + " because output file would overwrite it");
   						return false;
   					}
				} catch (Exception e) {
				}
				if (TRACE_DEBUG())
					System.out.println("Output file doesn't overwrite input file");
   			}
   		} else if (TRACE_DEBUG()) {
   			System.out.println("outpath not specified");
   		}
   		
   		if (TRACE_DEBUG())
   			System.out.println("File is valid!");
   		
  		return true;
   	}
   
   /*hashtable for the name of the files */
   private static Hashtable<String, Integer> _unicityOfFileName;
   
   public int parseArgument(String arg, String nextArg) {
	   int argsToConsume = 1; // default: consume the argument sent in
	   
        if (arg.startsWith("-Xjit:")) { // undocumented option - for passing options to the JIT
           String newJitOpts = arg.substring("-Xjit:".length());
           if (jitOpts() != null) {
              setJitOpts(jitOpts() + "," + newJitOpts);
           } else {
              setJitOpts(arg);
           }
        } else if (arg.equalsIgnoreCase("-optFile")) {
           argsToConsume = 2;
           String[] newArgs = readArgs(nextArg);

           if (Main.parseArgumentsError == Main.optFileNotFound) {
              System.out.println("Options File " + arg + " cannot be found");
              usage();
              return Main.parseArgumentsError;
           } else if (Main.parseArgumentsError == Main.optFileIOException) {
              System.out.println("IO Exception while reading the Options File " + arg);
              return Main.optFileIOException;
           } else if (Main.parseArgumentsError == Main.optFileEmpty) {
              System.out.println("Options File " + arg + " is empty");
              return Main.optFileEmpty;
           } else if (Main.parseArgumentsError == Main.optFileInvalid) {
              return Main.optFileInvalid;
           }
           else if (parseArguments(newArgs) != 0) {
              return Main.parseArgumentsError;
           }
        } else if (arg.equalsIgnoreCase("-debug")) {
        	;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-detaileddebug")) {
            ;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-help")) {
            ;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-?")) {
            ;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-noversion")) {
        	;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-version")) {
        	;	// processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-nologo")) {
        	;	// ignore for backwards compatibility
        } else if (arg.equalsIgnoreCase("-logo")) {
        	;	// ignore for backwards compatibility

        } else if (arg.length() > 0 && arg.charAt(0) != '-') {
        	if (!populateAction()) {
 	    	   unrecognizedPopulateArgument(arg);
	    	   return Main.parseArgumentsError;
        	}

        	// this is a file, not an option
        	File f = null;
        	File file = null;
        	f = new File(arg);
        	if (f.isAbsolute() || !bSearchPath())
        		file = f;
        	else
        		file = new File(searchPath() + File.separator + arg);
       	
        	/* add the file name only once to the list */
        	if (checkValidInputFileName(file, true)) {
        		// this should work because checkValidInputFileName just did it
        		String fileNameCanonical=null;
        		try {
        			fileNameCanonical = file.getCanonicalPath();
        		} catch (Exception e) {
        		}
        		if (TRACE_DEBUG())
        			System.out.println("File found, adding to list of files");
        		Integer addedFileName = null;
        		addedFileName = _unicityOfFileName.get(fileNameCanonical);
        		if (addedFileName == null) {
        			fileNames().add(fileNameCanonical);
        			_unicityOfFileName.put(fileNameCanonical, new Integer(0));
        		}
        	} else {
        		if (badFileEncountered())
        			return Main.parseArgumentsError;
        	}

        	setBSearchPath(false);
        	setInputFiles(true);
        } else if (arg.equalsIgnoreCase("-verbose") 	||
        		   arg.equalsIgnoreCase("-noisy")		||
        		   arg.equalsIgnoreCase("-quiet")) {
        	
        	if (!populateAction()) {
        		unrecognizedPopulateArgument(arg);
        		return Main.parseArgumentsError;
        	}
    		;	// argument was processed earlier in parseArguments(), but don't want to return an error
        } else if (arg.equalsIgnoreCase("-recurse")) {
        	if (!populateAction()) {
        		unrecognizedPopulateArgument(arg);
        		return Main.parseArgumentsError;
        	}
        	else if (!bSearchPath()) {
        		System.err.println("Error: -recurse only valid with a search path");
        		return Main.parseArgumentsError;
        	}
        	setRecurse(true);
        } else if (arg.equalsIgnoreCase("-norecurse")) {
        	if (!populateAction()) {
        		unrecognizedPopulateArgument(arg);
        		return Main.parseArgumentsError;
        	}
        	setRecurse(false);
        } else if (arg.equalsIgnoreCase("-verify")) {
        	if (!populateAction()) {
        		unrecognizedPopulateArgument(arg);
        		return Main.parseArgumentsError;
        	}
        	setVerify(true);
        } else if (arg.equalsIgnoreCase("-noverify")) {
        	if (!populateAction()) {
        		unrecognizedArgument(arg);
        		return Main.parseArgumentsError;
        	}
        	setVerify(false);
        } else if (nextArg != null) {
        	argsToConsume = 2;
        	if (arg.equalsIgnoreCase("-searchPath")) {
            	if (!populateAction()) {
            		unrecognizedPopulateArgument(arg);
            		return Main.parseArgumentsError;
            	}
        		;	// processed earlier in parseArguments(), but don't want to return an error
        	} else if (arg.equalsIgnoreCase("-outPath")) {
            	if (!populateAction()) {
            		unrecognizedPopulateArgument(arg);
            		return Main.parseArgumentsError;
            	}
        		if (nextArg.length() > 0 && nextArg.charAt(0) == '-') {
        			System.err.println("\nDirectory name cannot start with - \n");
        			return Main.parseArgumentsError;
        		}
                
        		File canonicalOutFile = null;
        		try {
        			File outPathFile = new File(nextArg);
        			canonicalOutFile = outPathFile.getCanonicalFile();
        		} catch (Exception e) {
        			System.err.println("\nOutput directory not valid");
        			return Main.parseArgumentsError;
        		}
        		
        		setOutPath(canonicalOutFile.toString());
        		if (outPath() == null) {
        			System.err.println("\nOutput directory not specified or invalid\n");
        			return Main.parseArgumentsError;
        		}
        	} else {
        		System.out.println("\nSevere error examining argument " + arg + "\n");
        		return Main.parseArgumentsError;
        	}
        } else {
    		System.out.println("\nSevere error examining argument " + arg + "\n");
    		return Main.parseArgumentsError;
        }
       
       return argsToConsume;
   }

   private int parseSearchPathOption(String sSearchPath) {
   		/* deal with the searchPath option first given that file path processing is based */
   		/* on searchPath's argument */
   		if (TRACE_DEBUG())
   			System.out.println(" InputSearchPath = " + sSearchPath);
   			/*Check/create searchPath validity*/
   			/*
	      		1. is the searchPath a full path directory name
	      		2. if it is not then it is relative to the current directory
	         		create the full path and  check it if it is valid
	    	*/
   		File file = new File(sSearchPath);
   		if (file.isDirectory()) {
   			try {
   				setSearchPath(file.getCanonicalPath());
   			} catch (IOException e) {
   			}
   			if (TRACE_DEBUG())
   				System.out.println("searchPath is a directory = " + searchPath());
   		}
   		else {
   			System.err.println("Search Path " + file.getPath() + " is NOT a directory");
   			return Main.parseArgumentsError;
   		}
   		
   		setBSearchPath(true);
	
		/* prepare searchPath if it was not set yet */
		if (searchPath().charAt(0) == '.' ) {
			File fSearchPath = new File(searchPath());
			try {
				setSearchPath(fSearchPath.getCanonicalPath());
			} catch (IOException e) {
			}
		}
		return 0;
   }

  // parse the command line arguments, updating the flags and command line arguments that will be
  // used to build the underlying command line.
  public int parseArguments(String[] args) {
     // next find and parse any searchPath option since interpreting file names depends on it
     for (int i = 0; i < args.length; i++) {
	     if (args[i].equalsIgnoreCase("-searchPath")) {
	         if (parseSearchPathOption(args[++i]) != 0)
	             return -1;
	         break;
	     }
     }
     
     /*hashtable for the names of the files */
     _unicityOfFileName = new Hashtable<String, Integer>();

     int rc = startArgumentParsing(args);
     if (rc != 0)
    	 return rc;
     
     for (int i = 0; i < args.length; i++) {
    	String arg = args[i];
    	
        if (TRACE_DEBUG())
           System.out.println("Passed ARGS " + arg + " length " + args.length);

        String nextArg=null;
        if (i < args.length - 1)
        	nextArg = args[i+1];
        int argsToConsume = parseArgument(arg, nextArg);
        if (TRACE_DEBUG())
        	System.out.println("Args to consume == " + argsToConsume);
        if (argsToConsume > 0)
        	i+= argsToConsume-1;
        else if (argsToConsume < 0)
        	return argsToConsume;	// parsing error so bail
     }
     
    return 0;
  }
  
  /**
   * Create the list of options passed to the application
   * @param original options string 
   * @return options strings to be placed in the application's args vector.
   */
  public Vector<String> buildApplicationOptions(Vector <String> options) {
	  options.add(new String("-noversion"));
	  
	  if (TRACE_DEBUG())
		  options.add(new String("-detaileddebug"));
	  
	  if (debug())
		  options.add(new String("-debug"));
	  
	  if (outPath() != null) {
		  options.add(new String("-outPath "));
		  options.add(outPath());
	  }
	  
	  if (!searchPath().equals(".")) {
		  options.add(new String("-searchPath "));
		  options.add(searchPath());
	  }
	  
	  if (recurse())
		  options.add(new String("-recurse"));
	  else
		  options.add(new String("-norecurse"));

	  if (noisy())
		  options.add(new String("-noisy"));
	  else if (verbose())
		  options.add(new String("-verbose"));
	  else
		  options.add(new String("-quiet"));

	  if (inputFiles()) {
		  for (String fileName: fileNames()) {
			  options.add(fileName);
		  }
	  }

	  return options;
  }
  
  public Vector<String> buildJavaVmOptions(Vector <String> options) {
	  String bootClassPath = System.getProperty("sun.boot.class.path");
	  StringBuffer opt = new StringBuffer("-Xbootclasspath:").append(bootClassPath);;
	  options.add(opt.toString());

	  if (realTimeEnvironment()) {
		  options.add(new String("-Xrealtime"));
		  options.add(new String("-Xmx512m"));
	  }
	  if (jitOpts() != null) {
		  options.add(jitOpts());
	  }

	  return options;
  }
  
};
