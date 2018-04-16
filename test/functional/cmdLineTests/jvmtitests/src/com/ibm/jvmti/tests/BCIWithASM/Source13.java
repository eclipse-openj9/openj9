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

import java.io.FileOutputStream;

public class Source13 {
	
	public void method1(){
		System.out.println("Source13#method1() : entering sleep-awake cycle..");
		for ( int i = 0 ; i < 10 ; i++ ) {
			try {
				Thread.sleep(10);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		System.out.println("Done!");
		method3();
	}

	public void method2(){
		System.out.println("Source13#method2() : Performing array copy..");
		double counter = 0 ;
		for ( int i = 5 ; i < 10 ; i++ ) {
			counter += Math.random();
			System.arraycopy(new String[]{"1","2","3"}, 0, new Object[]{"4","5","6"}, 1, 2);
		}
		System.out.println("Done!");
	}

	public void method3() {
		try {
			System.out.println("Source13#method3() : Performing Garbage dump");
			FileOutputStream fos = new FileOutputStream( "garbage_dump" );
			fos.write(new byte[]{1,2,3,4,5,6,7} );
			fos.flush();
			fos.close();
			System.out.println("Done!");
			method2();
		} catch (Exception e ) {}
		
	}
}
