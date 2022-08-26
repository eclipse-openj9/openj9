/*******************************************************************************
 * Copyright (c) 1991, 2022 IBM Corp. and others
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
 */
public class J9DDRImageProcess implements ImageProcess {

	private static final String JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE = "OPENJ9_JAVA_COMMAND_LINE";
	private static final String LEGACY_JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE = "IBM_JAVA_COMMAND_LINE";

	private final IProcess process;
	private boolean processDataSet;
	private ProcessData j9rasProcessData;
	private String version;
	private WeakReference<Map<Long, Object>> cachedThreads;

	public J9DDRImageProcess(IProcess thisProcess) {
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
	 */
	public String getCommandLine() throws DataUnavailable, CorruptDataException {
		try {
			Properties environment = getEnvironment();
			String javaCommandLine = environment.getProperty(JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE);

			if (javaCommandLine != null) {
				return javaCommandLine;
			}

			javaCommandLine = environment.getProperty(LEGACY_JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE);

			if (javaCommandLine != null) {
				return javaCommandLine;
			}

			return process.getCommandLine();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process, e);
		} catch (DataUnavailableException e) {
			throw new DataUnavailable(e.getMessage());
		}
	}

	private void checkFailureInfo() {
		if (!processDataSet) {
			j9rasProcessData = J9RASImageDataFactory.getProcessData(process);
			processDataSet = true;
		}
	}

	/**
	 * This method returns the ImageThread that matches the TID stored in the J9RAS data structure,
	 * or
	 * case 1: if no J9RAS structure available return null
	 * case 2: if J9RAS structure available but no TID field (old JVMs, old jextract behaviour)
	 *         - return the first thread, or null if no threads
	 * case 3: if J9RAS structure available with TID field but TID is zero (eg dump triggered outside JVM)
	 *         - platform specific code if core readers have identified a current thread, else...
	 * case 4: if J9RAS structure available with TID field but no match (eg Linux), return a stub ImageThread
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
					// Java 5 SRs 9 and earlier do not contain tid in RAS structure.
					// JExtract currently just takes first thread as we do here.
					for (IOSThread thread : process.getThreads()) {
						return new J9DDRImageThread(process, thread);
					}
					return null;
				}

				for (IOSThread thread : process.getThreads()) {
					if (thread.getThreadId() == currentThreadId) {
						return new J9DDRImageThread(process, thread);
					}
				}

				if (currentThreadId == 0) {
					// For dumps triggered from outside the JVM, the TID in the J9RAS structure will be unset/zero. The core
					// readers may be able to identify the crashing/triggering thread in these cases. For zOS, we can look
					// for a thread with a non-zero Task Completion Code. There may be more we can do here for Win/Linux/AIX.
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
				// If we're on Linux, the TID of the failing thread will have been altered by the fork/abort method
				// of taking the dump. If there's only one thread - it's the failing thread with a different tid.
				// We cannot match the native thread at this point, so provide a stub based on the information in the RAS structure.
				return new J9DDRStubImageThread(process, currentThreadId);
			} else {
				return null;
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process, e);
		}
	}

	/**
	 * This method gets the environment variables.
	 * First it tries to extract it from RAS structure.
	 * If not, it tries to get it from loaded modules.
	 * @return Properties instance of environment variables.
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 */
	public Properties getEnvironment() throws DataUnavailable, CorruptDataException {
		try {
			checkFailureInfo();
			if (j9rasProcessData != null) {
				try {
					Properties properties;
					long environ = j9rasProcessData.getEnvironment();

					if (process.getPlatform() == Platform.WINDOWS && j9rasProcessData.version() > 4) {
						// Get the env vars from an environment strings block instead of the environ global.
						properties = EnvironmentUtils.readEnvironmentStrings(process, environ);
					} else {
						long stringPointer = process.getPointerAt(environ);
						properties = EnvironmentUtils.readEnvironment(process, stringPointer);
					}
					if ((null == properties) || (0 == properties.size())) {
						/* In the case of env vars is null or empty,
						 * throw exception so that it tries to get env vars from modules.
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
			throw new DTFJCorruptDataException(process, e);
		} catch (DataUnavailableException e) {
			throw new DataUnavailable(e.getMessage());
		}
	}

	public ImageModule getExecutable() throws DataUnavailable, CorruptDataException {
		IModule executable;
		try {
			executable = process.getExecutable();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(new J9DDRCorruptData(process, e));
		}
		if (executable == null) {
			// Executable not available (J9DDRImageModule constructor below would throw NPE).
			throw new DataUnavailable("executable not found");
		}

		if (process.getPlatform() == Platform.LINUX) {
			// On Linux, the core file reader can easily fail to get the correct command line and executable
			// (because of 80 char limit on command line in the ELF header).
			// We preempt this by looking for the command line in the environment.
			String executableName = getExecutablePath();

			if (null == executableName) {
				return new J9DDRImageModule(process, executable);
			}

			// Override the executableName.
			return new J9DDRImageModule(process, executable, executableName);
		} else {
			return new J9DDRImageModule(process, executable);
		}
	}

	String getExecutablePath() {
		String commandLine;
		try {
			commandLine = getCommandLine();
		} catch (Exception e) {
			// Can't get command line - try getting it from system properties if we have them.
			return getExecutablePathFromSystemProperties();
		}
		// TODO Think about executable paths with spaces in.
		int spaceIndex = commandLine.indexOf(" ");
		String executableName;
		if (spaceIndex != -1) {
			executableName = commandLine.substring(0, spaceIndex);
		} else {
			executableName = commandLine;
		}
		return executableName;
	}

	private String getExecutablePathFromSystemProperties() {
		Iterator<?> runtimeIt = getRuntimes();

		while (runtimeIt.hasNext()) {
			Object o = runtimeIt.next();

			if (o instanceof JavaRuntime) {
				JavaRuntime runtime = (JavaRuntime) o;

				try {
					JavaVMInitArgs args = runtime.getJavaVMInitArgs();
					Iterator<?> optionsIt = args.getOptions();
					Pattern javaHomePattern = Pattern.compile("-Djava.home=(.*)");

					while (optionsIt.hasNext()) {
						Object optionObj = optionsIt.next();

						if (optionObj instanceof JavaVMOption) {
							JavaVMOption option = (JavaVMOption) optionObj;
							Matcher m = javaHomePattern.matcher(option.getOptionString());

							if (m.find()) {
								String javaHome = m.group(1);

								// This method is not used on Windows, so we don't have to worry about .exe or backslashes.
								return javaHome + "/bin/java";
							}
						}
					}
				} catch (Exception e) {
					// ignore
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
					// Java5 SR9 and earlier don't contain pid in J9RAS structure.
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
	public String toString() {
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
			throw new DTFJCorruptDataException(new J9DDRCorruptData(process, e));
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
				// attempt to load a default bootstrap class which will allow different implementations to provide their own initializers
				data.bootstrapRelative("view.dtfj.DTFJBootstrapShim", (Object) passbackArray, this);
			} catch (ClassNotFoundException e) {
				// no specific class was found, so use a generic native one instead
				try {
					data.bootstrap("com.ibm.j9ddr.view.nativert.Bootstrap", (Object) passbackArray, this);

				} catch (ClassNotFoundException e1) {
					// this class should be packaged and without it we can't work, so abort
					throw new Error(e1);
				}
			}

			if (passbackArray[0] != null) {
				toIterate.add(passbackArray[0]);
			}
		} catch (IOException e) {
			// VMDataFactory may throw IOException for JVMs that this level of DDR does not support. Pass the
			// detail message in a CDE. The underlying exception will have been logged in VMDataFactory.
			toIterate.add(new J9DDRCorruptData(process, "Unsupported JVM level: " + e.getMessage()));
		} catch (UnsupportedOperationException e) {
			// VMDataFactory may throw UnsupportedOperationException
			// exceptions for JVMs DDR does not support.
			toIterate.add(new J9DDRCorruptData(process, "Unsupported JVM level"));
		} catch (Exception e) {
			toIterate.add(new J9DDRCorruptData(process, e.getMessage()));
		}

		return toIterate.iterator();
	}

	private static String aixSignalName(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGEMT       8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGURG      17) SIGSTOP     18) SIGTSTP     19) SIGCONT     20) SIGCHLD
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGIO       24) SIGXCPU     25) SIGXFSZ
		 * 27) SIGMSG      28) SIGWINCH    29) SIGPWR      30) SIGUSR1     31) SIGUSR2
		 * 32) SIGPROF     33) SIGDANGER   34) SIGVTALRM   35) SIGMIGRATE  36) SIGPRE
		 * 37) SIGVIRT     38) SIGALRM1    39) SIGWAITING  50) SIGRTMIN    51) SIGRTMIN+1
		 * 52) SIGRTMIN+2  53) SIGRTMIN+3  54) SIGRTMAX-3  55) SIGRTMAX-2  56) SIGRTMAX-1
		 * 57) SIGRTMAX    60) SIGKAP      61) SIGRETRACT  62) SIGSOUND    63) SIGSAK
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGEMT";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGURG";
		case 17:
			return "SIGSTOP";
		case 18:
			return "SIGTSTP";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 27:
			return "SIGMSG";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGPWR";
		case 30:
			return "SIGUSR1";
		case 31:
			return "SIGUSR2";
		case 32:
			return "SIGPROF";
		case 33:
			return "SIGDANGER";
		case 34:
			return "SIGVTALRM";
		case 35:
			return "SIGMIGRATE";
		case 36:
			return "SIGPRE";
		case 37:
			return "SIGVIRT";
		case 38:
			return "SIGTALRM1";
		case 39:
			return "WAITING";
		case 60:
			return "SIGJAP";
		case 61:
			return "SIGRETRACT";
		case 62:
			return "SIGSOUND";
		case 63:
			return "SIGSAK";
		default:
			return null;
		}
	}

	private static String linuxSignalName(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGBUS       8) SIGFPE       9) SIGKILL     10) SIGUSR1
		 * 11) SIGSEGV     12) SIGUSR2     13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGSTKFLT   17) SIGCHLD     18) SIGCONT     19) SIGSTOP     20) SIGTSTP
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ
		 * 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO       30) SIGPWR
		 * 31) SIGSYS      34) SIGRTMIN    35) SIGRTMIN+1  36) SIGRTMIN+2  37) SIGRTMIN+3
		 * 38) SIGRTMIN+4  39) SIGRTMIN+5  40) SIGRTMIN+6  41) SIGRTMIN+7  42) SIGRTMIN+8
		 * 43) SIGRTMIN+9  44) SIGRTMIN+10 45) SIGRTMIN+11 46) SIGRTMIN+12 47) SIGRTMIN+13
		 * 48) SIGRTMIN+14 49) SIGRTMIN+15 50) SIGRTMAX-14 51) SIGRTMAX-13 52) SIGRTMAX-12
		 * 53) SIGRTMAX-11 54) SIGRTMAX-10 55) SIGRTMAX-9  56) SIGRTMAX-8  57) SIGRTMAX-7
		 * 58) SIGRTMAX-6  59) SIGRTMAX-5  60) SIGRTMAX-4  61) SIGRTMAX-3  62) SIGRTMAX-2
		 * 63) SIGRTMAX-1  64) SIGRTMAX
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGBUS";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGUSR1";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGUSR2";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGSTKFLT";
		case 17:
			return "SIGCHLD";
		case 18:
			return "SIGCONT";
		case 19:
			return "SIGSTOP";
		case 20:
			return "SIGTSTP";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGURG";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 26:
			return "SIGVTALRM";
		case 27:
			return "SIGPROF";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGIO";
		case 30:
			return "SIGPWR";
		case 31:
			return "SIGSYS";
		default:
			return null;
		}
	}

	private static String osxSignalName(int signal) {
		/* Derived from the output of "kill -l".
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGEMT       8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGURG      17) SIGSTOP     18) SIGTSTP     19) SIGCONT     20) SIGCHLD
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGIO       24) SIGXCPU     25) SIGXFSZ
		 * 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGINFO     30) SIGUSR1
		 * 31) SIGUSR2
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGEMT";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGURG";
		case 17:
			return "SIGSTOP";
		case 18:
			return "SIGTSTP";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 26:
			return "SIGVTALRM";
		case 27:
			return "SIGPROF";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGINFO";
		case 30:
			return "SIGUSR1";
		case 31:
			return "SIGUSR2";
		default:
			return null;
		}
	}

	private static String windowsSignalName(int signal) {
		switch (signal) {
		case 0xc0000005:
			return "EXCEPTION_ACCESS_VIOLATION";
		case 0xc000001d:
			return "EXCEPTION_ILLEGAL_INSTRUCTION";
		case 0xc000008d:
			return "EXCEPTION_FLT_DENORMAL_OPERAND";
		case 0xc000008e:
			return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case 0xc000008f:
			return "EXCEPTION_FLT_INEXACT_RESULT";
		case 0xc0000090:
			return "EXCEPTION_FLT_INVALID_OPERATION";
		case 0xc0000091:
			return "EXCEPTION_FLT_OVERFLOW";
		case 0xc0000092:
			return "EXCEPTION_FLT_STACK_CHECK";
		case 0xc0000093:
			return "EXCEPTION_FLT_UNDERFLOW";
		case 0xc0000094:
			return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case 0xc0000095:
			return "EXCEPTION_INT_OVERFLOW";
		case 0xc0000096:
			return "EXCEPTION_PRIV_INSTRUCTION";
		case 0xc00000fd:
			return "EXCEPTION_STACK_OVERFLOW";
		default:
			// answer a more easily readable string for other codes
			return String.format("Unknown signal number 0x%08x", signal);
		}
	}

	private static String zosSignalName(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGABRT      4) SIGILL       5) SIGPOLL
		 *  6) SIGURG       7) SIGSTOP      8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGUSR1     17) SIGUSR2     19) SIGCONT     20) SIGCHLD     21) SIGTTIN
		 * 22) SIGTTOU     23) SIGIO       24) SIGQUIT     25) SIGTSTP     26) SIGTRAP
		 * 28) SIGWINCH    29) SIGXCPU     30) SIGXFSZ     31) SIGVTALRM   32) SIGPROF
		 * 33) SIGDANGER
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGABRT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGPOLL";
		case 6:
			return "SIGURG";
		case 7:
			return "SIGSTOP";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGUSR1";
		case 17:
			return "SIGUSR2";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGQUIT";
		case 25:
			return "SIGTSTP";
		case 26:
			return "SIGTRAP";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGXCPU";
		case 30:
			return "SIGXFSZ";
		case 31:
			return "SIGVTALRM";
		case 32:
			return "SIGPROF";
		case 33:
			return "SIGDANGER";
		default:
			return null;
		}
	}

	public String getSignalName() throws DataUnavailable, CorruptDataException {
		String name = null;
		int signal = getSignalNumber();

		if (signal != 0) {
			switch (process.getPlatform()) {
			case AIX:
				name = aixSignalName(signal);
				break;
			case LINUX:
				name = linuxSignalName(signal);
				break;
			case OSX:
				name = osxSignalName(signal);
				break;
			case WINDOWS:
				name = windowsSignalName(signal);
				break;
			case ZOS:
				name = zosSignalName(signal);
				break;
			default:
				break;
			}

			if (name == null) {
				name = "Unknown signal number " + signal;
			}
		}

		return name;
	}

	public int getSignalNumber() throws DataUnavailable, CorruptDataException {
		checkFailureInfo();

		try {
			String gpInfo = j9rasProcessData.gpInfo();

			if (gpInfo != null) {
				// Names returned for OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE.
				String[] keys;

				switch (process.getPlatform()) {
				case AIX:
				case LINUX:
				case OSX:
					keys = new String[] { "Signal_Number" };
					break;
				case WINDOWS:
					keys = new String[] { "Windows_ExceptionCode", "ExceptionCode" };
					break;
				case ZOS:
					keys = new String[] { "Signal_Number", "Condition_Message_Number" };
					break;
				default:
					keys = new String[0];
					break;
				}

				for (String key : keys) {
					int signal = (int) longByResolvingRawKey(gpInfo, key);

					if (signal != 0) {
						return signal;
					}
				}
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			// Fall back to core file readers.
		}

		try {
			return this.process.getSignalNumber();
		} catch (DataUnavailableException e) {
			return 0;
		}
	}

	private Map<Long, Object> getThreadMap() {
		Map<Long, Object> dtfjThreadMap = null;
		if (cachedThreads != null) {
			dtfjThreadMap = cachedThreads.get();
		}

		/* We did not find a cache of threads, build one. */
		if (dtfjThreadMap == null) {
			Collection<? extends IOSThread> threads;
			long corruptId = -1;

			try {
				threads = process.getThreads();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				return Collections.singletonMap(corruptId, (Object) new J9DDRCorruptData(process, e));
			}

			// On Linux, fork/abort system dumping means we only get one thread, with an altered TID. In this case we have logic in
			// getCurrentThread that renames the dump thread. Add this thread in addition to the native thread.

			boolean forkandabort = (process.getPlatform() == Platform.LINUX) && (threads.size() == 1);

			dtfjThreadMap = new HashMap<>();

			if (forkandabort) {
				try {
					J9DDRBaseImageThread currentThread = (J9DDRBaseImageThread) getCurrentThread();
					if (currentThread != null) {
						dtfjThreadMap.put(currentThread.getThreadId(), currentThread);
					}
				} catch (CorruptDataException e) {
					// A corrupt thread won't have an id but we will still return the corrupt
					// data in the list of all threads.
					dtfjThreadMap.put(corruptId--, new J9DDRCorruptData(process, e.getMessage()));
				} catch (com.ibm.j9ddr.CorruptDataException e) {
					// A corrupt thread won't have an id but we will still return the corrupt
					// data in the list of all threads.
					dtfjThreadMap.put(corruptId--, new J9DDRCorruptData(process, e.getMessage()));
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
			cachedThreads = new WeakReference<>(dtfjThreadMap);
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
		return (J9DDRBaseImageThread) dtfjThreadMap.get(id);
	}

	/**
	 * Looks up the given key in rawText and finds the long that corresponds to it,
	 * i.e.: "... key=number...".
	 * Returns 0 if the key is not found.
	 *
	 * @param rawText
	 * @param key
	 * @return
	 */
	private static long longByResolvingRawKey(String rawText, String key) {
		// the value is assumed to be hexadecimal (one or more 'XDigit')
		Pattern pattern = Pattern.compile("\\b" + Pattern.quote(key) + "=(\\p{XDigit}+)");
		Matcher matcher = pattern.matcher(rawText);

		if (matcher.find()) {
			return Long.parseUnsignedLong(matcher.group(1), 16);
		}

		return 0;
	}

	/**
	 * Return the build version (e.g. 29) of the first VM in the process (useful for testing).
	 *
	 * Would be nice if DTFJJavaRuntime could implement this method, but that would be complicated
	 * as there are multiple version-packaged DTFJJavaRuntimes (e.g. j9ddr.vmxx.view.dtfj.java.DTFJJavaRunTime)
	 * and tests are not version-packaged.
	 */
	public String getVersion() {
		return version;
	}

	boolean isFailingProcess() throws DataUnavailableException {
		return process.isFailingProcess();
	}

	// currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}

}
