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

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;

import freemarker.ext.beans.BeansWrapper;
import freemarker.ext.beans.BooleanModel;
import freemarker.template.SimpleScalar;
import freemarker.template.TemplateHashModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;
import freemarker.template.TemplateScalarModel;

public class Property implements TemplateHashModel, TemplateScalarModel {
	
	String name;
	
	public Property(String propertyName) {
		name = propertyName;
	}

	public TemplateModel get(String arg0) throws TemplateModelException {
		if ( arg0.equalsIgnoreCase("name") ) {
			return new SimpleScalar(name);
		}
		try {		
			if ( arg0.equals("value")) {
				String value = UMA.getUma().getPlatform().replaceMacro(name);
				if (value == null ) {
					throw new UMAException("property " + name + " is not defined.");
				}
				return new SimpleScalar(value);
			}
			if ( arg0.equals("defined")) {
				return new BooleanModel(UMA.getUma().getPlatform().replaceMacro(name) != null, new BeansWrapper());
			}
		} catch (UMAException e) {
			throw new TemplateModelException(e);
		}
		return null;
	}

	public boolean isEmpty() throws TemplateModelException {
		return false;
	}
	
	public String getAsString() throws TemplateModelException {
		return name;
	}

}
