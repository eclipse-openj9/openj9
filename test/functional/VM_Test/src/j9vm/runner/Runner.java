/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
package j9vm.runner;
import java.io.*;
import java.util.Properties;
import java.util.Iterator;
import java.util.Map;
import java.lang.reflect.Method;

public class Runner {
	
	protected enum OSName {
		AIX,
		LINUX,
		WINDOWS,
		ZOS,
		UNKNOWN
	}
	
	protected enum OSArch {
		PPC,
		S390X,
		X86,
		UNKNOWN
	}
	
	protected enum AddrMode {
		BIT31,
		BIT32,
		BIT64,
		UNKNOWN
	}
	
	public static final String systemPropertyPrefix = "j9vm.";

	protected String className;
	protected String exeName;
	protected String bootClassPath;
	protected String userClassPath;
	protected String javaVersion;
	protected OutputCollector inCollector;
	protected OutputCollector errCollector;
	protected OSName osName = OSName.UNKNOWN;
	protected OSArch osArch = OSArch.UNKNOWN;
	protected AddrMode addrMode = AddrMode.UNKNOWN;

	private final String heapOptions = "-Xms64m -Xmx64m";

	private void setPlatform() {
		
		String OSSpec = System.getProperty("os.name").toLowerCase();
		if (OSSpec != null) {
			/* Get OS from the spec string */
			if (OSSpec.contains("aix")) {
				osName = OSName.AIX;
			} else if (OSSpec.contains("linux")) {
				osName = OSName.LINUX;
			} else if (OSSpec.contains("windows")) {
				osName = OSName.WINDOWS;
			} else if (OSSpec.contains("z/os")) {
				osName = OSName.ZOS;
			} else {
				System.out.println("Runner couldn't determine underlying OS. Got OS Name:" + OSSpec);
				osName = OSName.UNKNOWN;
			}
		}
		String archSpec = System.getProperty("os.arch").toLowerCase();
		if (archSpec != null) {
			/* Get arch from spec string */
			if (archSpec.contains("ppc")) {
				osArch = OSArch.PPC;
			} else if (archSpec.contains("s390")) {
				osArch = OSArch.S390X;
			} else if (archSpec.contains("amd64") || archSpec.contains("x86")) {
				osArch = OSArch.X86;
			} else {
				System.out.println("Runner couldn't determine underlying architecture. Got OS Arch:" + archSpec);
				osArch = OSArch.UNKNOWN;
			}
		}

		String addressingMode = System.getProperty("sun.arch.data.model");
		if (addressingMode != null) {
			/* Get address mode. S390 31-Bit addressing mode should return 32. */
			if ((osArch == OSArch.S390X) && (addressingMode.contains("32"))) {
				addrMode = AddrMode.BIT31;
			} else if (addressingMode.contains("32")) {
				addrMode = AddrMode.BIT32;
			} else if (addressingMode.contains("64")) {
				addrMode = AddrMode.BIT64;
			} else {
				System.out.println("Runner couldn't determine underlying addressing mode. Got addressingMode:" + addressingMode);
				addrMode = AddrMode.UNKNOWN;
			}
		}
	}
	
	public Runner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion)  {
		super();
		this.className = className;
		this.exeName = exeName;
		this.bootClassPath = bootClassPath;
		this.userClassPath = userClassPath;
		this.javaVersion = javaVersion;
		setPlatform();
	}

	public String getBootClassPathOption () {
		if (bootClassPath == null)  return "";
		return "-Xbootclasspath:" + bootClassPath;
	}

	public String getUserClassPathOption () {
		if (userClassPath == null)  return "";
		return "-classpath " + userClassPath;
	}

	public String getJ9VMSystemPropertiesString() {
		String result = "";
		Properties systemProperties = System.getProperties();
		Iterator it = systemProperties.entrySet().iterator();
		while(it.hasNext()) {
			Map.Entry entry = (Map.Entry) it.next();
			String key = (String) entry.getKey();
			if(key.startsWith(systemPropertyPrefix)) {
				String value = (String) entry.getValue();
				result += "-D" + key + "=" + value + " ";
			}

		}
		return result;
	}

	public String getCustomCommandLineOptions() {
		/* For sub-classes to override, if desired. */
		return "";
	}

	public String getCommandLine() {
		return exeName + " " + heapOptions + " " + getCustomCommandLineOptions() + " "
			+ getJ9VMSystemPropertiesString() + " " + getBootClassPathOption() + " "
			+ getUserClassPathOption() + " ";
	}
	
	public String getTestClassArguments() {
		/* For sub-classes to override, if desired. */
		return "";
	}
	
	public int runCommandLine(String commandLine)  {
		System.out.println("command: " + commandLine);
		System.out.println();
		Process process;
		try  {
			process = Runtime.getRuntime().exec(commandLine);
		} catch (Throwable e)  {
			System.out.println("Exception starting process!");
			System.out.println("(" + e.getMessage() + ")");
			e.printStackTrace();
			return 99999;
		}

		BufferedInputStream inStream = new BufferedInputStream(process.getInputStream());
		BufferedInputStream errStream = new BufferedInputStream(process.getErrorStream());
		inCollector = new OutputCollector(inStream);
		errCollector = new OutputCollector(errStream);
		inCollector.start();
		errCollector.start();
		try  {
			process.waitFor();
			inCollector.join();
			errCollector.join();
		} catch (InterruptedException e)  {
			/* Nothing. */
		}
		/* Must release process resources here, or wimpy platforms
		   like Neutrino will run out of handles! */
		int retval = process.exitValue();
		process.destroy(); process = null;
		System.gc();
		return retval;
	}

	public boolean run()  {
		int retval = runCommandLine(getCommandLine() + " " + className + " " + getTestClassArguments());
		if ( 0 != retval ) {
			System.out.println("no-zero exit value: " + retval);
			return false;
		}
		return true;
	}

}
