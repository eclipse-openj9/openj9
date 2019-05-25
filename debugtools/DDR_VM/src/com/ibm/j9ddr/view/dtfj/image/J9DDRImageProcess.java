/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.image;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.VMDataFactory;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.EnvironmentUtils;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.view.dtfj.DTFJCorruptDataException;
import com.ibm.j9ddr.view.dtfj.image.J9RASImageDataFactory.ProcessData;

/**
 * Adapter for DDR IProcesses to make them implement the ImageProcess
 * API
 * 
 * @author andhall
 *
 */
public class J9DDRImageProcess implements ImageProcess {

	private static final String JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE = "IBM_JAVA_COMMAND_LINE";
	private final IProcess process;
	private boolean processDataSet = false;
	private ProcessData j9rasProcessData;
	private String version;
	private WeakReference<Map<Long,Object>> cachedThreads = null;
	
	// These flag definitions came from j9port.h
	private final static int J9PORT_SIG_FLAG_SIGSEGV 	= 4;
	private final static int J9PORT_SIG_FLAG_SIGBUS 	= 8;
	private final static int J9PORT_SIG_FLAG_SIGILL 	= 16;
	private final static int J9PORT_SIG_FLAG_SIGFPE 	= 32;
	private final static int J9PORT_SIG_FLAG_SIGTRAP 	= 64;
	private final static int J9PORT_SIG_FLAG_SIGQUIT 	= 0x400;
	private final static int J9PORT_SIG_FLAG_SIGABRT 	= 0x800;
	private final static int J9PORT_SIG_FLAG_SIGTERM 	= 0x1000;
	private final static int J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO 	= 0x40020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO = 0x80020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW 	= 0x100020;
	
	private final static String[] SIGNAL_NAMES = {
		"ZERO",
		"SIGHUP",		// 1
		"SIGINT",		// 2 + win
		"SIGQUIT",
		"SIGILL",		// 4 + win
		"SIGTRAP",		// 5
		"SIGABRT",
		"SIGEMT",
		"SIGFPE",		// 8 + win
		"SIGKILL",
		"SIGBUS",		// 10
		"SIGSEGV",		// 11 + win
		"SIGSYS",
		"SIGPIPE",
		"SIGALRM",
		"SIGTERM",		// 15 + win
		"SIGUSR1",
		"SIGUSR2",
		"SIGCHLD",
		"SIGPWR",
		"SIGWINCH",		// 20
		"SIGURG/BREAK",	// 21 / win
		"SIGPOLL/ABRT",	// 22 / win
		"SIGSTOP",
		"SIGTSTP",
		"SIGCONT",		// 25
		"SIGTTIN",
		"SIGTTOU",
		"SIGVTALRM",
		"SIGPROF",
		"SIGXCPU",		// 30
		"SIGXFSZ",
		"SIGWAITING",
		"SIGLWP",
		"SIGAIO",		// 34
		"SIGFPE_DIV_BY_ZERO",		// 35 - synthetic from generic signal
		"SIGFPE_INT_DIV_BY_ZERO",	// 36 - synthetic from generic signal
		"SIGFPE_INT_OVERFLOW"		// 37 - synthetic from generic signal
	};
	
	public J9DDRImageProcess(IProcess thisProcess)
	{
		this.process = thisProcess;
	}

	public IProcess getIProcess() {
		return process;
	}
	
	/**
	 * This method tries to get command line of the program that generated core file.
	 * 
	 * We can't get the command line from the core dump on zOS, or on recent Windows versions. On Linux
	 * it may be truncated. The java launcher stores the command line in an environment variable, so for
	 * all platforms we now try that first, with the core reader as a fallback.
	 * 
	 * @return String instance of the commandline
	 * @throws DataUnavailable
	 * @throws {@link CorruptDataException}
	 *  
	 */
	public String getCommandLine() throws DataUnavailable, CorruptDataException {
		try {
			Properties environment = getEnvironment();
			String javaCommandLine = environment.getProperty(JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE);
			
			if (javaCommandLine != null) {
				return javaCommandLine;
			}
			return process.getCommandLine();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process,e);
		} catch (DataUnavailableException e) {
			throw new DataUnavailable(e.getMessage());
		}
	}

	private void checkFailureInfo()
	{
		if (!processDataSet) {
			j9rasProcessData = J9RASImageDataFactory.getProcessData(process);
			
			processDataSet = true;
		}
	}
	
	/**
	 * This method returns the ImageThread that matches the TID stored in the J9RAS data structure,
	 *  or
	 *  case 1: if no J9RAS structure available return null
	 *  case 2: if J9RAS structure available but no TID field (old JVMs, old jextract behaviour) 
	 *  		- return the first thread, or null if no threads
	 *  case 3: if J9RAS structure available with TID field but TID is zero (eg dump triggered outside JVM)
	 *  		- platform specific code if core readers have identified a current thread, else...
	 *  case 4: if J9RAS structure available with TID field but no match (eg Linux), return a stub ImageThread
	 * 
	 * @return ImageThread
	 * @throws {@link CorruptDataException}
	 */
	public ImageThread getCurrentThread() throws CorruptDataException {
		try {
			checkFailureInfo();
			if (j9rasProcessData != null) {
				long currentThreadId;
				try {
					currentThreadId = j9rasProcessData.tid();
				} catch (DataUnavailable e) {
					// Java 5 SRs 9 and earlier do not contain tid in RAS structure
					// JExtract currently just takes first thread as we do here
					for (IOSThread thread : process.getThreads()) {
						return new J9DDRImageThread(process, thread);
					}
					return null;
				}

				for (IOSThread thread : process.getThreads()) {
					if (thread.getThreadId() == currentThreadId) {
						return new J9DDRImageThread(process,thread); 
					}
				}
				
				if (currentThreadId == 0) {
					// For dumps triggered from outside the JVM, the TID in the J9RAS structure will be unset/zero. The core
					// readers may be able to identify the crashing/triggering thread in these cases. For zOS, we can look
					// for a thread with a non-zero Task Completion Code. There may be more we can do here for Win/Linux/AIX
					if (process.getPlatform() == Platform.ZOS) {
						for (IOSThread thread : process.getThreads()) {
							Properties threadProps = thread.getProperties();
							String tcc = threadProps.getProperty("Task Completion Code");
							if (tcc != null && !tcc.equals("0x0")) { 
								return new J9DDRImageThread(process, thread);
							}
						}
					}
				}
				//If we're on Linux, the TID of the failing thread will have been altered by the fork/abort method
				//of taking the dump. If there's only one thread - it's the failing thread with a different tid.
				//We cannot match the native thread at this point, so provide a stub based on the information in the RAS structure.
				return new J9DDRStubImageThread(process, currentThreadId);

			} else {
				return null;
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process,e);
		}
	}
	
	/**
	 * This method gets the environment variables.
	 * First it tries to extract it from RAS structure. 
	 * If not, it tries to get it from loaded modules. 
	 * @return Properties instance of environment variables. 
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 * 
	 */
	public Properties getEnvironment() throws DataUnavailable, CorruptDataException {
		try {
			checkFailureInfo();
			if (j9rasProcessData != null) {
				try {
					Properties properties;
					long environ = j9rasProcessData.getEnvironment();
					
					if (process.getPlatform() == Platform.WINDOWS && j9rasProcessData.version() > 4) {
						// Get the env vars from an environment strings block instead of the environ global
						properties =  EnvironmentUtils.readEnvironmentStrings(process, environ);
					} else {
						long stringPointer = process.getPointerAt(environ);
						properties =  EnvironmentUtils.readEnvironment(process, stringPointer);
					}
					if ((null == properties) || (0 == properties.size())) {
						/* In the case of env vars is null or empty, 
						 * throw exception so that it tries to get env vars from modules 
						 */
						 throw new com.ibm.j9ddr.CorruptDataException("");
					}
					return properties;
				} catch (com.ibm.j9ddr.CorruptDataException e1) {
					/* Seems like RAS structure is corrupted. Try to get it from modules. */
					return process.getEnvironmentVariables();
				}
			} else {
				/* We don't have a J9RAS structure to work with. Try to get it from modules. */
				return process.getEnvironmentVariables();
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process,e);
		} catch (DataUnavailableException e) {
			throw new DataUnavailable(e.getMessage());
		}
	}

	public ImageModule getExecutable() throws DataUnavailable, CorruptDataException {
		
		IModule executable;
		try {
			executable = process.getExecutable();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(new J9DDRCorruptData(process,e));
		}
		if (executable == null) {
			// Executable not available (J9DDRImageModule constructor below would throw NPE)
			throw new DataUnavailable("executable not found");
		}
		
		if (process.getPlatform() == Platform.LINUX) {
			//On Linux, the core file reader can easily fail to get the correct command line and executable
			//(because of 80 char limit on command line in the ELF header).
			//We preempt this by looking for the command line in the environment
			String executableName = getExecutablePath();
			
			if (null == executableName) {
				return new J9DDRImageModule(process, executable);
			}
			
			//Override the executableName
			return new J9DDRImageModule(process, executable, executableName);
		} else {
			return new J9DDRImageModule(process, executable);
		}
	}

	String getExecutablePath()
	{
		String commandLine;
		try {
			commandLine = getCommandLine();
		} catch (Exception e) {
			//Can't get command line - try getting it from system properties if we have them
			return getExecutablePathFromSystemProperties();
		}
		//TODO think about executable paths with spaces in
		int spaceIndex = commandLine.indexOf(" ");
		
		String executableName = null;
		if (spaceIndex != -1) {
			executableName = commandLine.substring(0, spaceIndex);
		} else {
			executableName = commandLine;
		}
		return executableName;
	}

	private String getExecutablePathFromSystemProperties()
	{
		Iterator<?> runtimeIt = getRuntimes();
		
		while (runtimeIt.hasNext()) {
			Object o = runtimeIt.next();
			
			if (o instanceof JavaRuntime) {
				JavaRuntime runtime = (JavaRuntime)o;
				
				try {
					JavaVMInitArgs args = runtime.getJavaVMInitArgs();
					
					Iterator<?> optionsIt = args.getOptions();
					
					Pattern javaHomePattern = Pattern.compile("-Djava.home=(.*)");
					
					while (optionsIt.hasNext()) {
						Object optionObj = optionsIt.next();
						
						if (optionObj instanceof JavaVMOption) {
							JavaVMOption option = (JavaVMOption)optionObj;
							
							Matcher m = javaHomePattern.matcher(option.getOptionString());
							
							if (m.find()) {
								String javaHome = m.group(1);
								
								//Note this method not used on Windows - don't have to worry about .exe or \
								return javaHome + "/bin/java";
							}
						}
					}
					
				} catch (Exception e) {
					//Ignore
				}
			}
		}
		
		return null;
	}

	public String getID() throws DataUnavailable, CorruptDataException {
		try {
			checkFailureInfo();
			if (j9rasProcessData != null) {
				try {
					return Long.toString(j9rasProcessData.pid());
				} catch (DataUnavailable e) {
					// Java5 SR9 and earlier don't contain pid in J9RAS structure
					return Long.toString(process.getProcessId());
				}				
			} else {
				return Long.toString(process.getProcessId());
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			return "<Unknown>";
		}
	}
	
	@Override
	public String toString()
	{
		try {
			return "ImageProcess: " + getID();
		} catch (Exception ex) {
			return "ImageProcess";
		}
	}

	public Iterator<?> getLibraries() throws DataUnavailable, CorruptDataException {
		try {
			Collection<? extends IModule> modules = process.getModules();
			
			List<ImageModule> libraries = new ArrayList<ImageModule>();
			
			for (IModule module : modules) {
				libraries.add(new J9DDRImageModule(process, module));
			}
			
			return libraries.iterator();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(new J9DDRCorruptData(process,e));
		}
	}

	public int getPointerSize() {
		return process.bytesPerPointer() * 8;
	}

	public Iterator<?> getRuntimes() {
		List<Object> toIterate = new LinkedList<Object>();
		
		try {
			IVMData data = VMDataFactory.getVMData(process);
			version = data.getVersion();
			
			Object[] passbackArray = new Object[1];
			
			try {
				//attempt to load a default bootstrap class which will allow different implementations to provide their own initializers
				data.bootstrapRelative("view.dtfj.DTFJBootstrapShim", (Object)passbackArray, this);
			} catch (ClassNotFoundException e) {
				//no specific class was found, so use a generic native one instead
				try {
					data.bootstrap("com.ibm.j9ddr.view.nativert.Bootstrap", (Object)passbackArray, this);
					
				} catch (ClassNotFoundException e1) {
					//this class should be packaged and without it we can't work, so abort
					throw new Error(e1);
				}
			}
			
			if (passbackArray[0] != null) {
				toIterate.add(passbackArray[0]);
			}
		} catch (IOException e) {
			// VMDataFactory may throw IOException for JVMs that this level of DDR does not support. Pass the
			// detail message in a CDE. The underlying exception will have been logged in VMDataFactory.
			toIterate.add(new J9DDRCorruptData(process,"Unsupported JVM level: " + e.getMessage()));
		} catch (UnsupportedOperationException e) {
			// VMDataFactory may throw unsupported UnsupportedOperationException
			// exceptions for JVM's DDR does not support.
			toIterate.add(new J9DDRCorruptData(process,"Unsupported JVM level"));
		} catch (Exception e) {
			toIterate.add(new J9DDRCorruptData(process,e.getMessage()));
		}
		
		return toIterate.iterator();
	}
	
	public String getSignalName() throws DataUnavailable, CorruptDataException {
		int number = getSignalNumber();
		
		if (number == 0) {
			return null;
		} else if (number > 0 && number < SIGNAL_NAMES.length) {
			return SIGNAL_NAMES[number];
		} else {
			return "Unknown signal number " + number;
		}
	}

	public int getSignalNumber() throws DataUnavailable, CorruptDataException {
		checkFailureInfo();
		
		try {
			if (j9rasProcessData.gpInfo() != null) {
				int genericSignalNumber = (int)longByResolvingRawKey(j9rasProcessData.gpInfo(), "J9Generic_Signal_Number");
				if(genericSignalNumber == 0) {
					//on Windows AMD64 the key name is J9Generic_Signal, so try that if we haven't got a signal number
					genericSignalNumber = (int)longByResolvingRawKey(j9rasProcessData.gpInfo(), "J9Generic_Signal");
				}
				
				if (genericSignalNumber != 0) {
					return resolveGenericSignal(genericSignalNumber);
				} else {
					return 0;
				}
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			//Do nothing - fall back to core file readers
		}
		
		try {
			return this.process.getSignalNumber();
		} catch (DataUnavailableException e) {
			return 0;
		}
	}

	private Map<Long, Object> getThreadMap() {
		Map<Long, Object> dtfjThreadMap = null;
		if( cachedThreads != null ) {
			dtfjThreadMap = (Map<Long, Object>)cachedThreads.get();
		}
		
		/* We did not find a cache of threads, build one. */
		if( dtfjThreadMap == null ) {
			Collection<? extends IOSThread> threads = null;
			long corruptId = -1;
			
			try {
				threads = process.getThreads();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				return Collections.singletonMap(corruptId, (Object)new J9DDRCorruptData(process,e));
			}

			//On Linux, fork/abort system dumping means we only get one thread, with an altered TID. In this case we have logic in
			//getCurrentThread that renames the dump thread. Add this thread in addition to the native thread.

			boolean forkandabort = ((process.getPlatform() == Platform.LINUX) && (threads.size() == 1));

			dtfjThreadMap = new HashMap<Long, Object>();
			
			if (forkandabort) {
				try {
					J9DDRBaseImageThread currentThread = (J9DDRBaseImageThread)getCurrentThread();
					if( currentThread != null ) {
						dtfjThreadMap.put(currentThread.getThreadId(), currentThread);
					}
				} catch (CorruptDataException e) {
					// A corrupt thread won't have an id but we will still return the corrupt
					// data in the list of all threads.
					dtfjThreadMap.put(corruptId--, new J9DDRCorruptData(process,e.getMessage()));
				} catch (com.ibm.j9ddr.CorruptDataException e) {
					// A corrupt thread won't have an id but we will still return the corrupt
					// data in the list of all threads.
					dtfjThreadMap.put(corruptId--, new J9DDRCorruptData(process,e.getMessage()));
				}
			} 
			
			for (IOSThread thread : threads) {
				try {
					dtfjThreadMap.put(thread.getThreadId(), new J9DDRImageThread(process, thread));
				} catch (com.ibm.j9ddr.CorruptDataException e) {
					// In the unlikely event that we can't get the thread id, store the thread
					// without the id.
					dtfjThreadMap.put(corruptId--, new J9DDRImageThread(process, thread));
				}
			}
			cachedThreads = new WeakReference<Map<Long, Object>>(dtfjThreadMap);
		}
		
		return dtfjThreadMap;
	}
	
	@SuppressWarnings("unchecked")
	public Iterator<?> getThreads() {
		Map<Long, Object> dtfjThreadMap = getThreadMap();
		return dtfjThreadMap.values().iterator();
	}
	
	/*
	 * Return the thread with the given id or null.
	 */
	public J9DDRBaseImageThread getThread(long id) {
		Map<Long, Object> dtfjThreadMap = getThreadMap();
		return (J9DDRBaseImageThread)dtfjThreadMap.get(id);
	}
	
	private int resolveGenericSignal(int num) {
		if ((num & J9PORT_SIG_FLAG_SIGQUIT) != 0) 	return 3;
		if ((num & J9PORT_SIG_FLAG_SIGILL) != 0) 	return 4;
		if ((num & J9PORT_SIG_FLAG_SIGTRAP) != 0) 	return 5;
		if ((num & J9PORT_SIG_FLAG_SIGABRT) != 0) 	return 6;
		if ((num & J9PORT_SIG_FLAG_SIGFPE) != 0) {
			if (num == J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO) 		return 35;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO) 	return 36;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW) 	return 37;
			return 8;
		}
		if ((num & J9PORT_SIG_FLAG_SIGBUS) != 0) 	return 10;
		if ((num & J9PORT_SIG_FLAG_SIGSEGV) != 0) 	return 11;
		if ((num & J9PORT_SIG_FLAG_SIGTERM) != 0) 	return 15;
		return num;
	}
	
	/**
	 * Looks up the given key in rawText and finds the long that corresponds to it.  ie: "... key=number..."
	 * Returns 0 if the key is not found.
	 * 
	 * @param rawText
	 * @param key
	 * @return
	 */
	private static long longByResolvingRawKey(String rawText, String key)
	{
		long value = 0;
		int index = rawText.indexOf(key);
		
		while ((index > 0) && (!Character.isWhitespace(rawText.charAt(index-1)))) {
			index = rawText.indexOf(key, index+1);
		}
		
		if (index >= 0) {
			int equalSign = rawText.indexOf("=", index);
			
			if (equalSign > index) {
				int exclusiveEnd = equalSign +1;
				
				while ((exclusiveEnd < rawText.length()) && (!Character.isWhitespace(rawText.charAt(exclusiveEnd)))) {
					exclusiveEnd++;
				}
				//the value on the right of the equal sign is always supposed to be hexadecimal
				String number = "0x" + rawText.substring(equalSign+1, exclusiveEnd);
				value = longFromString(number, 0);
			}
		}
		return value;
	}
	
	/**
	 * Helper method for parsing strings into numbers.
	 * Strips the 0x from strings, if they exist.  Also handles the case of 16 character numbers which would be greater than MAX_LONG
	 * @param value The string representing the number
	 * @param defaultValue The value which will be returned if value is null
	 * @return defaultValue if the value is null or the long value of the given string
	 */
	private static long longFromString(String value, long defaultValue)
	{
		long translated = defaultValue;
		int radix = 10;
		
		if (null != value) {
			if (value.startsWith("0x")) {
				value = value.substring(2);
				radix = 16;
				if (16 == value.length()) {
					//split and shift since this would overflow
					String highS = value.substring(0, 8);
					String lowS = value.substring(8, 16);
					long high = Long.parseLong(highS, radix);
					long low = Long.parseLong(lowS, radix);
					translated = (high << 32) | low;
				} else {
					translated = Long.parseLong(value, radix);
				}
			} else {
				translated = Long.parseLong(value, radix);
			}
		}
		return translated;
	}
	
	/** 
	 *  Returns the build version(e.g. 23-26) of the first VM in the process (useful for testing).
	 *  
	 *  Would be nice if DTFJJavaRuntime could implement this method, but that would be complicated
	 *  as there are multiple version-packaged DTFJJavaRuntimes (e.g. j9ddr.vmxx.view.dtfj.java.DTFJJavaRunTime)
	 *  and tests are not version-packaged.
	 */
	public String getVersion()
	{
		return version;
	}
	
	boolean isFailingProcess() throws DataUnavailableException {
		return process.isFailingProcess();
	}
	
	//currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}
}

