/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;

/**
 * Class representing a preprocess copy simple copy (as opposed to a source that must be preprocessed). The SimpleCopy class is
 * responsible for copying entire folder or single files to the appropriate destination.
 */
public class SimpleCopy {
	private File sourcePath = null; /* The source directory or file to the copy. */
	private File outputDir = null; /* The output directory for the copy. */
	private int copyFileCount = 0; /* The number of copy files for each source directory. */
	private final Map<File, String[]> copyFilesBySourceDir = new HashMap<>(); /* the relative paths of all of the files for a given sourceDir. */
	private String baseDir = "";
	private String simpleOutput = "";

	/**
	 * Returns the copy task's output directory.
	 *
	 * @return      the output directory
	 */
	public File getOutputDir() {
		return this.outputDir;
	}

	/**
	 * Sets the copy task's output directory.
	 *
	 * @param       outputDir       the task's output directory
	 */
	public void setOutputDir(File outputDir) {
		if (outputDir == null) {
			throw new NullPointerException();
		}
		this.outputDir = outputDir;
	}

	/**
	 * Returns the copy task's source path.
	 *
	 * @return      the source path
	 */
	public File getSourcePath() {
		return this.sourcePath;
	}

	/**
	 * Sets the copy task's source path.
	 *
	 * @param       sourcePath      the source path
	 */
	public void setSourcePath(File sourcePath) {
		if (sourcePath == null) {
			throw new NullPointerException();
		}

		this.sourcePath = sourcePath;
	}

	/**
	 * Sets the simplecopy base directory.
	 *
	 * @param       baseDir         the base directory
	 */
	public void setBaseDir(String baseDir) {
		this.baseDir = baseDir;
	}

	/**
	 * Sets the output path of of this simplecopy task.  This can either be
	 * an absolute path or one relative to the configuration output path.
	 *
	 * @param       simpleOutput    the simplecopy output path
	 */
	public void setSimpleOutput(String simpleOutput) {
		this.simpleOutput = simpleOutput;
	}

	/**
	 * Performs the build.
	 */
	public boolean copy() {
		System.out.println("\nCopying from " + sourcePath.getAbsolutePath());

		if (sourcePath.isDirectory()) {
			computeBuildFiles();
			copyFileCount = 0;
			String[] copyFiles = copyFilesBySourceDir.get(sourcePath);

			for (String copyFile : copyFiles) {
				File sourceFile = new File(sourcePath, copyFile);
				File outputFile = new File(outputDir, copyFile);

				// Exclude CVS files so the generated project does not display as shared in a repository
				if (outputFile.getParentFile().getName().indexOf("CVS") > -1) {
					outputFile.delete();
					continue;
				}

				try {
					copyFile(sourceFile, outputFile);
					copyFileCount++;
				} catch (IOException e) {
					System.out.println("IOException occured in file " + sourceFile.getAbsolutePath() + ", copy failed.");
					e.printStackTrace();
					return false;
				} catch (Exception e) {
					System.out.println("Exception occured in file " + sourceFile.getAbsolutePath() + ", copy failed.");
					e.printStackTrace();
					return false;
				}
			}
		} else {
			File sourceFile = sourcePath;
			File outputFile = null;

			if (simpleOutput.startsWith(File.separator) || simpleOutput.startsWith(":", 1)) {
				outputFile = new File(simpleOutput, sourcePath.getName());
			} else if (!simpleOutput.equals("")) {
				outputFile = new File(new File(outputDir, simpleOutput), sourcePath.getName());
			} else {
				String parent = sourcePath.getParent();
				parent = parent.substring(baseDir.length());

				StringBuffer strBuffer = new StringBuffer(outputDir.getAbsolutePath());
				StringTokenizer st = new StringTokenizer(parent, File.separator);

				if (st.countTokens() > 2) {
					st.nextToken();
					String token = st.nextToken();

					if (!token.equals("src")) {
						strBuffer.append(File.separator);
						strBuffer.append(token);
					}

					while (st.hasMoreTokens()) {
						strBuffer.append(File.separator);
						strBuffer.append(st.nextToken());
					}
				}

				strBuffer.append(File.separator);
				strBuffer.append(sourcePath.getName());

				outputFile = new File(strBuffer.toString());
			}

			try {
				copyFile(sourceFile, outputFile);
				copyFileCount++;
			} catch (IOException e) {
				System.out.println("IOException occured in file " + sourceFile.getAbsolutePath() + ", copy failed.");
				e.printStackTrace();
				return false;
			}
		}

		System.out.println(copyFileCount + " file(s) copied.\n");
		return true;
	}

	public static void copyFile(File inputFile, File outputFile) throws IOException {
		outputFile.getParentFile().mkdirs();
		outputFile.createNewFile();

		try (InputStream input = new FileInputStream(inputFile)) {
			try (OutputStream output = new FileOutputStream(outputFile)) {
				byte[] buffer = new byte[4096];

				for (int numBytes; (numBytes = input.read(buffer)) != -1;) {
					output.write(buffer, 0, numBytes);
				}
			}
		}
	}

	/**
	 * Recursively searches the given root directory to find all files. The file
	 * paths are returned, relative to the root directory.
	 */
	private static List<String> getFiles(File root) {
		List<String> fileList = new ArrayList<>();
		File[] files = root.listFiles();

		if (files != null) {
			getFiles(files, "", fileList);
		} else {
			System.out.print("Error reading the source directory \"");
			System.out.print(root.getAbsolutePath());
			System.out.println("\" - No Files copied");
		}

		return fileList;
	}

	/**
	 * This is a helper function to getFiles(File);
	 */
	private static void getFiles(File[] files, String relativePath, List<String> fileList) {
		for (File file : files) {
			String filePath = relativePath + file.getName();

			if (file.isFile()) {
				fileList.add(filePath);
			} else {
				getFiles(file.listFiles(), filePath + File.separator, fileList);
			}
		}
	}

	private void computeBuildFiles() {
		List<String> copyFiles = getFiles(sourcePath);
		String[] buildFilesArray = copyFiles.toArray(new String[copyFiles.size()]);

		copyFileCount += buildFilesArray.length;
		copyFilesBySourceDir.put(sourcePath, buildFilesArray);
	}
}
