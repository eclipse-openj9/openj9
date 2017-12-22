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

public class Flag implements TemplateScalarModel {
	
	ConfigurationImpl config;
	com.ibm.j9tools.om.Flag flag;
	String operation;
	
	public Flag(String flagName, String operation, ConfigurationImpl config) {
		this.config = config;
		this.flag = config.getFlags().get(flagName);
		this.operation = operation;
	}

	public String getAsString() throws TemplateModelException {
		if (operation.equalsIgnoreCase("description")) {
			return flag.getDescription();
		} else if (operation.equalsIgnoreCase("ifRemoved")) {
			return flag.getIfRemoved();
		} else if (operation.equalsIgnoreCase("category")) {
			return flag.getCategory();
		} else if (operation.equalsIgnoreCase("component")) {
			return flag.getComponent();
		}
		return null;
	}

}
