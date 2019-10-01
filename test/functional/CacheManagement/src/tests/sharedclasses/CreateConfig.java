package tests.sharedclasses;

import java.io.FileWriter;

/**
 * Based on input parameters and environment variables, this will create a config.properties file suitable for 
 * running the tests.
 */
public class CreateConfig {

	public static void main(String[] args) throws Exception {

//		Properties p = System.getProperties();
//		p.list(System.out);
		
		// format is:
//		# Which java.exe to call
//		#java_exe=c:/andyc/j9vmwi3224/sdk/jre/bin/java.exe
//		#java_exe=c:/ipartrid/j9vmwi3224/jre/bin/java.exe
//		java_exe=c:/ben/j9vmwi3224/sdk/jre/bin/java.exe
//
//		# Default location for cache files and javasharedresources
//		defaultCacheLocation=C:/Documents and Settings/clemas/Local Settings/Application Data
//		# These are used if set, otherwise just the default is assumed
//		# cacheDir=
//		# controlDir=
//
//		# and a java for creating old incompatible cache files
//		java5_exe=c:/andyc/j9vmwi3223/sdk/jre/bin/java.exe
//
//		# If set, this will tell us what commands are executing
//		logCommands=true
		
		String javaForTesting = System.getProperty("testjava");
		String cacheDir = System.getProperty("cachedir");
		String java5 = System.getProperty("refjava");
		
		// make sure '/' the right way round!
		javaForTesting = javaForTesting.replace('\\','/');
		cacheDir = cacheDir.replace('\\','/');
		java5 = java5.replace('\\','/');
		
		FileWriter writer = new FileWriter("config.properties");
		writer.write("# Java to be tested\n");
		writer.write("java_exe="+javaForTesting+"\n\n");
		writer.write("# Cache directory\n");
		writer.write("cacheDir="+cacheDir+"\n\n");
		writer.write("# Old jdk for creating old caches\n");
		writer.write("java5_exe="+java5+"\n");
		writer.flush();
		writer.close();
	}
	
}
