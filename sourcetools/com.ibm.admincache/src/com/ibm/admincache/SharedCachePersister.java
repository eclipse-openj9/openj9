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
import java.io.PrintStream;
import java.util.LinkedList;
import java.util.Vector;
import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.util.jar.*;
import java.io.IOException;

import com.ibm.oti.shared.*;
import java.net.*;

/**
 * @author Mark Stoodley
 */

class SharedCachePersister extends JarPersister {
	public SharedCachePersister()
	{
		_config = new SharedCacheConfig();
		_config.setRequiresRealTimeEnvironment(false);
		_config.setNeedOutPath(false);
	}

	
	public SharedCacheConfig sharedCacheConfig() { return (SharedCacheConfig) _config; };
	
	public void updateInternalState() {}
	

	public void prepareForAction() {
	}

	private Vector<String> javaCommand(Vector<String> cmd) {
		String javaHome = System.getProperty("java.home");
		StringBuffer javaCmd = new StringBuffer(javaHome).append(File.separator).append("bin").append(File.separator).append("java");

		if (sharedCacheConfig().nativeDebugger()) {
			String devenvPath = System.getenv("VSINSTALLDIR");
			if (devenvPath != null) {
				cmd.add(new StringBuffer(devenvPath).append(File.separator).append("devenv").toString());
				cmd.add(new String("/debugexe"));
			}
			else {
				cmd.add(new String("gdb"));
				cmd.add(javaCmd.toString());
				cmd.add(new String("--args"));
			}
		}
		
		cmd.add(javaCmd.toString());
		
		if (sharedCacheConfig().debugger())
			cmd.add(new String("-Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=8096"));
		
		return cmd;
	}
	
	private String[] convertToStringArray(Vector<String> v) {
		String[] array=new String[v.size()];
		if (config().TRACE_DEBUG())
			System.out.println("convertToStringArray (" + v.size() + " arguments");
		int position = 0;
		for (String option: v) {
			if (config().TRACE_DEBUG())
				System.out.println("\t[" + position + "]\t" + option);
			array[position++] = option;
		}
		return array;
	}
	
	private int executeSharedCacheUtility(String cmdOption) {
		return executeSharedCacheUtility(cmdOption, System.out, System.err);
	}
	
	private int executeSharedCacheUtility(String cmdOption, OutputStream outStream, OutputStream errStream) {
		Vector<String> scCmdLine = javaCommand(new Vector<String>());
		if (Config.realTimeEnvironment())
			scCmdLine.add(new String("-Xrealtime"));
		
		StringBuffer scArg = new StringBuffer("-Xshareclasses:").append(cmdOption);
		if (sharedCacheConfig().cacheName() != null) {
			scArg.append(",").append("name=").append(sharedCacheConfig().cacheName());
		}
		if (sharedCacheConfig().cacheDir() != null) {
			scArg.append(",").append("cacheDir=").append(sharedCacheConfig().cacheDir());
		}
		scCmdLine.add(scArg.toString());
		
		try {
			if (config().debug()) {
				dumpCommandline(System.out, scCmdLine);
			}
			Process proc = Runtime.getRuntime().exec(convertToStringArray(scCmdLine));
			CaptureThread ct;
			CaptureThread ce;
			ct = new CaptureThread(proc.getInputStream(), outStream);
			ce = new CaptureThread(proc.getErrorStream(), errStream);
			ct.start();
			ce.start();
        
        	proc.waitFor();
			proc.exitValue();
			
			ct.join();
			ce.join();
		}
		catch (IOException ioe) {
			System.err.println("Error executing shared cache utility: " + cmdOption);
			return Main.errorRunningSharedCacheUtility;
		}
		catch (InterruptedException ie) {
			System.err.println("Error: interrupted while performing shared cache utility:" + cmdOption);
			return Main.interruptedRunningSharedCacheUtility;
		}
		if (SharedCacheConfig.commandError()) {
			return Main.errorRunningSharedCacheUtility;
		}
		
		return 0;
	}


	static void dumpCommandline(PrintStream outStream, Vector <String> cmdLine) {
		outStream.println("cmd is:");
		String separator = "";
		for (String s: cmdLine) {
			outStream.print(separator);
			outStream.print(s);
			separator = " \\\n";
		}
		outStream.print('\n');

	}
	
	private long _cacheSizeInMB = 0L;
	private void setCacheSize() {
		sharedCacheConfig().setCacheMaxSize(new String("-Xscmx" + _cacheSizeInMB + "M"));
		if (sharedCacheConfig().TRACE_DEBUG())
			System.out.println("setCacheSize: " + sharedCacheConfig().cacheMaxSize());
		if (_cacheSizeInMB > 700) {
			System.out.println("NOTE: predicted cache size (" + _cacheSizeInMB + "MB) exceeds recommended maximum shared class cache size of 700MB");
			System.out.println("If your jar files contain primarily class files then you may not be able to create a cache of this size");
			System.out.println("\tor you may not be able to connect to the created cache when you run your application.");
			if (sharedCacheConfig().aot()) {
				if (sharedCacheConfig().aotFilterFile() == null)
					System.out.println("Alternatively, you may want to more selectively compile AOT methods by using -aotFilterFile");
				else
					System.out.println("This cache size prediction does *not* account for your -aotFilterFile setting.");
			}
			System.out.println("To override this warning message, please directly specify " + sharedCacheConfig().cacheMaxSize() + " on your command-line");
			System.out.println("\tbut beware that the resulting failure may not occur until the very end of the population procedure.");
			System.exit(Main.cacheSizeWarning);
		}
	}
	private long roundToMB(long size) {
		return (size + (1L<<20) - 1L) >> 20;
	}
	
	private void pickInitialCacheSize(Vector <String>sourceFiles) {
		// if user set a cache size, don't override it
		if (sharedCacheConfig().explicitCacheSize())
			return;
		
		if (sharedCacheConfig().testGrow()) {
			// MGS test
			_cacheSizeInMB=5L;
			setCacheSize();
			return;
		}
		
	    // rule of thumb: combined size of jar files * 15, rounded up to MB
		long totalBytes = 0;
		if (sharedCacheConfig().TRACE_DEBUG())
			System.out.println("Starting to preduct cache size, start size is " + totalBytes);
		for (String fileName: sourceFiles) {
			File file = new File(fileName);
			
			if (sharedCacheConfig().TRACE_DEBUG())
				System.out.println("Adding size of " + fileName + ": " + file.length());
			totalBytes += file.length();
		}
		
		long multiplier=4L;
		if (sharedCacheConfig().aot())
				multiplier = 15L;
		
		_cacheSizeInMB += multiplier * roundToMB(totalBytes);
		if (sharedCacheConfig().TRACE_DEBUG())
			System.out.println("Total bytes is " + totalBytes + ", initial cache size is " + _cacheSizeInMB);
		setCacheSize();
	}
	
	private boolean growCacheSize() {
		if (_cacheSizeInMB > 0) {
			_cacheSizeInMB += _cacheSizeInMB;
			setCacheSize();
			return true;
		}
		return false;
	}
	
	private int startPopulationCommand() {
		if (!Config.realTimeEnvironment()) {
			System.err.println("-populate action currently only supported in real-time environments");
			return Main.populateNeedsRealTimeEnv;
		}
		if (sharedCacheConfig().classPath() == null) {
			System.err.println("-populate action requires -classpath <class path> option to be specified");
			return Main.populateNeedsClassPath;
		}
		
		SharedCacheConfig.setPopulating(true);
		
		_cacheSizeInMB = 0L;
		if (sharedCacheConfig().grow()) {
			try {
				File tempFile = File.createTempFile("admincache", "_printAllStats.tmp");
				sharedCacheShutDownHook scsdh = new sharedCacheShutDownHook(tempFile);
				Runtime.getRuntime().addShutdownHook(scsdh);

				FileOutputStream tempFileOutputStream = new FileOutputStream(tempFile);
				int rc = executeSharedCacheUtility("printAllStats", tempFileOutputStream, tempFileOutputStream);
				if (rc != 0)
					return Main.unableToGrowCache;
				
				sharedCacheConfig().setPrintAllStatsFileName(tempFile.getAbsolutePath());

				// now need to go find the size of this cached
				FileReader reader = new FileReader(tempFile);
				BufferedReader tempFileReader = new BufferedReader(reader);
				String line = tempFileReader.readLine();
				while (line != null) {
					String cacheSizeSearchString = "cache size                           = ";
					if (line.startsWith(cacheSizeSearchString)) {
						if (sharedCacheConfig().TRACE_DEBUG())
							System.out.println("found cache size line " + line);
						_cacheSizeInMB = roundToMB((long)Integer.parseInt(line.substring(cacheSizeSearchString.length())));
						if (sharedCacheConfig().TRACE_DEBUG())
							System.out.println("extracted size " + _cacheSizeInMB);
						break;
					}
					line = tempFileReader.readLine();
				}
		    } catch (IOException e) {
		    	System.err.println("Internal Error: unable to create temporary file for cache growing");
		    	return Main.unableToGrowCache;
		    }
		}
		
		Vector<String> sourceFiles = null;
		if (config().bSearchPath()) {
  	  		sourceFiles = new Vector<String>();
  			sourceFiles.addAll(new JarFinder(this).searchForFiles(config().searchPath(), config().fileTypes(), config().fileNames(), true));
		} else {
			sourceFiles = config().fileNames();
		}
		
		pickInitialCacheSize(sourceFiles);
		
		int rc = 0;
		do {
			Vector <String> cmd = javaCommand(new Vector<String>());
			cmd = sharedCacheConfig().buildJavaVmOptions(cmd);
			cmd.add(new String("com.ibm.admincache.SharedCachePersister"));
			cmd.add(new String("-populate"));
			cmd = sharedCacheConfig().buildApplicationOptions(cmd);
	   	
			try {
				if (config().debug())
					dumpCommandline(System.out, cmd);
		   	
				Process proc = Runtime.getRuntime().exec(convertToStringArray(cmd));
				CaptureThread ct = new CaptureThread(proc.getInputStream(),System.out);
				CaptureThread ce = new CaptureThread(proc.getErrorStream(), System.err);
				ct.start();
				ce.start();
				proc.waitFor();
				rc = proc.exitValue();
				ct.join();
				ce.join();
			} catch (IOException ioe) {
				System.err.println("Error invoking admincache");
				System.exit(Main.cantInvokeAdmincacheError);
			} catch (InterruptedException ioe) {
				System.err.println("Interrupted while invoking admincache");
				System.exit(Main.interruptedDuringAdmincacheError);
			}
			
			if (SharedCacheConfig.fullCacheMessageFound()) {
				if (growCacheSize()) {
					SharedCacheConfig.setFullCacheMessageFound(false);
					System.out.println("Initial cache size not large enough");
					System.out.println("Growing cache size and restarting population procedure");
					continue;
				}
				else {
					System.err.println("Shared cache size specified isn't large enough.");
					System.err.println("   Please set a larger size or no size to let " + config().myName() + " search for the right size");
					break;
				}
			}
			else
				break;
		}
		while (true);
		
		if (rc == 0 && sharedCacheConfig().printVMArgs()) {
			Vector vmargs = sharedCacheConfig().buildVMArgs(new Vector<String>());
			System.out.print("VM args needed at runtime: ");
			for (int opt=0;opt < vmargs.size()-1;opt++) {
				String option=(String)vmargs.elementAt(opt);
				System.out.print(option);
				System.out.print(' ');
			}
			
			if (vmargs.size() > 0) {
				String option=(String)vmargs.lastElement();
				System.out.println(option);
			}
			System.out.println("");
		}

		return rc;
	}
	
	public int performAction() {
		prepareForAction();
		      
		switch (sharedCacheConfig().action()) {
		case SharedCacheConfig.ACTION_DESTROY :
			return executeSharedCacheUtility("destroy");
		case SharedCacheConfig.ACTION_DESTROYALL :
			return executeSharedCacheUtility("destroyAll");
		case SharedCacheConfig.ACTION_LISTALLCACHES :
			return executeSharedCacheUtility("listAllCaches");
		case SharedCacheConfig.ACTION_PRINTSTATS :
			return executeSharedCacheUtility("printStats");
		case SharedCacheConfig.ACTION_PRINTALLSTATS :
			return executeSharedCacheUtility("printAllstats");
		case SharedCacheConfig.ACTION_POPULATE :
			return startPopulationCommand();
		default :
			System.err.println("Error: no action specified");
			sharedCacheConfig().usage();
			return -1;
		}
	}
	
	private int copyJarFile(File origFile, String outputFileName) {
		// create output jar file as copy of original file
		try {
			String copyCmd="cp -f " + origFile.getPath() + " " + outputFileName;
			Process proc = Runtime.getRuntime().exec(copyCmd);
			if (config().debug())
				System.out.println("Copy cmd: "+copyCmd);
			CaptureThread ct = new CaptureThread(proc.getInputStream());
			CaptureThread ce = new CaptureThread(proc.getErrorStream());
			ct.start();
			ce.start();
			proc.waitFor();
			proc.exitValue();
			ct.join();
			ce.join();
        
		} catch (IOException ioe) {
			System.err.println("Error creating output file: " + outputFileName);
			return Main.fileToConvertError;
		} catch (InterruptedException ioe) {
			System.err.println("Interrupted while creating output file: " + outputFileName);
			return Main.fileToConvertError;
		}
		
		return 0;
	}
	private boolean checkEntryIsClass(ZipEntry ze)
    	{
		return checkIsClass(ze.getName());
    	}

	private boolean checkIsClass(String s)
		{
		return (s.endsWith(".class"));
    	}

	private String className(String fileName)
		{
		return fileName.substring(0,fileName.indexOf(".class")).replace('/','.');
		}

	private Vector<Class> _loadedClasses = null;
	
	URLClassLoader _urlClassLoader = null;
	URL _url = null;
	SharedClassURLHelper _helper = null;
	ZipClassLoader _zcl = null;
	
	private Class loadClass(String className, boolean compileClassMethods) {
		if (_helper == null)
			return null;
		
		Class clazz=null;
		try {
			if (_zcl == null) // no zip/jar file to look in, so just try the usual way
				clazz = Class.forName(className);
			else {
				clazz=_zcl.loadClass(className);
			}
			
			if (clazz != null) {
				_helper.storeSharedClass(_url, clazz);
				if (sharedCacheConfig().aot()) {
					if (compileClassMethods) {
						if (sharedCacheConfig().COMPILE_AFTER_LOADING())
							_loadedClasses.add(clazz);
						else
							compileMethods(clazz);
					}
				}
			}
		}
		catch (ClassFormatError e) {
			if (config().TRACE_DEBUG()) {
				System.out.println("caught ClassFormatError: " + className);
				e.printStackTrace();
			}
		}
		catch (NoClassDefFoundError e) {
			if (config().TRACE_DEBUG()) {
				System.out.println("caught NoClassDefFoundError: " + className);
				e.printStackTrace();
			}
		}
		catch (UnsatisfiedLinkError e) {
			if (config().TRACE_DEBUG()) {
				e.printStackTrace();
				System.out.println("caught UnsatisfiedLinkError: " + className);
			}
		}
		catch (IncompatibleClassChangeError e) {
			if (config().TRACE_DEBUG()) {
				e.printStackTrace();
				System.out.println("caught IncompatibleClassChangeError: " + className);
			}
		}
		catch (SecurityException e) {
			if (config().TRACE_DEBUG()) {
				e.printStackTrace();
				System.out.println("caught SecurityException: " + className);
			}
		}
		catch (ClassNotFoundException e) {
			if (config().TRACE_DEBUG()) {
				e.printStackTrace();
				System.out.println("caught ClassNotFoundException: " + className);
			}
		}
		return clazz;
    }
	
	private Vector<String> _classNamesFromFilterFile = null;
	private void readClassFilterFile(File filterFile) {
		FileReader filterFileReader = null;
		try {
			filterFileReader = new FileReader(filterFile);
		} catch (Exception e) {
			System.err.println("Error opening class filter file: " + filterFile.getName());
			System.exit(Main.errorOpeningClassFilterFile);
		}
		
		// 256-character buffer should be sufficient
		BufferedReader br = new BufferedReader(filterFileReader, 256);
		try {
			_classNamesFromFilterFile = new Vector<String>();
			String line;
			while ((line = br.readLine()) != null) {
				if (true /*line.matches("class load: [\\w/\\$]*$")*/)
					_classNamesFromFilterFile.add(line.substring(12).replace('/', '.')); // 12 == start of the class name
				else {
					System.err.println("Improperly formed class found in class filter file: " + line);
					System.exit(Main.badClassFilterFile);
				}
			}
		} catch (Exception e) {
			System.err.println("Error reading class filter file");
			System.exit(Main.errorReadingClassFilterFile);
		}
		
	}
	
	private boolean matchesClassFilters(String className) {
		String classFilter = sharedCacheConfig().classFilter();
		File classFilterFile = sharedCacheConfig().classFilterFile();
		
		/*
		if (classFilter != null && !className.matches(classFilter))
			return false;
		*/
		
		if (classFilterFile != null) {
			if (_classNamesFromFilterFile == null) {
				readClassFilterFile(classFilterFile);
			}
			
			boolean matches = false;
			for (String filter: _classNamesFromFilterFile) {
				if (className.equals(filter)) {
					matches = true;
					break;
				}
			}
			if (!matches)
				return false;
		}
		return true;
	}

	private JarInputStream setUpJarFile(File file) throws Exception {
		_url = file.toURL();
		BufferedInputStream bis = new BufferedInputStream(new FileInputStream(file));
		JarInputStream jis = new JarInputStream(bis);
		_zcl = new ZipClassLoader(jis);
		_helper = null;
		try {
			_helper = Shared.getSharedClassHelperFactory().getURLHelper (_zcl);
		} catch (HelperAlreadyDefinedException e) {
		}
		return jis;
	}
	
	public int repopulateFromJar(File jarFile, LinkedList <String> classes, Hashtable<String, LinkedList> methodsFromClass) {
		int rc = 0;

		try {
			if (jarFile.isFile())
				setUpJarFile(jarFile);
			for (String className: classes) {
				if (sharedCacheConfig().TRACE_DEBUG())
					System.out.println("Populating current cache with previous cache's class " + className);
				loadClass(className, false);
				LinkedList methodList = methodsFromClass.get(className);
				if (methodList != null) {
					Iterator methodIter = methodList.iterator();
					while (methodIter.hasNext()) {
						String methodName = (String) methodIter.next();
						String methodSig = (String) methodIter.next();
						if (sharedCacheConfig().TRACE_DEBUG())
							System.out.println("Populating current cache with previous cache's AOT method name " + methodName + " sig " + methodSig);
						Compiler.command("TR_COMPILEMETHOD " + className.replace('.', '/') + " " + methodName + " " + methodSig);
					}
				}
				else if (sharedCacheConfig().TRACE_DEBUG())
					System.out.println("\tNo AOT methods from this class to compile");
				if (SharedCacheConfig.fullCacheMessageFound())
					return 0;	// just end early so we can restart as quickly as possible
				
			}

		} catch (Exception e) {
			System.err.println("IO Error reading file " + jarFile.getPath() + " to grow existing cache");
			rc = Main.fileToConvertError;
			return rc;
		}

		return rc;
	}
	
	public int convertJar(File origFile, String outputFileName) {
		int rc = 0;

        if (config().verbose()) {
			  System.out.print("Processing classes in " + origFile.getPath() + " into shared class cache ");
			  if (sharedCacheConfig().cacheName() != null)
				  System.out.print(sharedCacheConfig().cacheName());
			  System.out.println("");
        }

		if (_loadedClasses == null)
			_loadedClasses = new Vector<Class>();
		else
			_loadedClasses.clear();
				
		try {
			JarInputStream zis = setUpJarFile(origFile);
			JarEntry ze = zis.getNextJarEntry();
			
			boolean cachedSomeClass = false;
			while (ze != null) {
				if (config().noisy())
					System.out.print("Processing: " + ze.getName());
				
				if (checkEntryIsClass(ze)) {
					_zcl.setZipEntry(ze);
					String className = className(ze.getName());
					if (matchesClassFilters(className)) {
						Class clazz = loadClass(className, true);
						if (clazz == null) {
							if (config().noisy())
								System.out.println(" (could not add to cache because class could not be loaded)");
						} else {
							cachedSomeClass = true;
							if (config().noisy())
								System.out.println(" (adding to cache)");
						}
					}
					else {
						if (config().noisy())
							System.out.println(" (no filter match: not adding to cache)");
					}
				}
				else if (config().noisy())
					System.out.println("");
				
				ze = zis.getNextJarEntry();
			}
			
			_zcl = null;

			zis.close();

			if (!cachedSomeClass) {
				System.out.println("\t(Found no classes in this file to populate into cache)");
				return 1;
			}

			if (sharedCacheConfig().COMPILE_AFTER_LOADING() && sharedCacheConfig().aot())
				rc = compileMethods();
			
			if (rc != 0) {
				return Main.aotCodeCompilationFailure;
			}
			
		} catch (Exception e) {
			System.err.println("IO Error reading file " + origFile.getPath());
			rc = Main.fileToConvertError;
			return rc;
		}
		
		return 0;
	}
	
	private int compileMethods(Class clazz) {
    	if (config().TRACE_DEBUG())
    		System.out.println("\tCompiling methods in class " + clazz.getName());
    	Compiler.compileClass(clazz);
    	return 0;
	}
	
	private int compileMethods() {
	    for (Class clazz: _loadedClasses) {
	    	compileMethods(clazz);
       }
		return 0;
	}
	
	private void loadClassesInOriginalCache(Hashtable<String, LinkedList> jarFiles, Hashtable<String, LinkedList> classesInCache) {
		for (String jarFileEntry: jarFiles.keySet()) {
			try {
				if (sharedCacheConfig().TRACE_DEBUG())
					System.out.println("Populating new cache from classes in jar file " + jarFileEntry);
				File jarFile = new File(jarFileEntry);
				LinkedList<String> classList = jarFiles.get(jarFileEntry);
				repopulateFromJar(jarFile, classList, classesInCache);
			}
			catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
	
	private String extractMethodName(String line, String lineMarker) {
		int rommethodStart = line.indexOf(lineMarker);
		int methodNameStart = rommethodStart + lineMarker.length();
		String signatureSearchString = " Signature: ";
		int methodNameEnd = line.indexOf(signatureSearchString);
		String methodName = line.substring(methodNameStart, methodNameEnd);
		return methodName;
	}

	private String extractMethodSig(String line, String lineMarker) {
		String signatureSearchString = " Signature: ";
		int methodNameEnd = line.indexOf(signatureSearchString);
		int signatureStart = methodNameEnd + signatureSearchString.length();
		String signatureEndSearchString = " Address: ";
		int signatureEnd = line.indexOf(signatureEndSearchString);
		String signature = line.substring(signatureStart, signatureEnd);
		return signature;
	}
	
	private String extractAddress(String line) {
		String addressSearchString = " Address: ";
		int addressStart = line.indexOf(addressSearchString);
		return line.substring(addressStart + addressSearchString.length());
	}
	
	private int processPrintAllStatsClasses() {
		Hashtable<String, Vector> classPathEntries = new Hashtable<String, Vector>();

		try {
			boolean inClassPath = false;
			String currentClassPathAddress = null;

			/* Do two passes through the printStats output. Read the CLASSPATH entries first, since
			 * due to organization in cachelets a ROMCLASS using a CLASSPATH may be found in the printStats
			 * output before the CLASSPATH itself.
			 */
			FileReader reader = new FileReader(sharedCacheConfig().printAllStatsFileName());
			BufferedReader tempFileReader = new BufferedReader(reader);
			String data = tempFileReader.readLine();
			while (data != null) {
				if (inClassPath) {
					// this record is a class path entry
					if (data.charAt(0) != '\t') {
						// class path entries have tab as first char
						// although other lines have tab as first char, they cannot directly follow a class path entry
						// (some other line that doesn't start with tab will always come first)
						inClassPath = false;
					}
					else {
						String classPathEntry = data.substring(1);
						File cpFile = new File(classPathEntry);
						if ((!cpFile.isFile() && !cpFile.isDirectory()) || !cpFile.canRead()) {
							System.err.println("Cannot locate class path entry found in current cache " + classPathEntry);
							System.err.println("Cannot grow cache because not all classes in cache can be loaded");
							return Main.unableToGrowCache;
						}

						classPathEntries.get(currentClassPathAddress).add(classPathEntry);
						
						data = tempFileReader.readLine();
						continue;
					}
				}
				
				if (data.indexOf(" CLASSPATH") > 0 && data.indexOf(File.separatorChar) < 0) {
					// looking for separator char should prevent falsely identifying a directory that happens to have "CLASSPATH" in it
					inClassPath = true;
					int addressStart = data.indexOf("0x");
					String dataWithoutIndex = data.substring(addressStart);
					currentClassPathAddress = data.substring(addressStart,addressStart + dataWithoutIndex.indexOf(' '));
					classPathEntries.put(currentClassPathAddress,new Vector());
					data = tempFileReader.readLine();
					continue;
				}
				
				data = tempFileReader.readLine();
			}
			tempFileReader.close();
		}
		catch (IOException ioe) {
			System.err.println("Internal error: unable to grow cache");
			return Main.unableToGrowCache;
		}

		try {
			Hashtable<String, LinkedList> populateJarFiles = new Hashtable<String, LinkedList>();
			Hashtable<String, LinkedList> classesInCache = new Hashtable<String, LinkedList>();
			Hashtable<String, String> classPathEntryForClass = new Hashtable<String, String>();
			Hashtable<String, String> romMethodSigs = new Hashtable<String, String>();
			Hashtable<String, String> romMethodNames = new Hashtable<String, String>();
			Hashtable<String, String> romClasses = new Hashtable<String, String>();
			LinkedList<String> unknownAotMethods = new LinkedList<String>();
			String currentClassName = null;

			/* The CLASSPATH entries have been read in the first pass. Do a second pass to read
			 * everything else. 
			 */
			FileReader reader = new FileReader(sharedCacheConfig().printAllStatsFileName());
			BufferedReader tempFileReader = new BufferedReader(reader);
			String data = tempFileReader.readLine();
			while (data != null) {
				String romClassSearchString = "ROMCLASS: ";
				int romclassStart = data.indexOf(romClassSearchString);
				if (romclassStart > 0) {
					int classNameStart = romclassStart + romClassSearchString.length();
					int classNameEnd = classNameStart + data.substring(classNameStart).indexOf(' ');
					currentClassName = data.substring(classNameStart, classNameEnd).replace('/','.');
					if (sharedCacheConfig().TRACE_DEBUG())
						System.out.println("Decoded class entry: " + currentClassName);

					if (classesInCache.get(currentClassName) == null)
						classesInCache.put(currentClassName, new LinkedList());
					data = tempFileReader.readLine();
					continue;
				}

				String classPathAddressSearchString = " in classpath ";
				int classPathAddressIndex = data.indexOf(classPathAddressSearchString);
				if (classPathAddressIndex > 0) {
					String classPathAddress = data.substring(classPathAddressIndex + classPathAddressSearchString.length());
					int classPathIndex = Integer.parseInt(data.substring(7,classPathAddressIndex));
					String entry = (String) classPathEntries.get(classPathAddress).get(classPathIndex);
					classPathEntryForClass.put(currentClassName, entry);
					if (populateJarFiles.get(entry) == null)
						populateJarFiles.put(entry, new LinkedList());
					populateJarFiles.get(entry).add(currentClassName);
					
					if (sharedCacheConfig().TRACE_DEBUG())
						System.out.println("Decoded classpath entry: " + entry + ", current className is " + currentClassName);
				}
				
				String romMethodSearchString = "ROMMETHOD: ";
				if (data.indexOf(romMethodSearchString) > 0) {
					String methodAddress = extractAddress(data);
					String methodName = extractMethodName(data, romMethodSearchString);
					String methodSig = extractMethodSig(data, romMethodSearchString);
					romMethodSigs.put(methodAddress, methodSig);
					romMethodNames.put(methodAddress, methodName);
					romClasses.put(methodAddress, currentClassName);

					if (sharedCacheConfig().TRACE_DEBUG())
						System.out.println("Decoded rommethod info: " + methodAddress + " is " + currentClassName + " " + methodName + " " + methodSig);
					
					data = tempFileReader.readLine();
					continue;
				}
				
				String aotSearchString = "AOT: ";
				if (data.indexOf(aotSearchString) > 0) {
					String methodAddress = extractAddress(data);
					String methodSig = romMethodSigs.get(methodAddress);
					String methodName = romMethodNames.get(methodAddress);
					String className = romClasses.get(methodAddress);
					if (methodName != null && className != null) {
						LinkedList<String> methodList = classesInCache.get(className);
						methodList.add(methodName);
						methodList.add(methodSig);
						if (sharedCacheConfig().TRACE_DEBUG())
							System.out.println("Decoded AOT method at address " + methodAddress + ": class " + className + " method " + methodName + " sig " + methodSig);
					}
					else {
						unknownAotMethods.add(methodAddress);
						if (sharedCacheConfig().TRACE_DEBUG())
							System.out.println("Current unknown AOT method at address " + methodAddress);
					}
				}
				
				data = tempFileReader.readLine();
			}
			
			Iterator<String> aotMethodIter = unknownAotMethods.iterator();
			while (aotMethodIter.hasNext()) {
				String methodAddress = aotMethodIter.next();
				String methodSig = romMethodSigs.get(methodAddress);
				String methodName = romMethodNames.get(methodAddress);
				String className = romClasses.get(methodAddress);
					
				if (methodName != null && className != null) {
					LinkedList<String> methodList = classesInCache.get(className);
					methodList.add(methodName);
					methodList.add(methodSig);
					if (sharedCacheConfig().TRACE_DEBUG())
						System.out.println("Decoded previously unknown AOT method at address " + methodAddress + ": class " + className + " method " + methodName + " sig " + methodSig);
				}
				else {
					System.err.println("Internal error: cannot find AOT method with address " + methodAddress);
					return Main.unableToGrowCache;
				}
			}
			
			if (sharedCacheConfig().TRACE_DEBUG())
				System.out.println("Re-creating existing cache:");
			loadClassesInOriginalCache(populateJarFiles, classesInCache);
			
		}
		catch (IOException ioe) {
			System.err.println("Internal error: unable to grow cache");
			return Main.unableToGrowCache;
		}
		
		return 0;
	}
	
	// This entry point should only be used by SharedCachePersister itself
	//	A new java command is invoked from performAction() with appropriate -Xshareclasses options to populate the cache
	public static void main(String[] args) {
		Main m = new Main();
		SharedCachePersister scp = new SharedCachePersister();
		m.setJarPersister(scp);
		int rc = m.processArgumentsAndCheckEnvironment(args);
		if (rc == 0) {
			if (scp.sharedCacheConfig().printAllStatsFileName() != null)
				rc = scp.processPrintAllStatsClasses();
			if (rc == 0)
				rc = scp.processJarFiles();
		}
		
		System.exit(rc);
	}
	
	   class sharedCacheShutDownHook extends Thread {
		      File  _printAllStatsFile;
		      public sharedCacheShutDownHook(File printAllStatsFile) {
		         _printAllStatsFile = printAllStatsFile;
		      }
		      public void run() {
				 _printAllStatsFile.delete(); /*delete the /tmp file */
		      }
		   }
};
