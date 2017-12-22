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

import com.ibm.uma.IConfiguration;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;

public class PlatformZOS extends PlatformUnix {
	public PlatformZOS( IConfiguration buildSpec ) {
		super(buildSpec);
	}
	
	protected String getZosSharedLibLinkName(Artifact artifact) {
		String libname = "lib";
		if ( artifact.appendRelease() ) {
			libname += artifact.getTargetName() + getStream();
		} else {
			libname += artifact.getTargetName();
		}
		libname += ".x";
		return libname;
	}
	
	protected String getxsrc(Artifact artifact) {
		return getZosSharedLibLinkName(artifact);
	}
	
	protected String getxdestdir(Artifact artifact) {
		String scope = "";
		if ( artifact.getScope() != null ) {
			scope = artifact.getScope() + "/";
		}
		return UMA.UMA_PATH_TO_ROOT_VAR + "lib/" + scope;
	}

	protected String getxdest(Artifact artifact) {
		return getxdestdir(artifact) + getZosSharedLibLinkName(artifact);
	}

	
	@Override
	public String getLibLinkName(Artifact artifact) throws UMAException {
		String libname = "-l";
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE: // FALL-TRHU
		case Artifact.TYPE_SHARED:
			libname = getxdest(artifact);
			break;
		case Artifact.TYPE_STATIC:
			libname += artifact.getTargetName();
			break;
		}
		return libname;
	}
	
	@Override
	protected void addPlatformSpecificLibraryLocationInformation(Artifact artifact, StringBuffer buffer) throws UMAException {
		super.addPlatformSpecificLibraryLocationInformation(artifact, buffer);
		buffer.append(artifact.getTargetNameWithScope() + "_xsrc=" + getxsrc(artifact)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_xdest=" + getxdest(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_xdestdir=" + getxdestdir(artifact) + "\n");
	}
}
