/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 *******************************************************************************/
package org.openj9.criu;

import java.nio.file.Paths;
import java.nio.file.Path;
import java.nio.file.Files;
import java.nio.file.attribute.PosixFilePermissions;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;

import org.eclipse.openj9.criu.CRIUSupport;
import org.eclipse.openj9.criu.SystemRestoreException;

public class CRIUTestUtils {
	public final static Path imagePath = Paths.get("cpData");

	public static void deleteCheckpointDirectory(Path path) {
		try {
			if (path.toFile().exists()) {
				Files.walk(path).map(Path::toFile).forEach(File::delete);

				Files.deleteIfExists(path);
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public static void createCheckpointDirectory(Path path) {
		try {
			path.toFile().mkdir();
			Files.setPosixFilePermissions(path, PosixFilePermissions.fromString("rwxrwxrwx"));
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public static void checkPointJVM(Path path) {
		checkPointJVM(path, true);
	}

	public static void checkPointJVM(Path path, boolean deleteDir) {
		checkPointJVM(null, path, deleteDir);
	}

	public static void checkPointJVM(CRIUSupport criu, Path path, boolean deleteDir) {
		if (CRIUSupport.isCRIUSupportEnabled()) {
			deleteCheckpointDirectory(path);
			createCheckpointDirectory(path);
			try {
				if (criu == null) {
					criu = new CRIUSupport(path);
				}
				showThreadCurrentTime("Performing CRIUSupport.checkpointJVM()");
				criu.setLeaveRunning(false).setShellJob(true).setFileLocks(true).checkpointJVM();
			} catch (SystemRestoreException e) {
				e.printStackTrace();
			}
			if (deleteDir) {
				deleteCheckpointDirectory(path);
			}
		} else {
			throw new RuntimeException("CRIU is not enabled");
		}
	}

	public static CRIUSupport prepareCheckPointJVM(Path path) {
		if (CRIUSupport.isCRIUSupportEnabled()) {
			deleteCheckpointDirectory(path);
			createCheckpointDirectory(path);
			return (new CRIUSupport(path)).setLeaveRunning(false).setShellJob(true).setFileLocks(true);
		} else {
			throw new RuntimeException("CRIU is not enabled");
		}
	}

	public static void checkPointJVMNoSetup(CRIUSupport criu, Path path, boolean deleteDir) {
		if (criu != null) {
			try {
				showThreadCurrentTime("Performing CRIUSupport.checkpointJVM()");
				criu.checkpointJVM();
			} catch (SystemRestoreException e) {
				e.printStackTrace();
			}
			if (deleteDir) {
				deleteCheckpointDirectory(path);
			}
		} else {
			throw new RuntimeException("CRIU is not enabled");
		}
	}

	public static void showThreadCurrentTime(String logStr) {
		System.out.println(logStr + ", current thread name: " + Thread.currentThread().getName() + ", " + new Date()
				+ ", System.currentTimeMillis(): " + System.currentTimeMillis() + ", System.nanoTime(): "
				+ System.nanoTime());
	}


	public static Path createOptionsFile(String name, String contents) {
		try {
			File options = new File(name);
			if (!options.createNewFile()) {
				System.out.println("WARN: File already exists but should not");
			}

			try (FileWriter writer = new FileWriter(options)) {
				writer.write(contents);
			}
			options.deleteOnExit();
			return options.toPath();
		} catch (IOException e) {
			e.printStackTrace();
		}
		return null;
	}
}
