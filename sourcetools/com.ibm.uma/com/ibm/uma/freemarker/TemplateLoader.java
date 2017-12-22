/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.freemarker;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Set;
import java.util.Vector;

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.util.Logger;

import freemarker.cache.URLTemplateLoader;

public class TemplateLoader extends URLTemplateLoader {
	
	HashMap<String, URL> allTemplates = new HashMap<String, URL>();
	HashMap<String, URL> allIncludeTemplates = new HashMap<String, URL>();
	
	public TemplateLoader() throws UMAException {
		// Find and cache all templates in the source tree
		addAllTemplatesFoundInDirectory(new File(UMA.getUma().getRootDirectory()));
	}
	
	public Set<String> getTemplates() {
		return allTemplates.keySet();
	}

	void addAllTemplatesFoundInDirectory(File dir) throws UMAException {
		String rootDir = UMA.getUma().getRootDirectory();
		File [] dirListing = dir.listFiles();
		Vector<File> directories = new Vector<File>();
		for ( File file : dirListing ) {
			if ( file.isDirectory() ) {
				directories.add(file);
			} else if ( file.getName().endsWith(".ftl") || file.getName().endsWith(".iftl")) {
				String templatePath = file.getParent();
				if ( templatePath.equalsIgnoreCase(rootDir) || rootDir.equalsIgnoreCase(templatePath+File.separator)) { 
					templatePath = "";
				} else {
					templatePath = file.getParent().substring(rootDir.length());
				}
				templatePath = templatePath.replace(File.separator, "/");
				String templateName = (templatePath.length()==0?"":templatePath+"/") + file.getName();
				URL url;
				try {
					url = new URL("file:///" + file.getCanonicalPath());
				} catch (MalformedURLException e) {
					throw new UMAException(e);
				} catch (IOException e) {
					throw new UMAException(e);
				}
				HashMap<String, URL> templates = allTemplates;
				if (file.getName().endsWith(".inc.ftl") || file.getName().endsWith(".iftl")) {
					templates = allIncludeTemplates;
				}
				templates.put(templateName, url);
				Logger.getLogger().println(Logger.InformationL2Log, "Found template " + templateName + " URL: " + url.toExternalForm());
			} 
		}
		for ( File file : directories ) {
			addAllTemplatesFoundInDirectory(file);
		}

	}
	@Override
	protected URL getURL(String arg0) {
		URL url = allTemplates.get(arg0);
		if ( url == null ) {
			url = allIncludeTemplates.get(arg0);
		}
		return url;
	}

}
