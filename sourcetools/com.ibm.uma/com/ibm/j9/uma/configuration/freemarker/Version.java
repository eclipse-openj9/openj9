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
package com.ibm.j9.uma.configuration.freemarker;

import com.ibm.j9.uma.configuration.ConfigurationImpl;

import freemarker.template.SimpleScalar;
import freemarker.template.TemplateHashModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;

public class Version implements TemplateHashModel {
	
	ConfigurationImpl config;
	
	public Version(ConfigurationImpl config) {
		this.config = config;
	}

	public TemplateModel get(String arg0) throws TemplateModelException {
		if (arg0.equals("major")) {
			return new SimpleScalar(config.getBuildInfo().getMajorVersion());
		} else if (arg0.equals("minor")) {
			return new SimpleScalar(config.getBuildInfo().getMinorVersion());
		} else if (arg0.equals("branch")) {
			return new SimpleScalar(config.getBuildInfo().getVmBranch());
		} else if (arg0.equals("streamName")) {
			return new SimpleScalar(config.getBuildInfo().getStreamName());
		} else if (arg0.equals("parentStream")) {
			return new SimpleScalar(config.getBuildInfo().getParentStream());
		} else if (arg0.equals("streamSplitDate")) {
			return new SimpleScalar(config.getBuildInfo().getStreamSplitDate().toString());
		} else if (arg0.equals("prefix")) {
			return new SimpleScalar(config.getBuildInfo().getPrefix());
		} 
		return null;
	}

	public boolean isEmpty() throws TemplateModelException {
		return false;
	}

}
