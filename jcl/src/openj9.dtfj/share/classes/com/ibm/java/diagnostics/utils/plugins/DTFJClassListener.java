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

import java.util.HashSet;
import java.util.Set;

class DTFJClassListener implements ClassListener {
	private final Set<PluginConfig> plugins = new HashSet<PluginConfig>();
	private final String id;		//optional ID for making this listener a singleton
	
	public DTFJClassListener() {
		id = null;
	}
	
	public DTFJClassListener(String id) {
		this.id = id;
	}
	
	public void visit(int version, int access, String name,	String signature, String superName, String[] interfaces) {
		//do nothing
	}

	public void visitAnnotation(String classname, boolean visible) {
		//do nothing
	}

	public void visitAnnotationValue(String name, Object value) {
		// do nothing
	}

	public void scanComplete(Entry entry) {
		ClassInfo info = entry.getData();
		if((info != null) && info.hasAnnotation(DTFJPluginManager.ANNOTATION_CLASSNAME)) {
			DTFJPluginConfig config = new DTFJPluginConfig(entry);
			config.processAnnotations();
			plugins.add(config);
		}
	}

	public Set<PluginConfig> getPluginList() {
		return plugins;
	}

	@Override
	public boolean equals(Object o) {
		if(id == null) {
			return super.equals(o);
		} else {
			if(o instanceof DTFJClassListener) {
				DTFJClassListener listener = (DTFJClassListener) o;
				return id.equals(listener.id);
			}
			return false;
		}
	}

	@Override
	public int hashCode() {
		if(id == null) {
			return super.hashCode();
		} else {
			return id.hashCode();
		}
	}
	
	
}
