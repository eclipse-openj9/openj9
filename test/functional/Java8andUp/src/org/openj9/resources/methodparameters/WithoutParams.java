package org.openj9.resources.methodparameters;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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


/**
 * IMPORTANT NOTE: This class needs to be compiled WITHOUT -parameters option.
 *
 * @author fkaraman
 *
 */
public class WithoutParams {

	public enum MOOD {
		HAPPY,
		SAD
	}

	public class InnerClass {
	}

	public WithoutParams() {
	}

	public WithoutParams(boolean firstParam) {
	}

	public WithoutParams(boolean firstParam, final int secondFinalParam) {
	}

	public WithoutParams(boolean firstParam, final int secondFinalParam, String thirdParam) {
	}

	public void sampleMethod0() {
	}

	public void sampleMethod1(boolean firstParam) {
	}

	public void sampleMethod2(boolean firstParam, final int secondFinalParam) {
	}

	public void sampleMethod3(boolean firstParam, final int secondFinalParam, String thirdParam) {
	}
}
