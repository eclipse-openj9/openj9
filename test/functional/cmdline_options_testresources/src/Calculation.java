/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

public class Calculation {

	public static void main(String[] args) {
		int efail = 1000;
		if(args.length ==1){
			/*
			 * efail represents the upper bound memory allocation failure we intent.
			 * efail is calculated as the sum of sfail (input from allocfib.xml) and 500.
			 * Since 1000 iterations takes too long to complete, so we choose 500 iterations.
			 */			
			efail = Integer.parseInt(args[0]) + 500;
		}
		// spit out the efail value on System.out. The value can be picked up by the cmdlinetester app.
		System.out.println(efail);
	}
}
