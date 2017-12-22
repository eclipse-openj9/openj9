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

import java.io.*;
import java.util.*;

/**
 * @author Mark Stoodley
 */
class SharedCacheConfig extends Config {

   public SharedCacheConfig() {
	   setMyName("admincache");
	   setMyVersion("1.08");
   }
   
   // program configuration options
   private final boolean _COMPILE_AFTER_LOADING = false;
   public boolean COMPILE_AFTER_LOADING() { return _COMPILE_AFTER_LOADING; }

   // command-line configuration options
   private String _cacheName = null;
   public String cacheName() { return _cacheName; }
   public void setCacheName(String s) { _cacheName = s; }

   private String _cacheDir = null;
   public String cacheDir() { return _cacheDir; }
   public void setCacheDir(String s) { _cacheDir = s; }
   
   private boolean _persistent;
   public boolean persistent() { return _persistent; }
   public void setPersistent(boolean b) { _persistent = b; }
   
   // public enum Action { NoAction, Destroy, DestroyAll, ListAllCaches, PrintStats, PrintAllStats, Populate };
   final static int ACTION_NOACTION = 0;
   final static int ACTION_DESTROY = 1;
   final static int ACTION_DESTROYALL = 2;
   final static int ACTION_LISTALLCACHES = 3;
   final static int ACTION_PRINTSTATS = 4;
   final static int ACTION_PRINTALLSTATS = 5;
   final static int ACTION_POPULATE = 6;
   
   private int _action = ACTION_NOACTION;
   public int action() { return _action; }
   public void setAction(int a) { _action = a; }
   
   private boolean _aot = true;
   public boolean aot() { return _aot; }
   public void setAot(boolean b) { _aot = b; }
 
   private String _origClassFilter;
   public String origClassFilter() { return _origClassFilter; }
   private void setOrigClassFilter(String s) { _origClassFilter = s; }

   static boolean _populating = false;
   public static boolean populating() { return _populating; }
   public static void setPopulating(boolean b) { _populating = b; }
   
   static boolean _commandError = false;
   public static boolean commandError() { return _commandError; }
   public static void setCommandError(boolean b) { _commandError = b; }
   
   private void badFilterString(String s) {
	   if (s != null && s.length() > 0)
		   System.err.println("bad filter string: " + s);
	   else
		   System.err.println("bad filter string");
	   System.err.println("\t1. enclose in curly braces {...}");
	   System.err.println("\t2. only valid class or method name");
	   System.err.println("\t\tseparate class packages with slashes (/)");
	   System.err.println("\t\tseparate class name from method name with period (.)");
	   System.err.println("\t3. wildcards can be used:");
	   System.err.println("\t\t* - match any number of characters");
	   System.exit(Main.badFilterString);
   }
   
   private void checkFilterString(String s, boolean isMethodFilter) {
	   if (s.charAt(0) != '{' || s.charAt(s.length()-1) != '}')
		   badFilterString(s);
	   char firstChar = s.charAt(1);
	   if (firstChar != '*' && firstChar != '?' && firstChar != '}' && !Character.isJavaLetter(firstChar))
		   badFilterString(s);
	   for (int i=1;i < s.length()-1;i++) {
		   char c = s.charAt(i);
		   
		   if (c == '*' || c == '?' || c == '/')
			   continue;
		   
		   if (isMethodFilter && (c == '.' || c == '(' || c == ')' || c == '<' || c == '>' || c == ',' || c == '[' || c == ';' || c == '$'))
			   continue;
		   
		   if (!Character.isJavaLetterOrDigit(c))
			   badFilterString(s);
	   }   
   }
	
   static String replace(String original, String pattern, String replace) {
       int i = 0, l = 0, len = pattern.length();
       StringBuffer result = new StringBuffer();

       l = original.indexOf(pattern, i);
       while (l >= 0) {
           result.append(original.substring(i, l));
           result.append(replace);
           i = l+len;
           l = original.indexOf(pattern, i);            
       }
       result.append(original.substring(i));
       return result.toString();
	}
   
   private String _classFilter;
   public String classFilter() { return _classFilter; }
   public void setClassFilter(String s) {
	   setOrigClassFilter(s);
	   checkFilterString(s, false);
	   s = s.substring(1,s.length()-1);
	   
	   // translate to regex for later use
	   s = replace(s, "*", ".*");
	   s = replace(s, "?", ".");
	   if (TRACE_DEBUG())
		   System.out.println("class filter " + s);
	   _classFilter = s;
   }
   
   private File _classFilterFile;
   public File classFilterFile() { return _classFilterFile; }
   public void setClassFilterFile(File f) { _classFilterFile = f; }
   
   private String _aotFilter = null;
   public String aotFilter() { return _aotFilter; }
   public void setAotFilter(String s) { checkFilterString(s, true); _aotFilter = s; }
   
   private File _aotFilterFile = null;
   public File aotFilterFile() { return _aotFilterFile; }
   public void setAotFilterFile(File f) { _aotFilterFile = f; }
   
   private boolean _printVMArgs;
   public boolean printVMArgs() { return _printVMArgs; }
   public void setPrintVMArgs(boolean b) { _printVMArgs = b; }
   
   private String _cacheMaxSize = null; //new String("-Xscmx150M");
   public String cacheMaxSize() { return _cacheMaxSize; }
   public void setCacheMaxSize(String s) { _cacheMaxSize = s; }
 
   private boolean _explicitCacheSize = false;
   public boolean explicitCacheSize() { return _explicitCacheSize; }
   public void setExplicitCacheSize(boolean b) { _explicitCacheSize = b; }
   
   private boolean _debugger = false;
   public boolean debugger() { return _debugger; }
   public void setDebugger(boolean b) { _debugger = b; }
   
   private boolean _nativeDebugger = false;
   public boolean nativeDebugger() { return _nativeDebugger; }
   public void setNativeDebugger(boolean b) { _nativeDebugger = b; }
   
   private String _extraAotArgs;
   public String extraAotArgs() { return _extraAotArgs; }
   public void setExtraAotArgs(String s) { _extraAotArgs = s; }
   
   private ArrayList<String> _extraArgs;
   public ArrayList<String> extraArgs() { return _extraArgs; }
   public void setExtraArgs(String s) {
	   if (_extraArgs == null) {
		   _extraArgs = new ArrayList<String>();
	   }
	   _extraArgs.add(s);
   }
   
   private boolean _grow = false;
   public boolean grow() { return _grow; }
   public void setGrow(boolean b) { _grow = b; }

   private String _classPath=null;
   public String classPath() { return _classPath; }
   public void setClassPath(String s) { _classPath = s; };
   
   private String _printAllStatsFileName;
   public String printAllStatsFileName() { return _printAllStatsFileName; }
   public void setPrintAllStatsFileName(String s) { _printAllStatsFileName = s; }
   
   private static boolean _fullCacheMessageFound = false;
   public static boolean fullCacheMessageFound() { return _fullCacheMessageFound; }
   public static void setFullCacheMessageFound(boolean b) { _fullCacheMessageFound = b; }
   
   private boolean _testGrow = false;
   public boolean testGrow() { return _testGrow; }
   public void setTestGrow(boolean b) { _testGrow = b; }
   
//	 helper routine to print out usage
	public void usage() {
		System.out.println("Usage: " + myName() + " [option]*");
		System.out.println("  where [option] can be:");
		System.out.println("     -help | -?                Action: show this help");
		System.out.println("     -Xrealtime                use in real time environment");
		System.out.println("     -cacheName <name>         specify name of shared cache (Use %%u to substitute username)");
		System.out.println("     -cacheDir <dir>           set the location of the JVM cache files");
		System.out.println("     -listAllCaches            Action: list all existing shared class caches");
		System.out.println("     -printStats               Action: print cache statistics");
		System.out.println("     -printAllStats            Action: print more verbose cache statistics");
		System.out.println("     -destroy                  Action: destroy the named (or default) cache");
		System.out.println("     -destroyAll               Action: destroy all caches");
		if (realTimeEnvironment()) {
			System.out.println("     -populate                 Action: Create a new cache and populate it");
			System.out.println("       -searchPath <path>      specify the directory in which to find files if no files specified (default is .)");
			System.out.println("                                 only one -searchPath option can be specified");
			System.out.println("       -classpath <class path> specify the classpath that will be used at runtime to access this cache");
			System.out.println("                                 the -classpath option is required");
			System.out.println("       -[no]recurse            [do not] recurse into subdirectories to find files to convert (default do not recurse)");
			System.out.println("       -[no]grow               if specified cache exists already, [do not] add to it (default no grow)");
			System.out.println("                                 if -grow is not selected, specified cache will be removed if present");
			System.out.println("       -verbose                print out progress messages for each jar");
			System.out.println("       -noisy                  print out progress messages for each class in each jar");
			System.out.println("       -quiet                  suppress all output");
			System.out.println("       -[no]aot                also perform AOT compilation on methods after storing classes into cache (default is aot)");
			//System.out.println("       -classFilter <pattern>  only classes listed in file will be stored into cache");
			System.out.println("       -aotFilter <signature>  only matching methods will be AOT compiled and stored into cache");
			System.out.println("          e.g. -aotFilter {mypackage/myclass.mymethod(I)I} compiles only mymethod(I)I");
			System.out.println("          e.g. -aotFilter {mypackage/myclass.mymethod*} compiles any mymethod");
			System.out.println("          e.g. -aotFilter {mypackage/myclass.*} compiles all methods from myclass");
			System.out.println("       -aotFilterFile <file>   only methods matching those in file will be AOT compiled and stored into cache");
			System.out.println("                                 (input file must have been created by -Xjit:verbose={precompile},vlog=<file>)");
			System.out.println("       -printvmargs            print VM arguments needed to access populated cache at runtime");
			System.out.println("       [jar file]*.[jar][zip]  explicit list of jar files to populate into cache");
			System.out.println("                                 if no files are specified, all files.[jar][zip] in the searchPath will be converted.");
		}
		System.out.println("Exactly one action option must be specified\n");
	}


		
   protected int acceptableArgumentWithParameters(String arg) {
	   // these options are not supported via admincache, but handling for them is in Config,
	   // so need to explicitly return -1 here
	   if ((arg.equalsIgnoreCase("-verify"))          ||
		   (arg.equalsIgnoreCase("-noverify"))        ||
		   (arg.equalsIgnoreCase("-optFile"))) {
		    return -1;
	   }
	   else if ((arg.equals("--debugger"))                 ||
		   (arg.equals("--nativedebugger"))           ||
           (arg.equalsIgnoreCase("-destroy"))         ||
           (arg.equalsIgnoreCase("-destroyAll"))      ||
           (arg.equalsIgnoreCase("-listAllCaches"))   ||
           (arg.equalsIgnoreCase("-printStats"))      ||
           (arg.equalsIgnoreCase("-printAllStats"))   ||
           (arg.startsWith("-AJ"))) {		// undocumented option to pass along any JVM option, like -J but -J is consumed by the launcher
           return 0;
	   } else if ((arg.equalsIgnoreCase("-cacheName"))||
                  (arg.equalsIgnoreCase("-cacheDir")) ||
                  (arg.equalsIgnoreCase("-classPath"))||
                  (arg.equalsIgnoreCase("-cp"))) {
		   return 1;
	   } else if (arg.equalsIgnoreCase("-populate")) {
		   if (!realTimeEnvironment())
			   return -1;
		   setPopulateAction(true);
		   return 0;
	   } else if ((arg.equalsIgnoreCase("-aot"))      ||
                  (arg.equalsIgnoreCase("-noaot"))    ||
                  (arg.equalsIgnoreCase("-grow"))     ||
                  (arg.equalsIgnoreCase("-nogrow"))   ||
                  (arg.equals("-testGrow"))           ||
                  (arg.startsWith("-Xscmx"))          ||
                  (arg.startsWith("-Xaot"))           ||
                  (arg.startsWith("-Xgc"))            ||
                  (arg.startsWith("-Xmx"))            ||
                  (arg.equalsIgnoreCase("-printvmargs"))) {
		   if (!realTimeEnvironment())
			   return -1;
		   return 0;
	   } else if(
			      //(arg.equalsIgnoreCase("-classFilter"))     ||
	              //(arg.equalsIgnoreCase("-classFilterFile")) ||
			   	  (arg.equalsIgnoreCase("-printAllStatsFile")) ||
	              (arg.equalsIgnoreCase("-aotFilter"))         ||
	              (arg.equalsIgnoreCase("-aotFilterFile"))) {
		   if (!realTimeEnvironment())
			   return -1;
		   return 1;
	   } else {
    	   return super.acceptableArgumentWithParameters(arg);
	   }
   }
   
   private File checkFileName(String fileName) {
	   File file = new File(fileName);
	   if (file.exists() && file.isFile() && file.canRead()) {
		   return file;
	   }
	   return null;
   }

   public int parseArgument(String arg, String nextArg) {
	   int argsToConsume=1;	// optimistic!
	   if (arg.equals("--debugger")) {			// undocumented option
		   setDebugger(true);
	   } else if (arg.equals("--nativedebugger")) {	// undocumented option
		   setNativeDebugger(true);
	   } else if (arg.equalsIgnoreCase("-aot")) {
		   setAot(true);
	   } else if (arg.equalsIgnoreCase("-noaot")) {
		   setAot(false);
	   } else if (arg.equalsIgnoreCase("-grow")) {
		   setGrow(true);
	   } else if (arg.equalsIgnoreCase("-nogrow")) {
		   setGrow(false);
	   } else if (arg.equalsIgnoreCase("-testgrow")) {
		   setTestGrow(true);
	   } else if (arg.equalsIgnoreCase("-printvmargs")) {
		   setPrintVMArgs(true);
	   } else if (arg.equalsIgnoreCase("-destroy")) {
		   setAction(ACTION_DESTROY);
	   } else if (arg.equalsIgnoreCase("-destroyAll")) {
		   setAction(ACTION_DESTROYALL);
	   } else if (arg.equalsIgnoreCase("-listAllCaches")) {
		   setAction(ACTION_LISTALLCACHES);
	   } else if (arg.equalsIgnoreCase("-printStats")) {
		   setAction(ACTION_PRINTSTATS);
	   } else if (arg.equalsIgnoreCase("-printAllStats")) {
		   setAction(ACTION_PRINTALLSTATS);
	   } else if (arg.equalsIgnoreCase("-populate")) {
		   setAction(ACTION_POPULATE);
	   } else if (arg.startsWith("-Xaot:")) {
		   setExtraAotArgs(arg.substring(6));
	   } else if (arg.startsWith("-Xgc")) {
		   setExtraArgs(arg);
	   } else if (arg.startsWith("-Xmx")) {
		   setExtraArgs(arg);
	   } else if (arg.startsWith("-Xscmx")) {
		   int size = convertSize(arg.substring(6));
		   if (size <= 0) {
			   System.err.println("Invalid cache size specified: " + arg);
			   Main.parseArgumentsError = Main.invalidCacheSize;
			   return Main.invalidCacheSize;
		   }
		   setCacheMaxSize(arg);
		   setExplicitCacheSize(true);
	   } else if (arg.startsWith("-AJ")) {
		   setExtraArgs(arg.substring(3));
	   } else if (nextArg != null) {
		   argsToConsume = 2;	// more optimism!
		   if (arg.equalsIgnoreCase("-cacheName")) {
			   setCacheName(nextArg);
		   } else if (arg.equalsIgnoreCase("-cacheDir")) {
			   setCacheDir(nextArg);
		   } else if (arg.equalsIgnoreCase("-classpath") || arg.equalsIgnoreCase("-cp")) {
			   setClassPath(nextArg);
		   } else if (arg.equalsIgnoreCase("-printAllStatsFile")) {
			   setPrintAllStatsFileName(nextArg);
//		   } else if (arg.equalsIgnoreCase("-classFilter")) {
//			   setClassFilter(nextArg);
//		   } else if (arg.equalsIgnoreCase("-classFilterFile")) {
//			   File classFilterFile = checkFileName(nextArg);
//			   if (classFilterFile != null)
//				   setClassFilterFile(classFilterFile);
//			   else {
//				   System.err.println("Cannot read specified class filter file: " + nextArg);
//				   Main.parseArgumentsError = Main.invalidFile;
//				   return Main.invalidFile;
//			   }
		   } else if (arg.equalsIgnoreCase("-aotFilter")) {
			   if (aotFilterFile() != null) {
				   System.err.println("Error: only one of aotFilter and aotFilterFile can be specified");
				   return Main.onlyOneAOTFilter;
			   }
			   setAotFilter(nextArg);
		   } else if (arg.equalsIgnoreCase("-aotFilterFile")) {
			   if (aotFilter() != null) {
				   System.err.println("Error: only one of aotFilter and aotFilterFile can be specified");
				   return Main.onlyOneAOTFilter;
			   }
			   File aotFilterFile = checkReadableFileName(nextArg);
			   if (aotFilterFile != null)
				   setAotFilterFile(aotFilterFile);
			   else {
				   System.err.println("Cannot read specified aot filter file: " + nextArg);
				   Main.parseArgumentsError = Main.invalidFile;
				   return Main.invalidFile;
			   }
		   } else {
			   argsToConsume = 0;
		   }
	   } else {
		   if (arg.equalsIgnoreCase("-aotFilter"))
			   badFilterString("no filter specified");
		   
		   argsToConsume = 0;
	   }

	   if (argsToConsume == 0)
		   return super.parseArgument(arg, nextArg);
	   
	   return argsToConsume;
   }
   
   private int convertSize(String size) {
	   	char unit = Character.toLowerCase(size.charAt(size.length() - 1));
	   	int number = Integer.parseInt(size.substring(0,size.length()-1));
	   	if (unit == 'k')
	   		number *= 1024;
	   	if (unit == 'm')
	   		number *= 1024 * 1024;
	   	return number;
   }
   
   public Vector<String> buildApplicationOptions(Vector <String> options) {
	   options = super.buildApplicationOptions(options);
	   
	   if (aot())
		   options.add(new String("-aot"));
	   
	   if (printAllStatsFileName() != null) {
		   options.add(new String("-printAllStatsFile"));
		   options.add(printAllStatsFileName());
	   }
	   
	   if (printVMArgs())
		   options.add(new String("-printvmargs"));
	   
	   return options;
   }
   
   	public Vector<String> buildVMArgs(Vector <String> options) {
   		StringBuffer scArg = new StringBuffer("-Xshareclasses");
	   	char delim=':';
	   
   		if (cacheName() != null) {
   			scArg.append(delim).append("name=").append(cacheName());
   			delim=',';
   		}
   		if (cacheDir() != null) {
   			scArg.append(delim).append("cacheDir=").append(cacheDir());
   			delim=',';
   		}
   		options.add(scArg.toString());
	   
	   	if (classPath() != null) {
		       options.add(new String("-classpath"));
		       options.add(classPath());
		   	}

	   	if (aot())
	   		options.add(new String("-Xaot"));

	   	if (extraArgs() != null)
	   		options.addAll(extraArgs());
	   	
		return options;
   	}
   
   public Vector<String> buildJavaVmOptions(Vector <String> options) {
	   
	   	StringBuffer scArg = new StringBuffer("-Xshareclasses");
	   	char delim=':';
	   	
	   	if (TRACE_DEBUG()) {
	   		scArg.append(delim).append("verbose,verboseAOT,verboseIO,verboseIntern");
	   		delim=',';
	   	}
	   	
	   	if (realTimeEnvironment()) {
	   		scArg.append(delim).append("grow");
	   		delim=',';
	   	}
	   	
	   	{
	   		scArg.append(delim).append("reset");
	   		delim=',';
	   	}
	   	
	   	if (cacheName() != null) {
	   		scArg.append(delim).append("name=").append(cacheName());
	   		delim=',';
	   	}
	   	
	   	if (cacheDir() != null) {
	   		scArg.append(delim).append("cacheDir=").append(cacheDir());
	   		delim=',';
	   	}	   	

	   	options.add(scArg.toString());

	   	if (cacheMaxSize() != null)
	   		options.add(cacheMaxSize());
	   	
	   	if (aot()) {
	   		StringBuffer aotArg = new StringBuffer("-Xaot:forceAOT,codetotal=1000000,loadExclude={*}," +
	   				"counts=\"- - - - - - - - - 1000000000 1000000000 1000000000 - - - 1000000 1000000 1000000\"," +
	   				"samplingFrequency=3600000,scorchingSampleThreshold=30,disableDynamicLoopTransfer,disableCHOpts");
	   		if (aotFilter() != null) {
	   			aotArg.append(",limit=").append(aotFilter());
	   		}
	   		else if (aotFilterFile() != null) {
	   			try {
	   				String aotFilterFileName = aotFilterFile().getCanonicalPath();
	   				aotArg.append(",limitFile=").append(aotFilterFileName);
	   			} catch (IOException ioe) {
	   				System.err.println("Error: IO Exception while accessing limit file: " + aotFilterFile().getName());
	   				System.exit(Main.aotLimitFileProblem);
	   			}
	   		}
	   		if (extraAotArgs() != null) {
	   			aotArg.append(",").append(extraAotArgs());	// rightmost wins, so put user overrides at the end
	   		}
	   		
	   		options.add(aotArg.toString());
	   		
	   		String newJitOpts = "-Xjit:abortExplicitJITCompilations,counts=\"- - - - - - - - - 1000000000 1000000000 1000000000 - - - 1000000 1000000 1000000\"";
	   		if (jitOpts() != null) {
	   			// rightmost wins, so put user overrides at the end
	   			newJitOpts = newJitOpts + "," + jitOpts().substring("-Xjit:".length());
	   		}
   			setJitOpts(newJitOpts);
	   	}
	   	
	   	if (extraArgs() != null) {
	   		options.addAll(extraArgs());
	   	}
	   	
	   	if (classPath() != null) {
	       options.add(new String("-classpath"));
	       options.add(classPath());
	   	}
	   	options = super.buildJavaVmOptions(options);

	   	return options;
   }
}
