package tests.sharedclasses;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Properties;
import java.util.Set;


/**
 * A superclass for test programs that provides lots of helpful methods for setting up
 * a shared classes environment and running shared classes commands.
 * 
 * Configuration is done from a config file.  The default name is config.properties, which can
 * be overridden with a -Dconfig.properties=<pathToConfigFile>
 */
public class TestUtils {
	
	public static boolean debug = false;
	
	// Known commands, defined in the config file
	public static final String DestroyCacheCommand = "destroyCache";
	public static final String DestroyPersistentCacheCommand = "destroyPersistentCache";
	public static final String DestroyNonPersistentCacheCommand = "destroyNonPersistentCache";
	public static final String RunSimpleJavaProgram = "runSimpleJavaProgram";
	public static final String RunComplexJavaProgramWithPersistentCache = "runComplexJavaProgram";
	public static final String RunPrintStats = "runPrintStats";
	public static final String RunPrintAllStats = "runPrintAllStats";
	public static final String RunResetPersistentCache = "runResetPersistentCache";
	public static final String RunResetNonPersistentCache = "runResetNonPersistentCache";
	public static final String RunSimpleJavaProgramWithPersistentCache = "runSimpleJavaProgramWithPersistentCache";
	public static final String RunSimpleJavaProgramWithPersistentCacheCheckStringTable = "runSimpleJavaProgramWithPersistentCacheCheckStringTable";
	public static final String RunSimpleJavaProgramWithNonPersistentCache = "runSimpleJavaProgramWithNonPersistentCache";
	public static final String CreateNonPersistentCache = "createNonPersistentCache";
	public static final String CreateCacheSnapshot = "createCacheSnapshot";
	public static final String ListAllCaches = "listAllCaches";
	public static final String ExpireAllCaches = "expireAllCaches";
	public static final String DestroyAllCaches = "destroyAllCaches";
	public static final String DestroyAllSnapshots = "destroyAllSnapshots";
	public static final String CreateIncompatibleCache = "createIncompatibleCache";
	public static final String RunInfiniteLoopJavaProgramWithPersistentCache = "runInfiniteLoopJavaProgramWithPersistentCache";
	public static final String RunInfiniteLoopJavaProgramWithNonPersistentCache = "runInfiniteLoopJavaProgramWithNonPersistentCache";
	public static final String ExpireAllCachesWithTime = "expireAllCachesWithTime";
	public static final String RunSimpleJavaProgramWithAgentWithPersistentCache = "runSimpleJavaProgramWithAgentWithPersistentCache";
	public static final String RunSimpleJavaProgramWithAgentWithNonPersistentCache = "runSimpleJavaProgramWithAgentWithNonPersistentCache";
	public static final String RunHanoiProgramWithCache = "runHanoiProgramWithCache";
	public static final String CheckJavaVersion = "checkJavaVersion";
	
	private static final String CMD_PREFIX="cmd.";

	private static boolean isRealtimeDefined = false;
	
	static Properties config;
	
	/** 
	 * Load the properties from the configuration file - some of these can be overridden
	 * with -D - these are cacheDir and controlDir
	 */
	static {
		try {
				
			config = new Properties();
						
			// Load the target specific properties ... note that we default to windows ...
			String configFile = System.getProperty("test.target.config", "config.defaults");
			InputStream is = TestUtils.class.getClassLoader().getResourceAsStream(configFile);
			config.load(is);
					
			if (config.getProperty("logCommands","false").equalsIgnoreCase("true")) {
				RunCommand.logCommands=true;
			}
			
			if (config.getProperty("debug","false").equalsIgnoreCase("true")) {
				debug = true;
			}
			// Allow some sys props to override the config
			//String override = System.getProperty("cacheDir");
			//if (override!=null) config.put("cacheDir",override);
			//override = System.getProperty("controlDir");
			//if (override!=null) config.put("controlDir",override);
			//override = System.getProperty("defaultCacheLocation");
			//if (override!=null) config.put("defaultCacheLocation",override);

			// Check if the defaultCacheLocation was set, otherwise lets work it out
		    if (config.getProperty("defaultCacheLocation")==null) {
		    	if (config.getProperty("cacheDir")==null && config.getProperty("controlDir")==null) {
		    		System.out.println("Please set either defaultCacheLocation or cacheDir or controlDir in the config file or with -D!");
		    	}
		    }
		    
		    if (config.get("apploc")==null) {
		    	// work it out...
				URL url = TestUtils.class.getClassLoader().getResource("testcode.jar");
				String thePath = new File(url.getFile()).getParentFile().getAbsolutePath();
				thePath = thePath.replace('\\','/');
				config.put("apploc", thePath);
		    }

		    /* */
		    String javaTestCommand = System.getProperty("JAVA_TEST_COMMAND","");
		    String java5home = System.getProperty("JAVA5_HOME","");
		    String exesuff = System.getProperty("EXECUTABLE_SUFFIX","");
		    String realtime = System.getProperty("REALTIME","");
		    javaTestCommand=javaTestCommand.replace('\\','/');
		    java5home=java5home.replace('\\','/');
 
		    if (!realtime.equals("") && !realtime.equalsIgnoreCase("false")) {
		    	isRealtimeDefined = true;
		    }
		    
		    config.put("java_exe",javaTestCommand); 

		    config.put("java5_exe",java5home + "/" + "jre" + "/" + "bin" + "/" + "java"+exesuff);
		    config.put("java5c_exe",java5home + "/" + "bin" + "/" + "javac"+exesuff);
		    config.put("java5jar_exe",java5home + "/" + "bin" + "/" + "jar"+exesuff);

		    
		    config.put("cmd.runCompileSimpleApp","%java5c_exe% %1%/SimpleApp.java");
		    config.put("cmd.runCompileSimpleApp2","%java5c_exe% %1%/SimpleApp2.java");
		    config.put("cmd.runJarClassesInDir","%java5jar_exe% cf MySimpleApp.jar -C %1% .");
		    config.put("cmd.runProgramSimpleApp","%java_exe% "+(isRealtimeDefined ? "-Xrealtime " : "")+"-Xshareclasses:"+(isRealtimeDefined ? "grow," : "")+"persistent,name=%1%,verbose  -classpath %2% SimpleApp");
		    
		    System.out.println("\n--------- PROGRAM RUN COMMANDLINE INFO ---------");
		    System.out.println("java_exe =\t" + config.get("java_exe"));
		    System.out.println("cmd.runProgramSimpleApp =\t" + config.get("cmd.runProgramSimpleApp" + "\n"));
		    		    
		    if (config.getProperty("logCommands","false").equalsIgnoreCase("true")) {
				RunCommand.logCommands=true;
			}
		    		    
		    if (exesuff.equals(".exe")==true)
		    {
		    	String appdata = System.getenv("APPDATA");		 
		    	String defaultCacheLocation = new File(appdata+"\\..\\Local Settings\\Application Data").getAbsolutePath();
		    	config.put("defaultCacheLocation",defaultCacheLocation);		    		    	
		    }
		    else
		    {
		    	config.put("defaultCacheLocation","/tmp/"); 
		    }
		    
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	public static boolean realtimeTestsSelected(){
		return isRealtimeDefined;
	}
	
	public static void registerCommand(String key, String command) {
		config.put("cmd."+key,command);
	}
	
	public static String getCacheDir() { return (String)config.get("cacheDir");}
	public static void setCacheDir(String dir) {
		if (dir==null) config.remove("cacheDir");
		else {
			config.put("cacheDir",dir);
		}
	}
	
	public static String getControlDir() { return (String)config.get("controlDir");}
	public static void setControlDir(String dir) {
		if (dir==null) config.remove("controlDir");
		else config.put("controlDir",dir);
	}

	
	
	
	public static String getCommand(String commandKey) {
		return getCommand(commandKey,"","","");
	}
	
	public static String getCommand(String commandKey,String insert1) {
		return getCommand(commandKey,insert1,"","");
	}

	public static String getCommand(String commandKey,String insert1,String insert2) {
		return getCommand(commandKey,insert1,insert2,"");
	}
	
	public static String getCommand(String commandKey,String insert,String insert2,String insert3) {
		String theCommand=(String)config.get(CMD_PREFIX+commandKey);
		if (theCommand==null) {
			fail("Could not find command "+commandKey);
		}
		Enumeration keys = config.keys();
		while (keys.hasMoreElements()) {
			String k = (String)keys.nextElement();
			if (k.startsWith(CMD_PREFIX)) continue; // not something to apply
			String replaceVal = config.getProperty(k);
			theCommand = theCommand.replaceAll("%"+k+"%", replaceVal);
		}
		if (insert!=null) {
			
			// need to copy with one special case, where we are using a back level
			// VM to create an incompatible cache - since it wont understand cacheDir
			// yet we want to create the cache in a particular directory. we will
			// need to use controlDir in this case...
			if (commandKey.equals(CreateIncompatibleCache)) {
				if (config.get("cacheDir")!=null) {
					insert = insert+",controlDir="+config.getProperty("cacheDir"); // gotta use controlDir...
				}
				if (config.get("controlDir")!=null) {
					insert = insert+",controlDir="+config.getProperty("controlDir");
				}	
			} 
			else if (commandKey.equals("runCompileSimpleApp") || commandKey.equals("runCompileSimpleApp2") ||commandKey.equals("runJarClassesInDir")) 
			{
				
			}
			//getCacheFileName getCacheFileNameNonPersist
			else {
			
				// Insert %1% is always the cache name "name=%1%"
				// we can add subsequent options through a tiny extension to that
				if (config.get("cacheDir")!=null) {
					insert = insert+",cacheDir="+config.getProperty("cacheDir");
				}
				if (config.get("controlDir")!=null) {
					insert = insert+",controlDir="+config.getProperty("controlDir");
				}
			}
			theCommand = fixup(theCommand,"%1%",insert);
			theCommand = fixup(theCommand,"%2%",insert2);
			theCommand = fixup(theCommand,"%3%",insert3);
		}
		return theCommand;
	}
	
	public static String fixup(String what,String from,String to) {
		String result = what;
		int idx = what.indexOf(from);
		while (idx!=-1) {
			StringBuffer s = new StringBuffer(what.substring(0,idx));
			s.append(to);
			s.append(what.substring(idx+from.length()));
			what =  s.toString();
			idx = what.indexOf(from);
		}
		return what;
	}

	public static void createNonPersistentCache(String cachename) {
		log("Creating non-persistent cache '"+cachename+"'");
		RunCommand.execute(getCommand(CreateNonPersistentCache,cachename));
	}
	
	public static void createCacheSnapshot(String cachename) {
		log("Creating snapshot of cache'" + cachename + "'");
		String expectedErrorMessages[] = new String[] { 
			"JVMSHRC700I" /* Snapshot of non-persistent shared cache <cachename> has been created */ 
		};
		RunCommand.execute(getCommand(CreateCacheSnapshot, cachename), null, expectedErrorMessages, false, false, null);
	}

	public static void createPersistentCache(String cachename) {
		log("Creating persistent cache '"+cachename+"'");
		RunCommand.execute(getCommand(RunSimpleJavaProgram,cachename));
	}


	public static void listAllCaches() {
		RunCommand.execute(getCommand(ListAllCaches),false);
	}
	
	public static void expireAllCaches() {
		RunCommand.execute(getCommand(ExpireAllCaches),false);
	}
	
	public static void expireAllCachesWithTime(String timeInMins) {
		RunCommand.execute(getCommand(ExpireAllCachesWithTime, timeInMins),false);
	}
	
	public static void runDestroyAllCaches() {
		RunCommand.execute(getCommand(DestroyAllCaches),false);
	}
	
	public static void runDestroyAllSnapshots() {
		RunCommand.execute(getCommand(DestroyAllSnapshots), false);
	}

	public static void createIncompatibleCache(String cachename) {
		RunCommand.execute(getCommand(CreateIncompatibleCache,cachename));
	}

	public static void runSimpleJavaProgram(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgram,cachename));
	}
	
	public static void checkJavaVersion() {
		RunCommand.execute(getCommand(CheckJavaVersion), false);
	}
	
	/**
	 * @example runPrintStats("foo",false,"classpath");
	 * @example runPrintStats("foo",false,"url");
	 * @example runPrintStats("foo",false,"token");
	 * @example runPrintStats("foo",false,"romclass");
	 * @example runPrintStats("foo",true,"rommethod");
	 * @example runPrintStats("foo",true,"aot");
	 * @example runPrintStats("foo",true,"jitprofile");
	 * @example runPrintStats("foo",true,"zipcache");
	 */
	public static void runPrintStats(String cachename,boolean isPersistent, String option) {
		RunCommand.execute(getCommand(RunPrintStats,cachename,"="+option,(isPersistent?"":",nonpersistent")),false);
	}
	public static void runPrintStats(String cachename,boolean isPersistent) {
		RunCommand.execute(getCommand(RunPrintStats,cachename,"",(isPersistent?"":",nonpersistent")),false);
	}

	public static void runResetPersistentCache(String cachename) {
		RunCommand.execute(getCommand(RunResetPersistentCache,cachename),false);
	}
	
	public static void runResetNonPersistentCache(String cachename) {
		RunCommand.execute(getCommand(RunResetNonPersistentCache,cachename),false);
	}
	
	public static void runPrintAllStats(String cachename,boolean isPersistent) {
		RunCommand.execute(getCommand(RunPrintAllStats,cachename,(isPersistent?"":",nonpersistent")),false);
	}
	public static void runSimpleJavaProgramWithPersistentCache(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,cachename));
		checkOutputForDump(false);
	}
	public static void runSimpleJavaProgramWithPersistentCacheCheckStringTable(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCacheCheckStringTable,cachename));
		checkOutputForDump(false);
	}
	public static void runSimpleJavaProgramWithPersistentCache(String cachename,boolean careAboutExitValue) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,cachename),careAboutExitValue);
		checkOutputForDump(false);
	}
	public static void runSimpleJavaProgramWithPersistentCacheCheckForDump(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,cachename));
		checkOutputForDump(true);
	}
	public static void runSimpleJavaProgramWithPersistentCacheCheckForDump(String cachename, boolean careAboutExitValue) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,cachename), careAboutExitValue);
		checkOutputForDump(true);
	}
	/* waitForMessage is the message for which this process waits */
	public static void runInfiniteLoopJavaProgramWithPersistentCache(String cachename, String waitForMessage) {
		RunCommand.execute(getCommand(RunInfiniteLoopJavaProgramWithPersistentCache, cachename), waitForMessage);
		checkOutputForDump(false);
	}
	/* waitForMessage is the message for which this process waits */
	public static void runInfiniteLoopJavaProgramWithNonPersistentCache(String cachename, String waitForMessage) {
		RunCommand.execute(getCommand(RunInfiniteLoopJavaProgramWithNonPersistentCache, cachename), waitForMessage);
		checkOutputForDump(false);
	}

	/**
	 * Will execute a simple java program using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 */
	public static void runSimpleJavaProgramWithPersistentCache(String cachename,String extraSharedClassesOptions) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runSimpleJavaProgramWithPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""));
	}

	/**
	 * Will execute a simple java program using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 * @param waitForMessage message for which this process waits 
	 */
	public static void runInfiniteLoopJavaProgramWithPersistentCache(String cachename,String extraSharedClassesOptions,String waitForMessage) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runInfiniteLoopJavaProgramWithPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""), waitForMessage);
	}
	
	/**
	 * Will execute a simple java program using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 */
	public static void runSimpleJavaProgramWithNonPersistentCache(String cachename,String extraSharedClassesOptions) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runSimpleJavaProgramWithNonPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""));
	}
	
	/**
	 * Will execute a simple java program using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 * @param waitForMessage message for which this process waits 
	 */
	public static void runInfiniteLoopJavaProgramWithNonPersistentCache(String cachename,String extraSharedClassesOptions,String waitForMessage) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runInfiniteLoopJavaProgramWithNonPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""), waitForMessage);
	}
	
	protected static void checkOutputForDump(boolean dumpGenerated) {
		if (dumpGenerated) {
			checkOutputContains("JVMDUMP....", "The JVM is dumping!!");
		} else {
			checkOutputDoesNotContain("JVMDUMP....", "The JVM is dumping!!");
		}
	}

	public static void runSimpleJavaProgramWithPersistentCacheAndReset(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,cachename+",reset"));
	}
	
	public static void runSimpleJavaProgramWithNonPersistentCacheMayFail(String cachename) {
		RunCommand.executeMayFail(getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename));
	}
	
	public static void runSimpleJavaProgramWithNonPersistentCache(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename));
	}
	
	public static void runSimpleJavaProgramWithNonPersistentCache(String cachename,boolean careAboutExitValue) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename),careAboutExitValue);
	}

	public static void runSimpleJavaProgramWithNonPersistentCacheAndReset(String cachename) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename+",reset"));
	}
	/**
	 * @param cachename name of the cache to use
	 * @param agentName name of the JVMTI agent. Actually it needs to be one of the JVMTI test in com.ibm.jvmti.tests package.
	 * 					See TestSharedCacheEnableBCI.java for an example.
	 * @param agentArgs	arguments to JVMTI agent. Again, these are the arguments passed to JVMTI test in com.ibm.jvmti.tests package.
	 * 					See TestSharedCacheEnableBCI.java for an example.
	 */
	public static void runSimpleJavaProgramWithAgentWithPersistentCache(String cachename, String agentName, String agentArgs) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithAgentWithPersistentCache, cachename, agentName, agentArgs));
		checkOutputForDump(false);
	}
	/**
	 * Will execute a simple java program with a JVMTI agent using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 */
	public static void runSimpleJavaProgramWithAgentWithPersistentCache(String cachename, String extraSharedClassesOptions, String agentName, String agentArgs) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runSimpleJavaProgramWithAgentWithPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""),
				agentName, agentArgs);
	}
	/**
	 * @param cachename name of the cache to use
	 * @param agentName name of the JVMTI agent. Actually it needs to be one of the JVMTI test in com.ibm.jvmti.tests package.
	 * 					See TestSharedCacheEnableBCI.java for an example.
	 * @param agentArgs	arguments to JVMTI agent. Again, these are the arguments passed to JVMTI test in com.ibm.jvmti.tests package.
	 * 					See TestSharedCacheEnableBCI.java for an example.
	 */
	public static void runSimpleJavaProgramWithAgentWithNonPersistentCache(String cachename, String agentName, String agentArgs) {
		RunCommand.execute(getCommand(RunSimpleJavaProgramWithAgentWithNonPersistentCache, cachename, agentName, agentArgs));
		checkOutputForDump(false);
	}
	/**
	 * Will execute a simple java program with a JVMTI agent using the named cache, extra options like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string that is used to launch the program.
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 */
	public static void runSimpleJavaProgramWithAgentWithNonPersistentCache(String cachename, String extraSharedClassesOptions, String agentName, String agentArgs) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		runSimpleJavaProgramWithAgentWithNonPersistentCache(cachename+
				(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""),
				agentName, agentArgs);
	}
	
	/**
	 * Will execute a hanoi program using the named cache, extraSharedClassesOptions like "verbose,silent" can be specified
	 * and they will be appended to the constructed -Xshareclasses option string. extraJavaOpt like "-Xint" can also be 
	 * specified that is used to launch the program. 
	 * 
	 * @param cachename name of the cache to use
	 * @param extraSharedClassesOptions extra shared classes options, eg "verbose,silent"
	 */
	public static void runHanoiProgramWithCache(String cachename,String extraSharedClassesOptions, String extraJavaOpt) {
		// this hides the magic rule that due to the command construction, the "name=" is always the last part of the
		// -Xshareclasses string in the defined commands.  So to add more shared classes options we just have to append
		// them to the cache name.
		RunCommand.execute(getCommand(RunHanoiProgramWithCache, 
				cachename+(extraSharedClassesOptions!=null&&extraSharedClassesOptions.length()>0?","+extraSharedClassesOptions:""),
				(extraJavaOpt!=null&&extraJavaOpt.length()>0?" "+extraJavaOpt:"")
				));
		checkOutputForDump(false);
	}
	
	protected static String getCacheFileLocationForNonPersistentCache(String cachename) {
		String cacheDir = getCacheDir(cachename,false);		
		String expectedFileLocation = 
				cacheDir+File.separator+
				getCacheFileName(cachename,false);
		return expectedFileLocation;
	}
	protected static String getCacheFileDirectory(boolean forPersistentCache) {
		String defaultLoc = getConfigParameter("defaultCacheLocation");
		String cacheDirLoc = getConfigParameter("cacheDir");
		String controlDirLoc=getConfigParameter("controlDir");
		String theDir = null;
		if (cacheDirLoc!=null) theDir = cacheDirLoc+(forPersistentCache?"":File.separator+"javasharedresources");
		else if (controlDirLoc!=null) theDir = controlDirLoc+(forPersistentCache?"":File.separator+"javasharedresources");
		else theDir = defaultLoc+File.separator+"javasharedresources";
		return theDir;
	}

	protected static String getCacheFileLocationForPersistentCache(String cachename) {
		String cacheDir = getCacheDir(cachename,true);		
		String expectedFileLocation = 
				cacheDir+File.separator+
				getCacheFileName(cachename,true);
		return expectedFileLocation;
	}
	
	protected static String getCacheFileLocationForCacheSnapshot(String cachename) {
		String cacheDir = getCacheDir(cachename, false);		
		String expectedFileLocation = 
				cacheDir + File.separator +
				getSnapshotFileName(cachename);
		return expectedFileLocation;
	}

	public static void checkFileExists(String location) {
		  if (!new File(location).exists()) {
		  	  fail("No file found at '"+location+"'");
		  }
	  }

	public static void checkFileDoesNotExist(String location) {
		  if (new File(location).exists()) {
			  fail("Did not expect to find file at '"+location+"'");
		  }
	  }

	public static void fail(String msg) {
		showOutput();
		System.err.println(msg);
		System.err.println("\tLast command: "+RunCommand.lastCommand);
		throw new TestFailedException(msg);
	}

	private static String lastcmd_getCacheDir = "";
	private static String lastresult_getCacheDir = "";
	public static String getCacheDir(String cachename,boolean persistent) 
	{
		//NOTE: use above statics to save some time when running tests ...
		
		String cmd = "";
		
		if (persistent==true)
		{
			cmd = getCommand("getCacheFileName",cachename);
		}
		else
		{
			cmd = getCommand("getCacheFileNameNonPersist",cachename);
		}
		
		if (lastcmd_getCacheDir.equals(cmd) && lastresult_getCacheDir.equals("")!=false)
		{
			return lastresult_getCacheDir;
		}
		lastcmd_getCacheDir=cmd;

		RunCommand.execute(cmd,false);
		String stderr = RunCommand.lastCommandStderr;

		System.out.println("stderr is :"+ stderr);
		String[] stderrarray = stderr.split("\n");

		for (int j=0; j<stderrarray.length;j+=1)
		{
			String test = stderrarray[j];
			if (test.indexOf("/")==0 || test.indexOf(":\\")==1)
			{
				
				int i = test.lastIndexOf(java.io.File.separator);
				if (i<1)
				{
					fail("Error: for cachename "+cachename+" not sure what happened but i is "+i);
					return "";
				}
				String cacheDir = test.substring(0,i);//read just the dir
				System.out.println("HERE(getCacheDir):"+test+" "+cacheDir);
				
				lastresult_getCacheDir = cacheDir;
				return cacheDir;
			}
		}		
		fail("Error: should never hit this code.");
		return "";
	}
	
	
	private static String lastcmd_getCacheFileName = "";
	private static String lastresult_getCacheFileName = "";
	public static String getCacheFileName(String cachename,boolean persistent) {
	
		//NOTE: use above statics to save some time when running tests ...
				
		String cmd = "";
		
		if (persistent==true)
		{
			cmd = getCommand("getCacheFileName",cachename);
		}
		else
		{
			cmd = getCommand("getCacheFileNameNonPersist",cachename);
		}

		if (lastcmd_getCacheFileName.equals(cmd) && lastresult_getCacheFileName.equals("")!=false)
		{
			return lastresult_getCacheFileName;
		}
		lastcmd_getCacheFileName=cmd;
		
		RunCommand.execute(cmd,false);
		String stderr = RunCommand.lastCommandStderr;
		
		String[] stderrarray = stderr.split("\n");

		for (int j=0; j<stderrarray.length;j+=1)
		{
			String test = stderrarray[j];
			if (test.indexOf("/")==0 || test.indexOf(":\\")==1)
			{
				int i = test.lastIndexOf(java.io.File.separator);
				if (i>=test.length())
				{
					fail("Error: for cachename "+cachename+" not sure what happened but i is "+i);
					return "";
				}				
				String cachefile =test.substring(i+1,test.length());
				//System.out.println("HERE(getCacheFileName):"+test+" "+cachefile);
				lastresult_getCacheFileName = cachefile;
				return cachefile;
			}
		}		
		fail("Error: should never hit this code.");
		return "";
		
			
	  }

	public static String getSnapshotFileName(String snapshotname) {
		
		//NOTE: use above statics to save some time when running tests ...
				
		String cmd = getCommand("getSnapshotFileName", snapshotname);

		if (lastcmd_getCacheFileName.equals(cmd) && lastresult_getCacheFileName.equals("") != false) {
			return lastresult_getCacheFileName;
		}
		lastcmd_getCacheFileName = cmd;
		
		RunCommand.execute(cmd, false);
		String stderr = RunCommand.lastCommandStderr;
		
		String[] stderrarray = stderr.split("\n");

		for (int j = 0; j < stderrarray.length; j += 1) {
			String test = stderrarray[j];
			if (test.indexOf("/") == 0 || test.indexOf(":\\") == 1) {
				int i = test.lastIndexOf(java.io.File.separator);
				if (i >= test.length()) {
					fail("Error: for snapshotname " + snapshotname + " not sure what happened but i is " + i);
					return "";
				}				
				String snapshotfile = test.substring(i + 1, test.length());
				lastresult_getCacheFileName = snapshotfile;
				return snapshotfile;
			}
		}		
		fail("Error: should never hit this code.");
		return "";		
	  }

	protected static String getConfigParameter(String string) {
		  return (String)config.get(string);
	  }
	
	protected static void setConfigParameter(String key,String value) {
		if (value==null) config.remove(key);
		else config.put(key,value);
	}

	public static boolean destroyCache(String cachename) {
		RunCommand.execute(getCommand(DestroyCacheCommand,cachename),false);
		if (messageOccurred("JVMSHRC023E")) return false;
		else 								return true;
	}
	
	public static boolean destroyPersistentCache(String cachename) {
		RunCommand.execute(getCommand(DestroyPersistentCacheCommand,cachename),false);
		if (messageOccurred("JVMSHRC023E")) return false;
		else 								return true;
	}

	public static boolean destroyNonPersistentCache(String cachename) {
		RunCommand.execute(getCommand(DestroyNonPersistentCacheCommand,cachename),false);
		if (messageOccurred("JVMSHRC023E")) return false;
		else 								return true;
	}

	public static boolean messageOccurred(String msgId) {
		if (RunCommand.lastCommandStderr.indexOf(msgId)!=-1) return true;
		if (RunCommand.lastCommandStdout.indexOf(msgId)!=-1) return true;
		return false;
	}
	
	public static void showOutput() { 
      System.out.println("Command executed: '"+RunCommand.lastCommand+"'");
	  System.out.println("vvv STDOUT vvv");
	  System.out.println(RunCommand.lastCommandStdout);
	  System.out.println("^^^ STDOUT ^^^");
  	  System.out.println("vvv STDERR vvv");
	  System.out.println(RunCommand.lastCommandStderr);
	  System.out.println("^^^ STDERR ^^^");
	  System.out.println("vvv STACK TRACE vvv");
	  new Throwable().printStackTrace(System.out);
	  System.out.println("^^^ STACK TRACE ^^^");
	  
	  if (RunCommand.the2ndLastCommand != null) {
	      System.out.println("2nd last command executed: '"+RunCommand.the2ndLastCommand+"'");
		  System.out.println("vvv 2ND LAST STDOUT vvv");
  		  System.out.println(RunCommand.the2ndLastCommandStdout);
		  System.out.println("^^^ 2ND LAST STDOUT ^^^");
	  	  System.out.println("vvv 2ND LAST STDERR vvv");
  		  System.out.println(RunCommand.the2ndLastCommandStderr);
		  System.out.println("^^^ 2ND LAST STDERR ^^^");
	  }
	}

	public static void checkOutputContains(String regex,String explanation) {
		String[] lines = RunCommand.lastCommandStderrLines;
		boolean found = false;
		if (lines!=null) {
			for (int i = 0; i < lines.length; i++) {
				if (lines[i].matches("^.*"+regex+".*$")) {
//					System.out.println("this line '"+lines[i]+"' matches "+regex);
					found=true;
					break;
				}
			}
		}
		if (!found) { 
			// check stdout
		    lines = RunCommand.lastCommandStdoutLines;
		    found = false;
			if (lines!=null) {
				for (int i = 0; i < lines.length; i++) {
					if (lines[i].matches("^.*"+regex+".*$")) {
	//					System.out.println("this line '"+lines[i]+"' matches "+regex);
						found=true;
					}
				}
			}
		}
		if (!found) {
			fail(explanation);
		}
	}
	public static void checkOutputDoesNotContain(String regex,String explanation) {
		String[] lines = RunCommand.lastCommandStderrLines;
		boolean isFound = false;
		if (lines!=null) {
			for (int i = 0; i < lines.length; i++) {
				if (lines[i].matches("^.*"+regex+".*$")) {
//					System.out.println("this line '"+lines[i]+"' matches "+regex);
					isFound=true;
				}
			}
		}
		if (isFound) {
			fail(explanation);
		}
	}

	public static void checkForMessage(String msgid, String explanation) {
		  if (!messageOccurred(msgid)) {
			  fail(explanation);
		  }
	  }
	public static void checkNoFileForPersistentCache(String cachename) {
			checkFileDoesNotExist(getCacheFileLocationForPersistentCache(cachename));
	  }
	//TODO isnt this the same as that other method below?
	public static void checkNoFileForNonPersistentCache(String cachename) {
		if (isWindows()) {
			checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename));
		} else {	checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename));
			checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename).replace("memory","semaphore"));
		}
	  }
	
	public static void checkFileDoesNotExistForPersistentCache(String cachename) {
		checkFileDoesNotExist(getCacheFileLocationForPersistentCache(cachename));
	}
	
	public static void checkFileDoesNotExistForCacheSnapshot(String cachename) {
		checkFileDoesNotExist(getCacheFileLocationForCacheSnapshot(cachename));
	}
	
	public static void checkFileDoesNotExistForNonPersistentCache(String cachename) {
		if (isWindows()) {
		checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename));
		} else {
			checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename));
			checkFileDoesNotExist(getCacheFileLocationForNonPersistentCache(cachename).replace("memory","semaphore"));
		}
	}
	
	public static void checkFileExistsForPersistentCache(String cachename) {
			checkFileExists(getCacheFileLocationForPersistentCache(cachename));
	  }
	
	public static void checkFileExistsForNonPersistentCache(String cachename) {
		Properties p = System.getProperties();
		if (isWindows()) {
			checkFileExists(getCacheFileLocationForNonPersistentCache(cachename)); 
		} else {
			// linux is harder, there is a memory file and a semaphore file to worry about
			checkFileExists(getCacheFileLocationForNonPersistentCache(cachename));
			checkFileExists(getCacheFileLocationForNonPersistentCache(cachename).replace("memory","semaphore"));
		}
	}
	
	public static void checkFileExistsForCacheSnapshot(String cachename) {
		checkFileExists(getCacheFileLocationForCacheSnapshot(cachename));
	}
	
		
	public static boolean isWindows() {
		return System.getProperty("os.name").toLowerCase().startsWith("windows");
	}

	public static boolean isMVS() {
		return System.getProperty("os.name").toLowerCase().startsWith("z/os");
	}
	
	public static boolean isLinux() {
		return System.getProperty("os.name").toLowerCase().startsWith("linux");
	}
	
	public static String getOS() {
		return System.getProperty("os.name");
	}
	
	public static boolean isCompressedRefEnabled() {
		checkJavaVersion();
		if (messageOccurred("Compressed References")) {
			return true;
		} else {
			return false;
		}
	}
	
	public static void checkForSuccessfulPersistentCacheOpenMessage(
			String cachename) {
			checkOutputContains("JVMSHRC237I.*" + cachename,
					"Did not get expected message about successful cache open");
	}

	public static void checkForSuccessfulPersistentCacheCreationMessage(
			String cachename) {
			checkOutputContains("JVMSHRC236I.*" + cachename,
					"Did not get expected message about successful cache creation");
	}

	public static void checkForSuccessfulPersistentCacheDestroyMessage(
			String cachename) {
			checkOutputContains(cachename + ".*has been destroyed",
					"Did not get expected message about successful cache destroy");
	}

	public static void checkForSuccessfulNonPersistentCacheOpenMessage(
			String cachename) {
		checkOutputContains("JVMSHRC159I.*" + cachename,
				"Did not get expected message about successful cache open");
	}

	public static void checkForSuccessfulNonPersistentCacheCreationMessage(
			String cachename) {
		checkOutputContains("JVMSHRC158I.*" + cachename,
				"Did not get expected message about successful cache creation");
	}

	public static void checkForSuccessfulNonPersistentCacheDestoryMessage(
			String cachename) {
		checkOutputContains(cachename + ".*is destroyed",
				"Did not get expected message about successful cache destroy");
	}

	public static void checkOutputForCompatiblePersistentCache(String cachename,boolean shouldBeFound) {
		checkOutputForCompatibleCache(cachename, true, shouldBeFound);
	}
	
	public static void checkOutputForCompatibleNonPersistentCache(String cachename,boolean shouldBeFound) {
		checkOutputForCompatibleCache(cachename, false, shouldBeFound);
	}
	
	/**
	 * In the most recent command output (RunCommand.lastCommandStderrLines) this method checks for
	 * the named cache in the 'Incompatible caches' section.  The flag shouldBeFound indicates whether
	 * it should be named in the section.
	 */
	public static void checkOutputForIncompatibleCache(String name,boolean shouldBeFound) {
		  String[] lines = RunCommand.lastCommandStderrLines;
		  boolean isFound = false;
		  if (lines!=null) {
			  int i=0;
			  boolean inCompatSection = false;
			  while (i<lines.length) {
				  if (inCompatSection) {
					  // Ok, this might be our entry
					  if (lines[i].matches(name+"[ \\t].*$")) {
						  isFound=true;
					  }
				  }
				  if (lines[i].startsWith("Incompatible")) inCompatSection=true;
				  if (lines[i].trim().equals("")) inCompatSection=false;
				  i++;
			  }
		  }
		  if (shouldBeFound) {
			  if (!isFound) {
				  fail("Cache '"+name+"' was not found to be incompatible");
			  }
		  } else {
			  if (isFound) {
				  fail("Cache '"+name+"' was unexpectedly found to be incompatible");
			  }			  
		  }
	  }
	
	
	
	public static void checkOutputForCompatibleCache(String name, boolean isPersistent,boolean shouldBeFound) {
		  String[] lines = RunCommand.lastCommandStderrLines;
		  boolean isFound = false;
		  if (lines!=null) {
			  int i=0;
			  boolean inCompatSection = false;
			  while (i<lines.length) {
				  if (inCompatSection) {
					  // Ok, this might be our entry
					  if (lines[i].matches(name+".*"+(isPersistent?"(yes|persistent)":"(no|non-persistent)")+".*$")) {
						  isFound=true;
					  }
				  }
				  if (lines[i].startsWith("Compatible")) inCompatSection=true;
				  if (lines[i].trim().equals("")) inCompatSection=false;
				  i++;
			  }
		  }
		  if (shouldBeFound) {
			  if (!isFound) {
				  fail((isPersistent?"":"Non-")+"Persistent cache '"+name+"' was not found to be compatible");
			  }
		  } else {
			  if (isFound) {
				  fail((isPersistent?"":"Non-")+"Persistent cache '"+name+"' was unexpectedly found to be compatible");
			  }
		  }
	 }
	
	public static List processOutputForCompatibleCacheList() {
		List compatibleCaches = new ArrayList();
		String[] lines = RunCommand.lastCommandStderrLines;
		if (lines!=null) {
		  int i=0;
		  boolean inCompatSection = false;
		  while (i<lines.length) {
			  if (inCompatSection) {
				  // Chop from position 0 to the first non-space or tab
//				  System.out.println(">>>"+lines[i]+"<<<");
				  if (lines[i].length()!=0) { // this will avoid including the final line of compatible section
					  int p=0; while (p<lines[i].length() && lines[i].charAt(p)!=' ' && lines[i].charAt(p)!='\t') p++;
					  compatibleCaches.add(lines[i].substring(0,p));
				  }
			  }
			  if (lines[i].startsWith("Compatible")) inCompatSection=true;
			  if (lines[i].trim().equals("")) {
				  inCompatSection=false;
			  }
			  i++;
		  }
		}
		return compatibleCaches;
	}
	
	public static void deleteTemporaryDirectory(String path) {		
		File f = new File(path);
		if (f.exists()) {
			File[] files = f.listFiles();
			for (int i = 0; i < files.length; i++) {
				if (files[i].isDirectory()) {
					deleteTemporaryDirectory((files[i]).getAbsolutePath());
				} else {
					files[i].delete();
					if (files[i].exists()) {
						fail("Couldn't delete temp file " + files[i].getAbsolutePath());
					}
				}
			}
			f.delete();
		}		
		if (f.exists()) {
			fail("Couldn't delete temp dir " + f.getAbsolutePath());
		}
		//System.out.println("Deleted Tmp Folder: " + f.getAbsolutePath());
	}

	public static String createTemporaryDirectory(String testname) {
		try {
		  File f = File.createTempFile("testSC"+testname,"dir");

		  //System.out.println("Created Tmp Folder: "+f.getAbsolutePath());
		  
		  if (!f.delete()) fail("Couldn't create the temp dir");
		  if (!f.mkdir()) fail("Couldnt create the temp dir");
		  return f.getAbsolutePath();  
		} catch (IOException e) {
			fail("Couldnt create temp dir: "+e);
			return null;
		}
	  }
	
	/**
	   * Create some caches that are generation 07 and 01
	   */
	protected static void createGenerations01() {
		log("Creating cache generations...");
		createPersistentCache("Foo");	createPersistentCache("Bar");
		createNonPersistentCache("Foo");createNonPersistentCache("Bar");
		renameGenerations("G07","G01"); // rename generation 03 files to generation 02
		createPersistentCache("Foo");	createPersistentCache("Bar");
		createNonPersistentCache("Foo");createNonPersistentCache("Bar");
	}
	public static void log(String s) {
		if (debug) System.out.println(s);
	}
	
	/**
	 * Create some caches that are generation 03 and 02
	 */
	protected static void createGenerations02() {
		createPersistentCache("Foo");	createPersistentCache("Bar");
		createNonPersistentCache("Foo");createNonPersistentCache("Bar");
		renameGenerations("G07","G02"); // rename generation 03 files to generation 01
		
		createPersistentCache("Foo");	createPersistentCache("Bar");
		createNonPersistentCache("Foo");createNonPersistentCache("Bar");
		
	}

	public static void renameGenerations(String from, String to) {
		  renameGeneration("Foo",true,from,to);
		  renameGeneration("Bar",true,from,to);
		  renameGeneration("Foo",false,from,to);
		  renameGeneration("Bar",false,from,to);
	}

	/** 
	 * Passed a generation number like 'G07', it will change that to 'to' (eg. G02).
	 */
	public static void renameGeneration(String name, boolean isPersistent,
			String from, String to) {
	  log("Changing generation numbers for "+(isPersistent?"":"non-")+"persistent cache '"+name+"' from "+from+" to "+to);
	  if (isWindows() || isPersistent) {
		  String cachefilename = getCacheFileName(name, isPersistent); // TODO null will need fixing for linux and non-persistent caches
	  
		  String expectedFileLocation = null;
		  if (isPersistent) {
			  expectedFileLocation = getCacheFileLocationForPersistentCache(name);
		  } else {
			  expectedFileLocation = getCacheFileLocationForNonPersistentCache(name);
		  }
		  
//		  String expectedFileLocation = 
//			  getConfigParameter("defaultCacheLocation")+File.separator+
//			  "javasharedresources"+File.separator+cachefilename;
		  
		  File currentCache = new File(expectedFileLocation);
		  if (!currentCache.exists()) {
			  fail("Unexpectedly couldn't find cache file: "+expectedFileLocation);
		  }
		  
		  if (expectedFileLocation.endsWith(from)) {
			  String newLocation = expectedFileLocation.substring(0,expectedFileLocation.length()-from.length())+to;
			  if (!currentCache.renameTo(new File(newLocation))) {
				  fail("Could not rename from '"+expectedFileLocation+"' to '"+newLocation+"'");
			  }
		  }
	  } else {
		  // non-persistent caches on non-windows have two control files
		  String locMemory = getCacheFileLocationForNonPersistentCache(name);
		  File f = new File(locMemory);
		  if (!f.exists()) fail("Unexpectedly couldn't find cache file: "+locMemory);
		  if (locMemory.endsWith(from)) {
			  String newLocation = locMemory.substring(0,locMemory.length()-from.length())+to;
			  if (!f.renameTo(new File(newLocation))) {
				  fail("Could not rename from '"+locMemory+"' to '"+newLocation+"'");
			  }
		  }
		  String locSemaphore = getCacheFileLocationForNonPersistentCache(name).replace("memory","semaphore");
		  f = new File(locSemaphore);
		  if (!f.exists()) fail("Unexpectedly couldn't find cache file: "+locSemaphore);
		  if (locSemaphore.endsWith(from)) {
			  String newLocation = null;
			  if (to.equals("G01") || to.equals("G02")) {
				  // semaphore doesnt have generation number on for gen1/2
				  newLocation = locSemaphore.substring(0,locSemaphore.length()-from.length()-1);
			  } else {
				  newLocation = locSemaphore.substring(0,locSemaphore.length()-from.length())+to;
			  }
			  if (!f.renameTo(new File(newLocation))) {
				  fail("Could not rename from '"+locSemaphore+"' to '"+newLocation+"'");
			  }
		  }
	  }
  }
			//  
			//	public void touchFiles(String listForTouching) {
			//
			//		if (listForTouching.length()==0) return;
			//		StringTokenizer st = new StringTokenizer(listForTouching,",");
			//		try { Thread.sleep(1000); } catch (Exception e) {} // allow for filesystem with crap resolution timer
			//		while (st.hasMoreTokens()) {
			//			String touch = st.nextToken();
			//			File f = new File(touch);
			//			f.r
			//			if (!f.exists()) throw new RuntimeException("Can't find: "+touch);
			//			boolean b = f.setLastModified(System.currentTimeMillis());
			//			log("Touched: "+f+" : "+(b?"successful":"unsuccessful"));
			//		}
			//	}

	public static void corruptCache(String cachename, boolean isPersistent, String howToCorruptIt) {
		String theFile = (isPersistent?getCacheFileLocationForPersistentCache(cachename):
            getCacheFileLocationForNonPersistentCache(cachename));
		
		 //if (!isPersistent && !isWindows()) {
		 //		System.out.println("corruptCache is not implemented on this target");
		 //		return;
		 //}
		
		  if (howToCorruptIt.equals("truncate")) {
			  
			  File f = new File(theFile);
			  File fNew = new File(theFile+".tmp");
			  // chop it to 1000 bytes
			  int i =0;
			  byte[] buff = new byte[1000];
			  try {
				  FileInputStream fis = new FileInputStream(f);				  
				  FileOutputStream fos = new FileOutputStream(fNew);
				  while (i<10000) {
					  int read = fis.read(buff);					  
					  if (read>0)
					  {
						  i+=read;
						  fos.write(buff,0,read);
					  }
					  else
					  {
						  fail("Could not create the fake corrupt file.");
						  break;
					  }
				  }
				  fos.flush();
				  fos.close();
				  fis.close();
				  if (!f.delete()) fail("Failed to delete existing cache");
				  if (!fNew.renameTo(f)) fail("Could not rename new truncated cache");
			  } catch (Exception e) {
				  fail("Cannot truncate file "+e);
			  }
		  } else if (howToCorruptIt.equals("ftpascii")) {
			  File f = new File(theFile);
			  File fNew = new File(theFile+".tmp");
			  // chop it to 10000 bytes
			  int i =0;
			  byte[] buff = new byte[1000];
			  try {
				  FileInputStream fis = new FileInputStream(f);
				  FileOutputStream fos = new FileOutputStream(fNew);
				  int read=1;
				  while (true && read>0) {
					  read = fis.read(buff);
					  i+=read;
					  byte b = 0x7f;
					  if (read>-1) {
						  for (int j=0;j<read;j++) {
							  buff[j]=(byte)(buff[j]&b);
						  }
						  fos.write(buff,0,read);
					  }
				  }
				  fos.flush();fos.close();
				  fis.close();
//				  System.out.println("Copied "+i+" bytes");
				  if (!f.delete()) fail("Failed to delete existing cache");
				  if (!fNew.renameTo(f)) fail("Could not rename new ftpascii cache");
			  } catch (Exception e) {
				  e.printStackTrace();
				  fail("Cannot ftpascii file "+e);
			  }
		  } else if (howToCorruptIt.equals("zero")) {
			  File f = new File(theFile);
			  File fNew = new File(theFile+".tmp");
			  byte[] buff = new byte[1000];
			  try {
				  FileInputStream fis = new FileInputStream(f);
				  FileOutputStream fos = new FileOutputStream(fNew);
				  int read=1;
				  int blocksToZero = 200;
				  int offsetForZero=5000;
				  boolean corrupted=false;
				  
				  if (realtimeTestsSelected()){
					  offsetForZero=100000;
				  }
				 
				  while (read > 0) {
					  read = fis.read(buff);
					  offsetForZero-=read;
					  if (read > -1) {
						  if (offsetForZero<=0 && !corrupted) {
							  if ((blocksToZero--)<0) {
								  corrupted=true;
							  }
							  for (int j=0;j<read;j++) {
								  buff[j]=0;
							  }
						  }
						  fos.write(buff,0,read);
					  }
				  }
				  fos.flush();fos.close();
				  fis.close();
				  if (!f.delete()) fail("Failed to delete existing cache");
				  if (!fNew.renameTo(f)) fail("Could not rename new zerod cache");
			  } catch (Exception e) {
				  e.printStackTrace();
				  fail("Cannot zero file "+e);
			  }				
		  } else if (howToCorruptIt.equals("zeroStringCache")) {
			  File f = new File(theFile);
			  File fNew = new File(theFile+".tmp");
			  byte[] buff = new byte[1000];
			  try {
				  FileInputStream fis = new FileInputStream(f);
				  FileOutputStream fos = new FileOutputStream(fNew);
				  int read=1;
				  int blocksToZero = 3;
				  int offsetForZero=2000;
				  boolean corrupted=false;
				  
				  while (read > 0) {
					  read = fis.read(buff);
					  offsetForZero-=read;
					  if (read > -1) {
						  if (offsetForZero<=0 && !corrupted) {
							  if ((blocksToZero--)<0) {
								  corrupted=true;
							  }
							  for (int j=0;j<read;j++) {
								  buff[j]=0;
							  }
						  }
						  fos.write(buff,0,read);
					  }
				  }
				  fos.flush();fos.close();
				  fis.close();
				  if (!f.delete()) fail("Failed to delete existing cache");
				  if (!fNew.renameTo(f)) fail("Could not rename new zeroStringCache cache");
			  } catch (Exception e) {
				  e.printStackTrace();
				  fail("Cannot zeroStringCache file "+e);
			  }				
		  } else if (howToCorruptIt.equals("zeroFirstPage")) {
			  File f = new File(theFile);
			  File fNew = new File(theFile+".tmp");
			  byte[] buff = new byte[1024];
			  try {
				  FileInputStream fis = new FileInputStream(f);
				  FileOutputStream fos = new FileOutputStream(fNew);
				  int read=1;
				  boolean corrupted=false;
				  
				  while (read > 0) {
					  read = fis.read(buff);
					  if (read > -1) {
						  if (!corrupted) {
							  for (int j=0;j<read;j++) {
								  buff[j]=0;
							  }
							  corrupted = true;
						  }
						  fos.write(buff,0,read);
					  }
				  }
				  fos.flush();fos.close();
				  fis.close();
				  if (!f.delete()) fail("Failed to delete existing cache");
				  if (!fNew.renameTo(f)) fail("Could not rename new zerod cache");
			  } catch (Exception e) {
				  e.printStackTrace();
				  fail("Cannot zero file "+e);
			  }				
		  }
	  }
	
	protected static boolean copyFile(File from,File to) {
		int i =0;
		  byte[] buff = new byte[1000];
		  try {
			  FileInputStream fis = new FileInputStream(from);
			  FileOutputStream fos = new FileOutputStream(to);
			  int read=1;
			  while (true && read>0) {
				  read = fis.read(buff);
				  i+=read;
				  if (read>-1) fos.write(buff,0,read);
			  }
			  fos.flush();fos.close();
			  fis.close();
//			  System.out.println("Copied "+i+" bytes from "+from+" to "+to);
		  } catch (Exception e) {
			  fail("Cannot copy file '"+from+"' "+e);
		  }
		  return true;
	}
	public static void moveControlFilesForNonPersistentCache(String name,
			String whereFrom, String whereTo) {
				  if (isWindows()) {
					  File f = new File(whereFrom+File.separator+getCacheFileName(name,false));
					  File fNew = new File(whereTo+File.separator+getCacheFileName(name, false));
					  if (!f.renameTo(fNew)) fail("Could not rename the control file");
				  } else {
					  File f = new File(whereFrom+File.separator+getCacheFileName(name,false));
					  File fNew = new File(whereTo+File.separator+getCacheFileName(name, false));
					  if (!f.renameTo(fNew)) fail("Could not rename the memory file from "+f.getAbsolutePath()+" to "+fNew.getAbsolutePath());
					  
					  f = new File(whereFrom+File.separator+getCacheFileName(name,false).replace("memory", "semaphore"));
					  fNew = new File(whereTo+File.separator+getCacheFileName(name, false).replace("memory", "semaphore"));
					  if (!f.renameTo(fNew)) fail("Could not rename the semaphore file from "+f.getAbsolutePath()+" to "+fNew.getAbsolutePath());	  
				  }
			  }
	public static Set getSharedMemorySegments() {	
	  Set shmSegments = new HashSet();
	  RunCommand.execute("ipcs");
	  String[] lines = RunCommand.lastCommandStdoutLines;
	  if (lines!=null) {
		  int i=0;
		  boolean inSharedMemorySegmentSection = false;
		  while (i<lines.length) {
			  if (inSharedMemorySegmentSection) {
				  if (lines[i].startsWith("0x")) {
					  String shm = lines[i].substring(11,21).trim();
	//				  System.out.println("From line '"+lines[i]+"' we have "+shm);
					  shmSegments.add(shm);
				  }
			  }
			  if (lines[i].indexOf("Shared Memory Segments")!=-1) inSharedMemorySegmentSection=true;
			  if (lines[i].trim().equals("")) inSharedMemorySegmentSection=false;
			  i++;
		  }
	  }
	  return shmSegments;
	}
	public static Set getSharedSemaphores() {	
		  Set shmSegments = new HashSet();
		  RunCommand.execute("ipcs");
		  String[] lines = RunCommand.lastCommandStdoutLines;
		  if (lines!=null) {
			  int i=0;
			  boolean inSharedSemaphoresSection = false;
			  while (i<lines.length) {
				  if (inSharedSemaphoresSection) {
					  if (lines[i].startsWith("0x")) {
						  String sem = lines[i].substring(11,21).trim();
	//					  System.out.println("From line '"+lines[i]+"' we have "+sem);
						  shmSegments.add(sem);
					  }
				  }
				  if (lines[i].indexOf("Semaphore Arrays")!=-1) inSharedSemaphoresSection=true;
				  if (lines[i].trim().equals("")) inSharedSemaphoresSection=false;
				  i++;
			  }
		  }
		  return shmSegments;
		}
	
	public static boolean removeSemaphoreForCache(String cacheName) {
		StringBuffer removeSemCmd = new StringBuffer("ipcrm -s ");
		String semid = null;
		RunCommand.execute(getCommand(ListAllCaches), false);
		String[] lines = RunCommand.lastCommandStderrLines;
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].startsWith(cacheName)) {
				String token[] = lines[i].split("(\\s)+");
				/* verify cache name */
				if (token[0].trim().equals(cacheName) == false) {
					continue;
				}
				/* verify non-persistent cache */
				if ((token[3].trim().equals("no") == false) && (token[3].trim().equals("non-persistent") == false)) {
					continue;
				}
				/* get semaphore */
				semid = token[7].trim();
			}
		}
			
		if (semid != null) {
			removeSemCmd.append(semid);
			RunCommand.execute(removeSemCmd.toString());
			return true;
		} else {
			return false;
		}
	}
	
	public static boolean removeSharedMemoryForCache(String cacheName) {
		StringBuffer removeShmCmd = new StringBuffer("ipcrm -m ");
		String shmid = null;
		RunCommand.execute(getCommand(ListAllCaches), false);
		String[] lines = RunCommand.lastCommandStderrLines;
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].startsWith(cacheName)) {
				String token[] = lines[i].split("(\\s)+");
				/* verify cache name */
				if (token[0].trim().equals(cacheName) == false) {
					continue;
				}
				/* verify non-persistent cache */
				if ((token[3].trim().equals("no") == false) && (token[3].trim().equals("non-persistent") == false)) {
					continue;
				}
				/* get shared memory */
				shmid = token[6].trim();
			}
			
		}
		if (shmid != null) {
			removeShmCmd.append(shmid);
			RunCommand.execute(removeShmCmd.toString());
			return true;
		} else {
			return false;
		}
	}
	
	public static String[] getLastCommandStdout() {
		return RunCommand.lastCommandStdoutLines;
	}
	
	public static String[] getLastCommandStderr() {
		return RunCommand.lastCommandStderrLines;
	}
}
