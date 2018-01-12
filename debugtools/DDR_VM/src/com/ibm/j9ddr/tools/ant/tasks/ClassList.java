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
package com.ibm.j9ddr.tools.ant.tasks;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.DirectoryScanner;
import org.apache.tools.ant.Project;
import org.apache.tools.ant.taskdefs.MatchingTask;

public class ClassList extends MatchingTask {
	private HashMap<String, Object> attribs = new HashMap<String, Object>();
	private static final String ATTRIB_DIR = "dir";
	private static final String ATTRIB_OUTPUT = "output";
	private static final String ATTRIB_FILE = "file";

    {
    	attribs.put(ATTRIB_DIR, null);
    	attribs.put(ATTRIB_OUTPUT, null);
    	attribs.put(ATTRIB_FILE, null);
    }
    
    public void setDir (File dir) {
        attribs.put(ATTRIB_DIR, dir);
    }
    
    public void setOutput(String path) {
    	attribs.put(ATTRIB_OUTPUT, path);
    }

    public void setFile(String name) {
    	attribs.put(ATTRIB_FILE, name);
    }
    
    public void execute() throws BuildException {
        checkAttributes();
        createList();
    }
    
    private void createList() {
    	File path = new File((String)attribs.get(ATTRIB_OUTPUT));
    	if(!path.exists()) {
    		boolean pathCreated = path.mkdirs();
    		if(!pathCreated) {
    			throw new BuildException("The output path for the file list [" + path.getPath() + "] could not be found");
    		}
    	}
    	File dir = (File)attribs.get(ATTRIB_DIR);
    	if(!dir.exists()) {
    		throw new BuildException("The output path for the file list [" + dir.getPath() + "] could not be found");
    	}
    	File outputFile = new File(path, (String)attribs.get(ATTRIB_FILE));
    	if(outputFile.exists()) {
    		outputFile.delete();
    	}
    	FileWriter out = null;
    	try {
			out = new FileWriter(outputFile);
			log("Created output file " + outputFile.getPath());
			DirectoryScanner ds = getDirectoryScanner(dir);
			String[] files = ds.getIncludedFiles();
			for (int i = 0; i < files.length; i++) {
			    int pos = files[i].lastIndexOf(File.separatorChar);
			    if(pos != -1) {
			    	String classname = files[i].substring(pos + 1);
			    	pos = classname.lastIndexOf('.');
			    	if(pos != -1) {
			    		String data = classname.substring(0, pos);
			    		pos = data.indexOf("Pointer");
			    		if(pos != -1) {
			    			String shortname = data.substring(0, pos);
			    			out.write(shortname.toLowerCase() + "=" + shortname + "\n");
			    		} else {
			    			out.write(data.toLowerCase() + "=" + data + "\n");
			    		}
			    		log("adding file: " + files[i]);
			    	}
			    }
			}
			out.close();
		} catch (IOException e) {
			throw new BuildException(e);
		} finally {
			if(out!=null) {
				try {
					out.close();
				} catch (Exception e) {
					log("Could not close output file " + e.getMessage(), Project.MSG_ERR);
				}
			}
		}
    }
    
    @SuppressWarnings("unchecked")
	private void checkAttributes() {
    	Iterator keys = attribs.keySet().iterator();
    	while(keys.hasNext()) {
    		String key = (String) keys.next();
    		if(attribs.get(key) == null) {
    			throw new BuildException(key + " attribute must be specified");
    		}
    		log(key + " = " + attribs.get(key), Project.MSG_DEBUG);
    	}
    }
}

