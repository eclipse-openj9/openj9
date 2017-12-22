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
package com.ibm.j9.uma.platform;

import java.util.Vector;

import com.ibm.uma.IConfiguration;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Export;
import com.ibm.uma.util.FileAssistant;

public class PlatformUnix extends PlatformImplementation {

	public PlatformUnix( IConfiguration buildSpec ) {
		super(buildSpec);
	}
	
	@Override
	public String getStaticLibPrefix() {
		return "lib";
	}

	@Override
	public String getStaticLibSuffix() {
		return ".a";
	}

	@Override
	public String getSharedLibPrefix() {
		return "lib";
	}

	@Override
	public String getSharedLibSuffix() {
		return ".so";
	}
	
	@Override
	public String getLibLinkName(Artifact artifact) throws UMAException {
		String libname = "-l";
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE: // FALL-TRHU
		case Artifact.TYPE_SHARED:
			if ( artifact.appendRelease() ) {
				libname += artifact.getTargetName() + getStream();
			} else {
				libname += artifact.getTargetName();
			}
			break;
		case Artifact.TYPE_STATIC:
			if ( isType("qnx") ) {
				return getLibName(artifact);
			}
			libname += artifact.getTargetName();
			break;
		}
		return libname;
	}
		
	
	
	@Override
	protected String addSystemLib(String lib) {
		return "-l"+lib;
	}
		
	@Override
	protected void writeExportFile(Artifact artifact) throws UMAException {
		if ( !(artifact.getType() == Artifact.TYPE_BUNDLE) && !(artifact.getType() == Artifact.TYPE_SHARED) )  return;
		if ( !isType("linux") && !isType("aix") ) return;

		String filename = UMA.getUma().getRootDirectory() + artifact.getContainingModule().getFullName() + "/" + artifact.getTargetNameWithScope() + ".exp";
		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();

		Vector<Export> exports = artifact.getExportedFunctions();
		if ( isType("linux") ) {
			buffer.append( " {\n");
			/*
			 * The linker doesn't like .exp files that has no exports after the 'global:' label.
			 * 
			 * This can happen when someone is creating a new shared library, but has yet to define any exports for it.
			 */
			if (exports.size() > 0) buffer.append( "\tglobal :");
		}
		for ( Export export : exports ) {
			if ( !export.evaluate() ) continue;
			if ( isType("aix") ) {
				buffer.append(stripExportName(export.getExport()) + "\n");
			}
			else if ( isType("linux") ) {
				buffer.append("\n\t\t" + stripExportName(export.getExport()) + ";");
			}
		}
		if ( isType("linux") ) {
			buffer.append("\n\tlocal : *;\n};\n");
		}

		fa.writeToDisk();
	}
}
