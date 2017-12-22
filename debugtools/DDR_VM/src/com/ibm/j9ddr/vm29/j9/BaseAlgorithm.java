/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

/**
 * Contains logic for doing a simple one-version match for an algorithm
 * @author andhall
 *
 */
public abstract class BaseAlgorithm implements IAlgorithm
{
	private final int vmMinorVersion;
	
	private final int algorithmVersion;
	
	protected BaseAlgorithm(int vmMinorVersion, int algorithmVersion)
	{
		this.vmMinorVersion = vmMinorVersion;
		this.algorithmVersion = algorithmVersion;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm.j9.IAlgorithm#matches(com.ibm.j9ddr.vm.j9.AlgorithmVersion)
	 */
	public boolean matches(AlgorithmVersion version)
	{
		return (version.getVMMinorVersion() == vmMinorVersion) && (version.getAlgorithmVersion() == algorithmVersion);
	}

}
