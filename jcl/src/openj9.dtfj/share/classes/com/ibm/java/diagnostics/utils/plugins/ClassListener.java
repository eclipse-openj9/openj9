/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils.plugins;

import java.util.Set;

/**
 * A subset of the ASM ClassVisitor and AnnotationVisitor methods on a
 * single interface to allow specific visitor actions to be carried out.
 * 
 * @author adam
 *
 */
public interface ClassListener {
	
	public void visit(int version, int access, String name, String signature, String superName, String[] interfaces);
	
	public void visitAnnotation(String classname, boolean visible);
	
	public void visitAnnotationValue(String name, Object value);
	
	public void scanComplete(Entry entry);
	
	/**
	 * List the plugins that have been found by this listener for inclusion into the 
	 * context to be returned to the user.
	 * 
	 * @return list of plugins found, an empty list or null if there are none
	 */
	public Set<PluginConfig> getPluginList();
}
