/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.testKitGen;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MkTreeGen {

	private MkTreeGen() {
	}

	public static void start() {
		traverse(new ArrayList<String>());
	}

	private static boolean continueTraverse(String currentdir) {
		// If the build list is empty then generate on every project.
		if (Options.getBuildList().isEmpty()) {
			return true;
		}
		// Only generate make files for projects that are specificed in the build list.
		String[] buildListArr = Options.getBuildList().split(",");
		for (String buildPath : buildListArr) {
			buildPath = buildPath.replaceAll("\\+", "/");
			if (currentdir.contains(buildPath) || buildPath.contains(currentdir)) {
				return true;
			}
		}
		return false;
	}

	public static boolean traverse(ArrayList<String> currentdirs) {
		String absolutedir = Options.getProjectRootDir();
		String currentdir = String.join("/", currentdirs);
		if (!currentdirs.isEmpty()) {
			absolutedir = absolutedir + '/' + currentdir;
		}

		if (!continueTraverse(currentdir)) {
			return false;
		}

		File playlistXML = null;
		List<String> subdirs = new ArrayList<String>();

		File directory = new File(absolutedir);
		File[] dir = directory.listFiles();

		for (File entry : dir) {
			boolean tempExclude = false;

			// TODO clean up JCL_VERSION
			// Temporary exclusion, remove this block when JCL_VERSION separation is removed
			if ((!Options.getJdkVersion().equalsIgnoreCase("Panama")) && (!Options.getJdkVersion().equalsIgnoreCase("Valhalla"))) {
				String JCL_VERSION = "";
				if (System.getenv("JCL_VERSION") != null) {
					JCL_VERSION = System.getenv("JCL_VERSION");
				} else {
					JCL_VERSION = "latest";
				}

				// Temporarily exclude projects for CCM build (i.e., when JCL_VERSION is latest)
				String latestDisabledDir = "proxyFieldAccess Panama";

				// Temporarily exclude SVT_Modularity tests from integration build where we are
				// still using b148 JCL level
				String currentDisableDir = "SVT_Modularity OpenJ9_Jsr_292_API";

				tempExclude = ((JCL_VERSION.equals("latest")) && latestDisabledDir.contains(entry.getName()))
						|| ((JCL_VERSION.equals("current")) && currentDisableDir.contains(entry.getName()));
			}
			File file = new File(absolutedir + '/' + entry.getName());
			if (!tempExclude) {
				if (file.isFile() && entry.getName().equals(Constants.PLAYLIST)) {
					playlistXML = file;
				} else if (file.isDirectory()) {
					currentdirs.add(entry.getName());
					if (traverse(currentdirs)) {
						subdirs.add(entry.getName());
					}
					currentdirs.remove(currentdirs.size() - 1);
				}
			}
		}

		if (playlistXML != null || !subdirs.isEmpty()) {
			MkGen mg = new MkGen(playlistXML, absolutedir, currentdirs, subdirs);
			mg.start();
			return true;
		}
		return false;
	}
}