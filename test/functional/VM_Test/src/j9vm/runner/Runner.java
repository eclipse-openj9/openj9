/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package j9vm.runner;

import java.io.BufferedInputStream;
import java.util.Map;
import java.util.Properties;

public class Runner {

	protected static enum OSName {
		AIX,
		LINUX,
		MAC,
		WINDOWS,
		ZOS,
		UNKNOWN;

		public static OSName current() {
			String osSpec = System.getProperty("os.name", "?").toLowerCase();

			/* Get OS from the spec string. */
			if (osSpec.contains("aix")) {
				return AIX;
			} else if (osSpec.contains("linux")) {
				return LINUX;
			} else if (osSpec.contains("mac")) {
				return MAC;
			} else if (osSpec.contains("windows")) {
				return WINDOWS;
			} else if (osSpec.contains("z/os")) {
				return ZOS;
			} else {
				System.out.println("Runner couldn't determine underlying OS. Got OS Name:" + osSpec);
				return UNKNOWN;
			}
		}
	}

	protected static enum OSArch {
		AARCH64,
		PPC,
		S390X,
		X86,
		UNKNOWN;

		public static OSArch current() {
			String archSpec = System.getProperty("os.arch", "?").toLowerCase();

			/* Get arch from spec string. */
			if (archSpec.contains("aarch64")) {
				return AARCH64;
			} else if (archSpec.contains("ppc")) {
				return PPC;
			} else if (archSpec.contains("s390")) {
				return S390X;
			} else if (archSpec.contains("amd64") || archSpec.contains("x86")) {
				return X86;
			} else {
				System.out.println("Runner couldn't determine underlying architecture. Got OS Arch:" + archSpec);
				return UNKNOWN;
			}
		}
	}

	protected static enum AddrMode {
		BIT31,
		BIT32,
		BIT64,
		UNKNOWN;

		public static AddrMode current(OSArch osArch) {
			String dataModel = System.getProperty("sun.arch.data.model", "?");

			/* Get address mode. S390 31-Bit addressing mode should return 32. */
			if (dataModel.contains("32")) {
				return (osArch == OSArch.S390X) ? BIT31 : BIT32;
			} else if (dataModel.contains("64")) {
				return BIT64;
			} else {
				System.out.println("Runner couldn't determine underlying addressing mode. Got addressing mode:" + dataModel);
				return UNKNOWN;
			}
		}
	}

	public static final String systemPropertyPrefix = "j9vm.";

	private static final String heapOptions = "-Xms64m -Xmx64m";

	protected final String className;
	protected final String exeName;
	protected final String bootClassPath;
	protected final String userClassPath;
	protected final String javaVersion;
	protected final OSName osName;
	protected final OSArch osArch;
	protected final AddrMode addrMode;
	protected OutputCollector inCollector;
	protected OutputCollector errCollector;

	public Runner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion) {
		super();
		this.className = className;
		this.exeName = exeName;
		this.bootClassPath = bootClassPath;
		this.userClassPath = userClassPath;
		this.javaVersion = javaVersion;
		this.osName = OSName.current();
		this.osArch = OSArch.current();
		this.addrMode = AddrMode.current(osArch);
	}

	public String getBootClassPathOption() {
		if (bootClassPath == null) {
			return "";
		}
		return "-Xbootclasspath:" + bootClassPath;
	}

	public String getUserClassPathOption() {
		if (userClassPath == null) {
			return "";
		}
		return "-classpath " + userClassPath;
	}

	public String getJ9VMSystemPropertiesString() {
		StringBuilder result = new StringBuilder();
		Properties systemProperties = System.getProperties();
		for (Map.Entry<?, ?> entry : systemProperties.entrySet()) {
			String key = (String) entry.getKey();
			if (key.startsWith(systemPropertyPrefix)) {
				if (result.length() > 0) {
					result.append(" ");
				}
				Object value = entry.getValue();
				result.append("-D").append(key).append("=").append(value);
			}
		}
		return result.toString();
	}

	public String getCustomCommandLineOptions() {
		/* For sub-classes to override, if desired. */
		return "";
	}

	public String getCommandLine() {
		StringBuilder command = new StringBuilder(exeName);

		appendIfNonTrivial(command, heapOptions);
		appendIfNonTrivial(command, getCustomCommandLineOptions());
		appendIfNonTrivial(command, getJ9VMSystemPropertiesString());
		appendIfNonTrivial(command, getBootClassPathOption());
		appendIfNonTrivial(command, getUserClassPathOption());

		return command.toString();
	}

	private static void appendIfNonTrivial(StringBuilder buffer, String options) {
		if (options != null) {
			options = options.trim();
			if (!options.isEmpty()) {
				buffer.append(" ").append(options);
			}
		}
	}

	public String getTestClassArguments() {
		/* For sub-classes to override, if desired. */
		return "";
	}

	public int runCommandLine(String commandLine) {
		System.out.println("command: " + commandLine);
		System.out.println();
		Process process;
		try {
			process = Runtime.getRuntime().exec(commandLine);
		} catch (Throwable e) {
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
		try {
			process.waitFor();
			inCollector.join();
			errCollector.join();
		} catch (InterruptedException e) {
			/* Nothing. */
		}
		/* Release process resources here, to avoid running out of handles. */
		int retval = process.exitValue();
		process.destroy();
		process = null;
		System.gc();
		return retval;
	}

	public boolean run() {
		int retval = runCommandLine(getCommandLine() + " " + className + " " + getTestClassArguments());
		if (0 != retval) {
			System.out.println("non-zero exit value: " + retval);
			return false;
		}
		return true;
	}

}
