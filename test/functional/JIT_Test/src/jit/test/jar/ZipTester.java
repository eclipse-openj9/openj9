/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package jit.test.jar;

import java.io.*;
import java.util.*;
import java.util.zip.*;
import org.testng.log4testng.Logger;
import org.testng.Assert;

/**
 * @author jistead
 *
 * To change this generated comment edit the template variable "typecomment":
 * Window>Preferences>Java>Templates.
 * To enable and disable the creation of type comments go to
 * Window>Preferences>Java>Code Generation.
 */
public class ZipTester {
	private static Logger logger = Logger.getLogger(ZipTester.class);
	public static final String CLASS_FILTER_TAG = "-class:";
	public static final String CLASS_FILTER_SUBSTRING = "substring";
	public static final String CLASS_FILTER =
		CLASS_FILTER_TAG + CLASS_FILTER_SUBSTRING;
	public static final String ZIP_NAMES = "zip1 zip2 ...";
	public static final String CLASS_FILTER_HELPER =
		CLASS_FILTER
			+ " --> load classes whose names contain "
			+ CLASS_FILTER_SUBSTRING;
	public static final String JAR_NAMES_HELPER =
		ZIP_NAMES
			+ " --> load classes from zip (or jar) files with fully qualified filenames.";

	public static void main(String args[]) {
		new ZipTester().run(args);
	}
	
	public void run(String args[]) {
		if (args.length < 1 || args[0].isEmpty()) {
			printUsageText();
			Assert.fail();
		}

		String classFilter = args[0];
		int zipFilenameIndex = 0;
		if (!classFilter.startsWith(CLASS_FILTER_TAG))
			classFilter = "";
		else {
			classFilter = classFilter.substring(CLASS_FILTER_TAG.length());
			zipFilenameIndex = 1;
		}

		process(classFilter, args, zipFilenameIndex);
	}

	public String getClassName() {
		return "jit.runner.ZipTester";
	}

	protected ZipTestClassLoader loader;

	public ZipTester() {
		loader = new ZipTestClassLoader();
	}

	public void printUsageText() {
		logger.debug(getGenericCommandline());
		printGenericCommandlineExplanation();
		logger.debug("");
		printCommandlineSamples();
	}

	protected void printCommandlineSamples() {
		logger.debug("\t" + getCommandlineSample1());
		logger.debug("\t" + getCommandlineSample2());
		logger.debug("\t" + getCommandlineSample3());
		logger.debug("\t" + getCommandlineSample4());
	}

	public String getCommandlineSample1() {
		return getClassName() + " c:\\foo.zip d:\\foo.jar";
	}

	public String getCommandlineSample2() {
		return getClassName() + " \"c:\\Directory Name With Spaces\\foo.zip\"";
	}

	public String getCommandlineSample3() {
		return getClassName()
			+ " "
			+ CLASS_FILTER_TAG
			+ "java.lang.Object c:\\foo.zip d:\\foo.jar";
	}

	public String getCommandlineSample4() {
		return getClassName()
			+ " "
			+ CLASS_FILTER_TAG
			+ "java.lang c:\\foo.zip";
	}

	protected String getGenericCommandline() {
		return getClassName() + " " + CLASS_FILTER + " " + ZIP_NAMES;
	}

	protected void printGenericCommandlineExplanation() {
		logger.debug("\t" + CLASS_FILTER_HELPER);
		logger.debug("\t" + JAR_NAMES_HELPER);
	}

	public boolean addZip(String zipPath) {
		ZipFile zipFile = null;

		try {
			zipFile = new ZipFile(zipPath);
			loader.addZipFile(zipFile);
		} catch (IOException e) {
			logger.debug("Error reading/opening zip file: (" + zipPath + ")");
			return false;
		}
		return true;
	}

	public void process(
		String classFilter,
		String[] fullyQualifiedZipFilenames,
		int index) {
		long beginTime = System.currentTimeMillis();

		for (; index < fullyQualifiedZipFilenames.length; index++) {
			File isADir = new File(fullyQualifiedZipFilenames[index]);
			if ( isADir.isDirectory() ) {
				String files[] = isADir.list();
				for ( int fileIndex = 0; fileIndex <files.length; ++ fileIndex ) {
					addZip(isADir.getAbsolutePath() + System.getProperty("file.separator") + files[fileIndex]);
				}
			} else {
				addZip(fullyQualifiedZipFilenames[index]);
			}
		}

		loader.processZipFiles(classFilter);
		long endTime = System.currentTimeMillis();
		loader.closeZipFiles();

		long totalTime = endTime - beginTime;
		long totalTimeS = totalTime / 1000;
		long totalTimeMS = totalTime - totalTimeS * 1000;
		StringBuffer buff = new StringBuffer("Elapsed time:\t");
		buff.append(String.valueOf(totalTimeS));
		buff.append(".");
		buff.append(String.valueOf(totalTimeMS));
		buff.append(" seconds.");
		logger.debug(buff.toString());
	}
}
