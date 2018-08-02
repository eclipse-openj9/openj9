/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package org.openj9.test.util;

import java.io.File;
import java.io.IOException;
import java.util.Objects;

public class FileUtilities {

	/**
	 * Delete root and its contents, if root is a directory.
	 * If root does not exist, this exits silently.
	 * @param root Starting point of the deletion.
	 * @throws IOException if the directory is inaccessible or cannot be deleted.
	 */
	public static void deleteRecursive(File root) throws IOException {
		deleteRecursive(root, true);
	}
	
	/**
	 * Delete root and its contents, if root is a directory.
	 * If root does not exist, this exits silently.
	 * @param root Starting point of the deletion.
	 * @param failOnError throw exception if there is a failure
	 * @throws IOException if the directory is inaccessible or cannot be deleted and failOnError is true.
	 */
	public static void deleteRecursive(File root, boolean failOnError) throws IOException {
		final String rootPath = root.getAbsolutePath();
		if (Objects.nonNull(rootPath) && root.exists()) {
			if (root.isDirectory()) {
				File[] children = root.listFiles();
				if (null != children) {
					for (File c : children) {
						deleteRecursive(c, failOnError);
					}
				} else if (failOnError) {
					throw new IOException("Error listing files in  "  //$NON-NLS-1$
							+ rootPath);
				}
			}
			if (!root.delete() && failOnError) {
				throw new IOException("Error deleting "  //$NON-NLS-1$
						+ rootPath);			
			}
		}
	}

}
