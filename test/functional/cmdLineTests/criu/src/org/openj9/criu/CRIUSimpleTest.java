
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.criu;

import java.nio.file.Paths;
import java.nio.file.Path;

public class CRIUSimpleTest {

	public static void main(String args[]) {
		try {
			int num_checkpoints=Integer.parseInt(args[1]);
			checkpoints(num_checkpoints);
		} catch (NumberFormatException e) {
			System.out.println("Input is not a valid integer");
		}
	}

	public static void checkpoints(int num_checkpoints) {
		Path path = Paths.get("cpData");
		System.out.println("Total checkpoint(s) " + num_checkpoints + ":\nPre-checkpoint");
		for (int cur_checkpint = 1; cur_checkpint <= num_checkpoints; ++cur_checkpint) {
			CRIUTestUtils.checkPointJVM(path);
			System.out.println("Post-checkpoint " + cur_checkpint);
		}
	}
}
