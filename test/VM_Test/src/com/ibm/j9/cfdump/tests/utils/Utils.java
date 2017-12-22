/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
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
package com.ibm.j9.cfdump.tests.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.JarURLConnection;
import java.net.URL;
import java.net.URLConnection;

/**
 * Utils for testing cfdump.
 */
@SuppressWarnings("nls")
public class Utils {

	/**
	 * Starts a daemon thread to consume the given input stream.
	 *
	 * @param input the stream to be consumed
	 */
	private static void consumeAndIgnore(InputStream input) {
		Runnable sink = () -> {
			byte[] buffer = new byte[1024];

			try {
				while (input.read(buffer) > 0) {
					/* consume and ignore */
				}

				input.close();
			} catch (IOException e) {
				/* ignore */
			}
		};

		Thread thread = new Thread(sink, "null-sink");

		thread.setDaemon(true);
		thread.start();
	}

	/**
	 * Close stdin of the given process; consume and ignore any output
	 * it writes to stdout; and return the stderr stream.
	 *
	 * @param process
	 * @return BufferedReader
	 * @throws IOException
	 */
	public static BufferedReader getBufferedErrorStreamOnly(Process process) throws IOException {
		/* We no input to send to cfdump. */
		process.getOutputStream().close();

		/* Consume and ignore anything written to stdout. */
		consumeAndIgnore(process.getInputStream());

		return new BufferedReader(new InputStreamReader(process.getErrorStream()));
	}

	/**
	 * Invoke cfdump on the specified class on the current classpath,
	 * with the specified command-line arguments.
	 *
	 * @param klass the class that cfdump will operate on.
	 * @param arg command-line arguments to cfdump.
	 * @return cfdump Process, or null if failed.
	 *
	 * @throws IOException
	 */
	public static Process invokeCfdumpOnClass(Class<?> klass, String arg) throws IOException {
		String sourceFile = getJarFileForClass(klass);
		String sourceFileFlag = "-z:";
		if (null == sourceFile){
			/* Try looking in lib/modules */
			sourceFile = System.getProperty("java.home") + "/lib/modules";
			sourceFileFlag = "-j:";
		}
		if (!new File(sourceFile).exists()) {
			System.out.println("Could not get jar file for class: " + klass.getCanonicalName());
			return null;
		}

		String cfdump = getCfdumpPath();
		String command[] = new String[] {
			cfdump,
			arg,
			sourceFileFlag + sourceFile,
			klass.getCanonicalName()
		};

		System.out.print("Running command: ");
		for (String s : command) {
			System.out.print(s + " ");
		}
		System.out.println();

		// Must set workingDir as cfdump on AIX uses it for the native libpath.
		File workingDir = new File(cfdump).getParentFile();
		return Runtime.getRuntime().exec(command, null, workingDir);
	}

	/**
	 * Invoke cfdump on the specified file path with the specified
	 * command-line arguments.
	 *
	 * @param klass the class that cfdump will operate on.
	 * @param arg command-line arguments to cfdump.
	 * @return cfdump Process, or null if failed.
	 *
	 * @throws IOException
	 */
	public static Process invokeCfdumpOnFile(File file, String arg) throws IOException {
		String cfdump = getCfdumpPath();
		String command[] = new String[] {
			cfdump,
			arg,
			file.getAbsolutePath()
		};

		System.out.print("Running command:");
		for (String s : command) {
			System.out.print(" " + s);
		}
		System.out.println();

		// Must set workingDir as cfdump on AIX uses it for the native libpath.
		File workingDir = new File(cfdump).getParentFile();
		return Runtime.getRuntime().exec(command, null, workingDir);
	}

	/**
	 * Finds path of cfdump utility based on java.library.path.
	 *
	 * @return path to cfdump
	 */
	public static String getCfdumpPath() {
		String[] extensions = { ".exe", "" };
		String separator = System.getProperty("path.separator");
		String[] libPathEntries = System.getProperty("java.library.path").split(separator);
		for (String path : libPathEntries) {
			for (String ext : extensions) {
				String cfdumpPath = path + "/cfdump" + ext;
				if (new File(cfdumpPath).exists()) {
					return cfdumpPath;
				}
			}
		}
		throw new RuntimeException("Error: cfdump not found on java.library.path!");
	}

	/**
	 * Finds the .jar file in which the specified class is found.
	 *
	 * @param klass
	 * @return path to jar file
	 */
	private static String getJarFileForClass(Class<?> klass) {
		URL url = klass.getResource(klass.getSimpleName() + ".class");
		if (url != null) {
			try {
				URLConnection connection = url.openConnection();
				if (connection instanceof JarURLConnection) {
					JarURLConnection urlConnection = (JarURLConnection) connection;
					return new File(urlConnection.getJarFileURL().getFile()).getCanonicalPath();
				}
			} catch (IOException e) {
				throw new RuntimeException(e);
			}
		}
		return null;
	}

	/**
	 * Returns true if the specified line exactly matches some line in the input stream,
	 * false otherwise.
	 *
	 * @param expectedLine
	 * @param in
	 * @return
	 * @throws IOException
	 */
	public static boolean findLineInStream(String expectedLine, BufferedReader in) throws IOException {
		try {
			for (String line; (line = in.readLine()) != null;) {
				if (line.equals(expectedLine)) {
					return true;
				}
			}
		} finally {
			in.close();
		}
		return false;
	}

}
