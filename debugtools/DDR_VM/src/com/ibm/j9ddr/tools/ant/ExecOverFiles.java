/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ant;

import java.util.LinkedList;
import java.util.List;

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.DirectoryScanner;
import org.apache.tools.ant.Task;
import org.apache.tools.ant.types.FileSet;
import org.apache.tools.ant.taskdefs.ExecTask;
import org.apache.tools.ant.types.Commandline;

import java.io.File;

/**
 * Ant task for executing a command over a fileset
 * and replacing the content of each file with the
 * output of the command. 
 * $FILENAME is replaced with the name of the file
 * 
 * usage:
 * 
 * <executeoverfiles command="... $FILENAME">
 * 	<fileset ... />
 * </executeoverfiles>
 * 
 * @author srwley
 *
 */
public class ExecOverFiles extends Task {
	private List<FileSet> fileSets = new LinkedList<FileSet>();
	private final ExecTask exec = new ExecTask();
	private String command = null;
	
	private final String FILENAME_REPLACE = "$FILENAME";
	
	public void addFileset(FileSet fileset) {
		fileSets.add(fileset);
	}
		
	public void setCommand(String cmd) {
		// replaces $FILENAME with the appropriate file name
		command = cmd;
	}
	
	public void execute() throws BuildException {
		if (exec == null) {
			throw new BuildException("No <exec> nested element supplied.");
		}
		if (command == null || command.length() == 0) {
			throw new BuildException("No non-empty command attribute supplied.");
		}
		
		for (FileSet fs : fileSets) {
			DirectoryScanner scanner = fs.getDirectoryScanner(getProject());
			String files[] = scanner.getIncludedFiles();
			exec.setDir(scanner.getBasedir());
			
			for (String filename : files) {
				Commandline cmd = new Commandline(command.replace(FILENAME_REPLACE, filename));
			
				exec.setCommand(cmd);
				// overwrites file with output
				exec.setOutput(new File(scanner.getBasedir() + "/" + filename));
				exec.execute();
			}
		}
	}
	
}
