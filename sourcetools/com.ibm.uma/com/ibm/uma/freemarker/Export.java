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

import freemarker.template.SimpleScalar;
import freemarker.template.TemplateHashModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;

public class Export implements TemplateHashModel {
	
	com.ibm.uma.om.Export export;
	
	public Export(com.ibm.uma.om.Export export) {
		this.export = export;
	}

	public TemplateModel get(String arg0) throws TemplateModelException {
		if ( arg0.equals("name")) {
			return new SimpleScalar(export.getExport());
		}
		if ( arg0.equals("native_type")) {
			// TODO: J9 specific
			String nativeType = "inative";
			if ( export.getExport().startsWith("Java_")
				|| export.getExport().equals("JNI_OnLoad")
				|| export.getExport().equals("JNI_OnUnload")
				|| export.getExport().equals("JVM_OnLoad")
				|| export.getExport().equals("JVM_OnUnload") ) {
				nativeType = "native";
			}
				
			return new SimpleScalar(nativeType);
		}
		return null;
	}

	public boolean isEmpty() throws TemplateModelException {
		return false;
	}

}
