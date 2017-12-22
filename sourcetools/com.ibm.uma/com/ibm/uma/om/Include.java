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
package com.ibm.uma.om;

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.util.Logger;

public class Include extends PredicateList {

	public static final int TYPE_ROOT_PATH = 0;
	public static final int TYPE_ARTIFACT = 1;
	public static final int TYPE_MACRO = 2;
	public static final int TYPE_RELATIVE_PATH = 3;

	int type;
	String path;
	String macro;
	
	
	public Include( String containingFile ) {
		super(containingFile);
	}
	
	public void setPath(String path) {
		this.path = path;
	}
		
	public void setType(String typeString ) {
		if ( typeString.equalsIgnoreCase("rootpath") ) {
			type = TYPE_ROOT_PATH;
		} else if ( typeString.equalsIgnoreCase("relativepath") ) {
			type = TYPE_RELATIVE_PATH;
		} else if ( typeString.equalsIgnoreCase("artifact") ) {
			type = TYPE_ARTIFACT;
		} else if ( typeString.equalsIgnoreCase("macro") ) {
			type = TYPE_MACRO;
		}
	}
	
	public String getPath() {
		return path;
	}
	
	public int getType() {
		return type;
	}
	
	@Override
	public boolean evaluate() throws UMAException {
		if ( !super.evaluate() ) return false;
	
		if ( type == TYPE_MACRO ) {
			macro = path;
			path = UMA.getUma().getPlatform().replaceMacro(path);
			type = TYPE_ARTIFACT;
		}
	
		if ( type == TYPE_ARTIFACT ) {
			Artifact artifact = UMA.getUma().getArtifact(path);
			if ( artifact == null ) {
				// TODO turn this into hard-error vs. soft-error
				if ( macro != null ) {
					Logger.getLogger().println(Logger.WarningLog, "Warning: include path macro " + macro + " does not expand to an artifact. [" + containingFile + "]");
				} else {
					Logger.getLogger().println(Logger.WarningLog, "Warning: include path " + path + " of type artifact not found. [" + containingFile + "]");
				}
				return false;
			} else if ( artifact.evaluate() ) {
				path = artifact.getContainingModule().getFullName();
				type = TYPE_ROOT_PATH;
			} else {
				return false;
			}
			
		}
		return true;
	}
}
 
