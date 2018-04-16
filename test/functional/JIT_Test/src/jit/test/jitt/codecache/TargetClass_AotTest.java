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
package jit.test.jitt.codecache;

import java.util.ArrayList;
import java.util.Random;

public class TargetClass_AotTest {
	ArrayList<Integer> evenList = new ArrayList<Integer>();
	ArrayList<Integer> oddList = new ArrayList<Integer>();
	Random r = new Random();

	public int callee(int count) {
		if ( count == 1 ) {
			return 1;
		}
		else {
			int res = 0 ;
			for ( int i = 0 ; i < count ; i++ ) {
				for ( int j = 0 ; j < count ; j++ ) {
					for ( int k = 0 ; k < count ; k++ ) {
						res = res + 1;

						int n = r.nextInt();

						if ( n % 2 == 0 ) {
							evenList.add(n);
						} else {
							oddList.add(n);
						}

						if ( n < 200 ) {
							if ( (n - n*k) < (n - n*i) ) {
								for ( int z = 0 ; z < n ; z++ ) {
									//new Helper().doWork(z);
									new Helper().doWork(z);
								}
							}
						}
					}
				}
			}
			return res;
		}
	}
	private static class Helper{
		public int doWork(int n) {
			 if ( n > 0 ) return n;
			 else return -1;
		}
	}
}
