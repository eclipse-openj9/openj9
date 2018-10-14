/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
 * Class containing the boilerplate code for picking an appropriate algorithm.
 *
 * @author andhall
 */
/* In Java 8 builds, the (interim) compiler gets confused if IAlgorithm is not fully qualified. */
public abstract class AlgorithmPicker<T extends com.ibm.j9ddr.vm29.j9.IAlgorithm>
{
	private final String algorithmId;
	
	protected AlgorithmPicker(String algorithmId)
	{
		this.algorithmId = algorithmId;
	}
	
	public T pickAlgorithm()
	{
		final AlgorithmVersion version = AlgorithmVersion.getVersionOf(algorithmId);
		
		for (T thisAlg : allAlgorithms()) {
			if (thisAlg.matches(version)) {
				return thisAlg;
			}
		}
		
		throw new UnsupportedOperationException("No implementation of " + algorithmId + " available for version " + version);
	}
	
	/**
	 * Template method that returns a fresh instance of each algorithm
	 * @return
	 */
	protected abstract Iterable<? extends T> allAlgorithms();
}
