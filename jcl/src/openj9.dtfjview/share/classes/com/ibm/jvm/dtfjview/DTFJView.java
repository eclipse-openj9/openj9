/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;

import com.ibm.java.diagnostics.utils.plugins.PluginConstants;
import com.ibm.jvm.dtfjview.spi.ISession;

// For DTFJ View tests, see VariedTest.java in com.ibm.dtfj.tests.scenarios in
//  the DTFJ Tests project.  VariedTest will produce a dump file that will fairly
//  comprehensively test DTFJ View.  See the comment at the top of VariedTest.java
//  for more details.

public class DTFJView {

	private ISession session;

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		DTFJView dtfjView = new DTFJView();
		dtfjView.launch(args);
	}
	
	public void launch(String[] args) {
		setPluginsPath();
		session = Session.getInstance(args);
		((Session)session).run();
	}
	
	/*
	 * There is no longer a hard coded list of in-built jdmpview commands, instead the dynamic discovery mechanism
	 * introduced by the introduction of support for DTFJ plugins is used. In order to do this the current class path
	 * is searched and if a entry for dtfjview.jar is located then it is prepended to the plugins system property. This
	 * will have the effect that at runtime the dtfjview jar is scanned and all plugins are added, which gives the in-built
	 * set of commands. Prepending this path ensures that in the event of a namespace clash with a subsequently developed
	 * plugin, the in-built command will take precedence.
	 */
	
	private void setPluginsPath() {
		ClassLoader loader = getClass().getClassLoader();
		if(loader instanceof URLClassLoader) {
			URLClassLoader urlloader = (URLClassLoader) loader;
			URL[] urls = urlloader.getURLs();
			for(URL url : urls) {
				try {
					File file = new File(url.toURI());
					if(file.exists() && file.getName().equalsIgnoreCase("dtfjview.jar")) {
						addSelfToPath(file);
						return;			//can exit the loop once the property has been modified
					}
				} catch (Exception e) {
					//ignore any exceptions
				}
			}
		}
	}
	
	private void addSelfToPath(File jar) throws IOException {
		String prop = System.getProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY);
		if(prop == null) {
			//system property is not set
			System.setProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY, jar.getCanonicalPath());
			return;
		}
		String[] paths = prop.split(File.pathSeparator);
		for(String path : paths) {
			File file = new File(path);
			if(file.exists() && file.equals(jar)) {
				return;		//already added
			}
		}
		prop = jar.getCanonicalPath() + File.pathSeparator + prop;
		System.setProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY, prop);
	}
}
