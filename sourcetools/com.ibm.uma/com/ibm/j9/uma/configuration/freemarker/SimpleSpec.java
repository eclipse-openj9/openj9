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

import freemarker.template.TemplateModelException;
import freemarker.template.TemplateScalarModel;

public class SimpleSpec implements TemplateScalarModel {
	
	ConfigurationImpl config;
	String operation;
	
	public SimpleSpec(String operation, ConfigurationImpl config) {
		this.config = config;
		this.operation = operation;
	}

	public String getAsString() throws TemplateModelException {
		if ( operation.equalsIgnoreCase("name") ) {
			return config.getBuildSpec().getName();
		} else if ( operation.equalsIgnoreCase("id") ) {
			return config.getBuildSpec().getId();
		} else if ( operation.equalsIgnoreCase("asmBuilderName") ) {
			return config.getBuildSpec().getAsmBuilder().getId();
		} else if ( operation.equalsIgnoreCase("cpuArchitecture") ) {
			return config.getBuildSpec().getCpuArchitecture();
		} else if ( operation.equalsIgnoreCase("os") ) {
			return config.getBuildSpec().getOs();
		} else if ( operation.equalsIgnoreCase("defaultJCL") ) {
			return config.getBuildSpec().getDefaultJCL().getId();
		} else if ( operation.equalsIgnoreCase("defaultSizes") ) {
			return config.getBuildSpec().getDefaultSizes().getId();
		} else if ( operation.equalsIgnoreCase("priority") ) {
			return "" + config.getBuildSpec().getPriority();
		} 
		return null;
	}

}
