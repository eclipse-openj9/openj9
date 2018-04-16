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
package com.ibm.jvmti.tests.BCIWithASM;

public class Source7 {
	public int method1(int key){
		switch (key) {
		case 100 : 
			//System.out.println("jumping to case 200 from 100");
			return 1; //<--- Inject : replace this by jump to goto case 200
		case 200 : 
			return 2;
		case 300 :			//<-- inject these 2 cases
			return 3;
		case 400 : 
			//System.out.println("jumping to case 300 from 400");
			return 4; //replace this by jump to go to case 300
		case 500 : 
			return 5;
		default :
			if ( key == 1000 ) {
				return 1;
			} else {
				//System.out.println("jump to if from else");
				return 2;// <-- inject : replace this by jump to if block 
			}
		}
	}
}
