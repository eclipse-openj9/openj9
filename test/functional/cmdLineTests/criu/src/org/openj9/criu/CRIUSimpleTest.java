
/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package org.openj9.criu;

import java.nio.file.Paths;
import java.nio.file.Path;

public class CRIUSimpleTest {

	public static void main(String args[]) {
		String test = args[0];

		switch(test) {
		case "SingleCheckpoint":
			singleCheckpoint();
			break;
		case "TwoCheckpoints":
			twoCheckpoints();
			break;
		case "ThreeCheckpoints":
			threeCheckpoints();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}
	}

	public static void singleCheckpoint() {
		Path path = Paths.get("cpData");
		System.out.println("Single checkpoint:\nPre-checkpoint");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint");
	}

	public static void twoCheckpoints() {
		Path path = Paths.get("cpData");
		System.out.println("Two checkpoints:\nPre-checkpoint");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint 1");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint 2");
	}

	public static void threeCheckpoints() {
		Path path = Paths.get("cpData");
		System.out.println("Three checkpoints:\nPre-checkpoint");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint 1");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint 2");
		CRIUTestUtils.checkPointJVM(path);
		System.out.println("Post-checkpoint 3");
	}
}
