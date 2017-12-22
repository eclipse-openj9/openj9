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
import freemarker.template.SimpleSequence;
import freemarker.template.TemplateHashModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;

public class BuildInfo implements TemplateHashModel {
	
	ConfigurationImpl config;
	
	public BuildInfo(ConfigurationImpl config) {
		this.config = config;
	}

	public TemplateModel get(String arg0) throws TemplateModelException {
		if (arg0.equals("stream")) {
			return new SimpleScalar(config.getBuildInfo().getStreamName());
		} else if (arg0.equals("version")) {
			return new Version(config);
		} else if (arg0.equals("fsroots")) {
			return new FsRoots(config);
		} else if (arg0.equals("buildid")) {
			String buildidstr = config.replaceMacro("buildid");
			int buildidint = Integer.valueOf(buildidstr);
			return new SimpleScalar(Integer.toString(buildidint));
		} else if (arg0.equals("unique_build_id")) {
			long uniqueBuildId = config.getBuildSpec().getId().hashCode();
			uniqueBuildId <<= 32; 
			String buildidstr = config.replaceMacro("buildid");
			int buildidint = Integer.valueOf(buildidstr);
			uniqueBuildId |= buildidint;
			return new SimpleScalar("0x"+Long.toHexString(uniqueBuildId).toUpperCase());
		} else if (arg0.equalsIgnoreCase("sourceControl")) {
			 return new SourceControl(config);
		} else if (arg0.equalsIgnoreCase("runtime")) {
			return new SimpleScalar(config.getBuildInfo().getRuntime());
		} else if (arg0.equalsIgnoreCase("defaultSizes")) {
			return new DefaultSizes(config);
		} else if (arg0.equalsIgnoreCase("jcls")) {
			return new SimpleSequence(config.getBuildInfo().getJCLs());
		} else if (arg0.equalsIgnoreCase("projects")) {
			return new SimpleSequence(config.getBuildInfo().getSources());
		} else if (arg0.equalsIgnoreCase("asmBuilders")) {
			return new SimpleSequence(config.getBuildInfo().getASMBuilders());
		} else if (arg0.equalsIgnoreCase("build_date")) {
			return new SimpleScalar(config.replaceMacro("build_date"));
		} else if (arg0.equalsIgnoreCase("vm_buildtag")) {
			return new SimpleScalar(config.replaceMacro("vm_build_tag"));
		} else if (arg0.equalsIgnoreCase("gc_buildtag")) {
			return new SimpleScalar(config.replaceMacro("gc_build_tag"));
		} else if (arg0.equalsIgnoreCase("jit_buildtag")) {
			return new SimpleScalar(config.replaceMacro("jit_build_tag"));
		}
		return null;
	}

	public boolean isEmpty() throws TemplateModelException {
		return false;
	}

}
