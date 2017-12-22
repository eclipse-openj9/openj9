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

import freemarker.template.TemplateHashModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;

public class Spec implements TemplateHashModel {

	public TemplateModel get(String arg0) throws TemplateModelException {
		if (arg0.equals("flags")) {
			return new Flags();
		}
		if (arg0.equals("tools")) {
			return new Tools();
		}
		if (arg0.equals("properties")) {
			return new Properties();
		}
		if (arg0.equals("artifacts")) {
			return new Artifacts(UMA.getUma().getArtifacts());
		}
		if (arg0.equals("type")) {
			return new Type();
		}
		if (arg0.equals("processor")) {
			return new Processor();
		}
		try {
			TemplateModel platformExtension = com.ibm.uma.UMA.getUma().getPlatform().getDataModelExtension("uma.spec", arg0);
			if (platformExtension!=null) return platformExtension;
			TemplateModel configurationExtension = com.ibm.uma.UMA.getUma().getConfiguration().getDataModelExtension("uma.spec", arg0);
			if (configurationExtension!=null) return configurationExtension;
		} catch (UMAException e) {
			throw new TemplateModelException(e);
		}
		return null;
	}

	public boolean isEmpty() throws TemplateModelException {
		return false;
	}

}
