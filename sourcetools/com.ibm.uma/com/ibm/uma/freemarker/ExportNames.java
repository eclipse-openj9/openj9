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

import java.util.Iterator;

import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Export;

import freemarker.template.TemplateCollectionModel;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelException;
import freemarker.template.TemplateModelIterator;

public class ExportNames implements TemplateCollectionModel {

	Artifact artifact;
	
	public ExportNames(Artifact artifact) throws UMAException {
		this.artifact = artifact;
	}
	
	class ExportIterator implements TemplateModelIterator {
		Iterator<Export> iterator;
	
		public ExportIterator(Artifact artifact) throws UMAException {
			iterator = artifact.getExportedFunctions().iterator();
		}
		public boolean hasNext() throws TemplateModelException {
			return iterator.hasNext();
		}
		public TemplateModel next() throws TemplateModelException {
			return new com.ibm.uma.freemarker.Export(iterator.next());		
		}
	}
	
	public TemplateModelIterator iterator() throws TemplateModelException {
		try {
			return new ExportIterator(artifact);
		} catch (UMAException e) {
			throw new TemplateModelException(e);
		}
	}
	
}
