/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.oti.VMCPTool;

import java.util.HashSet;
import java.util.Set;


import com.ibm.j9tools.om.BuildSpec;
import com.ibm.j9tools.om.ConfigDirectory;
import com.ibm.j9tools.om.Flag;
import com.ibm.j9tools.om.FlagDefinitions;
import com.ibm.j9tools.om.ObjectFactory;

class UmaFlagInfo  implements IFlagInfo {

	private BuildSpec buildSpec;
	private FlagDefinitions flagDefs;

	public UmaFlagInfo(String configDirectory, String buildSpecId) throws Throwable {
		ObjectFactory factory = new ObjectFactory(new ConfigDirectory(configDirectory));
		factory.initialize();
		buildSpec = factory.getBuildSpec(buildSpecId);
		flagDefs = factory.getFlagDefinitions();
	}

	public Set<String> getAllSetFlags() {
		HashSet<String> allSetFlags = new HashSet<String>();
		for ( String flagId : buildSpec.getFlags().keySet() ) {
			if ( isFlagSet(flagId) ) { 
				allSetFlags.add(flagId);
			}
		}
		
		return allSetFlags;
	}

	public boolean isFlagValid(String flag) {
		flag = Util.transformFlag(flag);
		if (flagDefs.getFlagDefinition(flag) == null ) {
			return false;
		}
		return true;
	}

	private boolean isFlagSet(String flagId) {
		flagId = Util.transformFlag(flagId);
		if ( isFlagValid(flagId) ) {
			Flag flag = buildSpec.getFlags().get(flagId);
			if ( flag != null ) {
				return flag.getState();
			}
		}
		return false;
	}
}
