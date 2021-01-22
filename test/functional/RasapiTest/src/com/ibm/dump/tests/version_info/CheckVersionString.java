/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.version_info;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel.MapMode;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;
import java.util.Set;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.jvm.trace.format.api.TraceContext;

/**
 * This class checks the various RAS artifacts contain the correct version
 * string. It is in java rather than perl so we can use DTFJ to read system and
 * heap dumps and the trace formatter to read the binary trace files directly.
 * 
 * It expects to be run by the JVM that generated the artifacts in the first
 * place.
 * 
 * @author hhellyer
 * 
 */
public class CheckVersionString {

	/**
	 * Arguments passed via system properties rather than command line arguments
	 * so we don't have to write parsing code.
	 */
	private static final String JAVACORE_FILE_PROPERTY = "java.file";
	private static final String SYSTEMDUMP_FILE_PROPERTY = "system.file";
	private static final String HEAPDUMP_FILE_PROPERTY = "heap.file";
	private static final String HEAPDUMP_CLASSIC_FILE_PROPERTY = "classic.heap.file";
	private static final String SNAPDUMP_FILE_PROPERTY = "snap.file";

	private static final String SDK_VERSION = "sdk.version";

	private static final DumpType[] allDumps = { new JavaDumpType(),
			/*new SystemDumpType(), - Disabled until DDR is stable. */
			 new HeapDumpType(),
			 new ClassicHeapDumpType(), new SnapDumpType() };

	private static Properties sysProps = System.getProperties();

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		// Check the version.properties file exists. If it
		// doesn't the dumps cannot get the right version
		// String anyway.
		String versionFilePath = sysProps.getProperty("java.home")
				+ File.separator + "lib" + File.separator
				+ "version.properties";
		File versionFile = new File(versionFilePath);
		if (!versionFile.exists()) {
			System.err
					.println("Package build does not contain " + versionFilePath);
			System.err
					.println("This should be present to indicate the build level of the fully packaged build of JVM and class libraries.");
			System.exit(1);
		}
		Properties versionProperties = new Properties();
		try {
			versionProperties.load(new FileInputStream(versionFile));
		} catch (FileNotFoundException e) {
			// We don't expect this to happen as we've already checked the file
			// exists.
			System.err
					.println("Package build does not contain " + versionFilePath);
			System.err
					.println("This should be present to indicate the build level of the fully packaged build of JVM and class libraries.");
			e.printStackTrace();
			System.exit(1);
		} catch (IOException e) {
			System.err.println("Unable to read " + versionFilePath);
			e.printStackTrace();
			System.exit(1);
		}

		final String sdkVersion = versionProperties.getProperty(SDK_VERSION);
		if (sdkVersion == null) {
			System.err.println(SDK_VERSION + " property in " + versionFilePath
					+ " was missing.");
			System.err
					.println("This should be present to indicate the build level of the fully packaged build of JVM and class libraries.");
			System.exit(1);
		}

		Set<String> failures = new HashSet<String>();
		// Check each artifact exists and contains the right data.
		for (int i = 0; i < allDumps.length; i++) {
			String error = checkDumpProperty(allDumps[i].getPropertyName(),
					sysProps);
			if (error != null) {
				failures.add(error);
			}
		}
		if (!failures.isEmpty()) {
			Iterator<String> fails = failures.iterator();
			while (fails.hasNext()) {
				System.err.println(fails.next());
			}
			System.exit(1);
		}

		Map<String, String> versions = new HashMap<String, String>();

		// Keyed list for printing on error messages.
		StringBuffer keyedVersions = new StringBuffer();

		for (int i = 0; i < allDumps.length; i++) {
			String versionString = allDumps[i].getVersionString();
			keyedVersions.append(allDumps[i].getPropertyName());
			keyedVersions.append(" : '");
			keyedVersions.append(versionString);
			keyedVersions.append("'\n");
			System.err.println(allDumps[i].getPropertyName() + " : "
					+ versionString);
			versions.put(allDumps[i].getPropertyName(), versionString);
		}
		// Check all versions are the same (after extraction) and that they all
		// contain
		// the contents of version.properties.
		Iterator<String> results = versions.values().iterator();
		String version = results.next();
		while (results.hasNext()) {
			String nextVersion = results.next();
			if (!version.equals(nextVersion)) {
				System.err
						.println("Version strings from all artifacts did not match. The strings were:\n"
								+ keyedVersions);
				System.exit(1);
				break;
			}
			version = nextVersion;
		}

		// Finally the version string must contain the contends of the sdk.version property
		// from version.properties.
		if (!version.contains(sdkVersion)) {
			System.err
					.println("The version strings did not contain the value of the sdk.version property in "
							+ versionFilePath
							+ "\n"
							+ "The value of sdk.version was "
							+ sdkVersion
							+ "\n"
							+ "The version strings all matched and were: "
							+ version);
			System.exit(1);
		}
		System.out.println("Version string checks passed.");
		System.exit(0);
	}

	private static String checkDumpProperty(String property, Properties sysProps) {
		if (sysProps.getProperty(property) == null) {
			return "Required -D parameter: " + property + " not set.";
		}
		File file = new File(sysProps.getProperty(property));
		if (!file.exists()) {
			return "Filed specified by " + property + " does not exist.";
		}
		return null;
	}

	private static abstract class DumpType {

		private final String propertyName;

		protected DumpType(String propName) {
			propertyName = propName;
		}

		public abstract String getVersionString();

		public String getPropertyName() {
			return propertyName;
		}
	}

	private static class SnapDumpType extends DumpType {

		protected SnapDumpType() {
			super(SNAPDUMP_FILE_PROPERTY);
		}

		@Override
		public String getVersionString() {
			String fileName = sysProps.getProperty(getPropertyName());
			File file = new File(fileName);
			FileInputStream f;
			try {
				f = new FileInputStream(file);
				ByteBuffer data = f.getChannel().map(MapMode.READ_ONLY, 0,
						file.length());
				String j9TraceFormat_dat = sysProps.getProperty("java.home")
						+ "/lib/J9TraceFormat.dat";
				TraceContext context = TraceContext.getContext(data,
						new FileInputStream(new File(j9TraceFormat_dat)));
				return context.getVmVersionString();
			} catch (FileNotFoundException e) {
				System.err.println(e.getMessage());
				e.printStackTrace();
			} catch (IOException e) {
				System.err.println(e.getMessage());
				e.printStackTrace();
			}
			return null;
		}
	}

	private static class SystemDumpType extends DTFJDumpType {

		protected SystemDumpType() {
			super(SYSTEMDUMP_FILE_PROPERTY,
					"com.ibm.dtfj.image.j9.ImageFactory");
		}

		public String getVersionString() {
			String version = super.getVersionString();
			if (version == null) {
				return version;
			}
			/*
			 * The version string looks like:
			 * Java(TM) SE Runtime Environment(build JRE 1.6.0 IBM J9 2.6 Windows XP x86-32 build 20100901_064112 (pwi3260sr9-20100818_03(SR9)))
			 * IBM J9 VM(JRE 1.6.0 IBM J9 2.6 Windows XP x86-32 20100901_064112 (JIT enabled, AOT enabled)
			 * J9VM - VM JIT - dev_20100825_16819 GC -
			 * R26_head_20100831_1631_B64043) but we only want this bit: JRE 1.6.0 IBM J9 2.6 Windows XP x86-32 build 20100901_064112 (pwi3260sr9-20100818_03(SR9))
			 */
			String prefix = "Java(TM) SE Runtime Environment(build ";
			int start = version.indexOf(prefix) + prefix.length();
			int end = version.indexOf(")))") + 2;
			if( start > -1 && end > -1 && end > start) {
				version = version.substring(start, end);
			}
			return version;
		}
	}

	private static class HeapDumpType extends DTFJDumpType {

		protected HeapDumpType() {
			super(HEAPDUMP_FILE_PROPERTY, "com.ibm.dtfj.phd.PHDImageFactory");
		}
	}

	private abstract static class DTFJDumpType extends DumpType {

		private final String factoryName;

		protected DTFJDumpType(String propertyName, String factoryName) {
			super(propertyName);
			this.factoryName = factoryName;
		}

		@Override
		public String getVersionString() {
			String fileName = sysProps.getProperty(getPropertyName());
			File file = new File(fileName);
			String version = null;
			try {
				Class<ImageFactory> factoryClass = (Class<ImageFactory>)Class.forName(factoryName);
				ImageFactory factory = factoryClass
						.newInstance();
				Image image = factory.getImage(file);
				Iterator asItr = image.getAddressSpaces();
				while (asItr.hasNext() && version == null) {
					Object nextAS = asItr.next();
					if (!(nextAS instanceof ImageAddressSpace)) {
						continue;
					}
					ImageAddressSpace as = (ImageAddressSpace) nextAS;
					Iterator psItr = as.getProcesses();
					while (psItr.hasNext() && version == null) {
						Object nextPS = psItr.next();
						if (!(nextPS instanceof ImageProcess)) {
							continue;
						}
						ImageProcess ps = (ImageProcess) nextPS;
						Iterator rtItr = ps.getRuntimes();
						while (rtItr.hasNext() && version == null) {
							Object nextRT = rtItr.next();
							if (!(nextRT instanceof JavaRuntime)) {
								continue;
							}
							JavaRuntime rt = (JavaRuntime) nextRT;
							version = rt.getVersion();
						}
					}
				}
			} catch (Exception e) {
				System.err
						.println("Exception getting version information from system dump: "
								+ e.getMessage());
				e.printStackTrace();
			}
			if (version == null) {
				return null;
			}
			return version;
		}
	}

	private static class JavaDumpType extends TextDumpType {

		private static final String EYECATCHER = "1CIJAVAVERSION ";

		protected JavaDumpType() {
			super(JAVACORE_FILE_PROPERTY, EYECATCHER);
		}
	}

	private static class ClassicHeapDumpType extends TextDumpType {

		private static final String EYECATCHER = "// Version: ";

		protected ClassicHeapDumpType() {
			super(HEAPDUMP_CLASSIC_FILE_PROPERTY, EYECATCHER);
		}

	}

	private abstract static class TextDumpType extends DumpType {

		private final String eyeCatcher;

		protected TextDumpType(String propertyName, String eyeCatcher) {
			super(propertyName);
			this.eyeCatcher = eyeCatcher;
		}

		@Override
		public String getVersionString() {
			// Find the string that starts 1CIJAVAVERSION
			// and return the contents of that line.
			String fileName = sysProps.getProperty(getPropertyName());
			File file = new File(fileName);
			FileReader f;
			String version = null;
			try {
				f = new FileReader(file);
				BufferedReader in = new BufferedReader(f);
				String line = in.readLine();
				while (line != null) {
					if (line.startsWith(eyeCatcher)) {
						version = line.substring(eyeCatcher.length());
						break;
					}
					line = in.readLine();
				}
			} catch (FileNotFoundException e) {
				System.err.println(e.getMessage());
				e.printStackTrace();
			} catch (IOException e) {
				System.err.println(e.getMessage());
				e.printStackTrace();
			}
			return version;
		}
	}
}
