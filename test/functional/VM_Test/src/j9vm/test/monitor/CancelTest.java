/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jun 6, 2006
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.monitor;

/**
 * 
 * @author PBurka
 *
 * Tests reservation cancellation.
 * 
 * On platforms where lock reservation is not supported this will not
 * really test anything, but should be harmless.
 */
public class CancelTest {

	private static CancelTest object = new CancelTest();
	
	public static void main(String[] args) throws InterruptedException {

		Helpers.monitorReserve(object);
		
		Thread thr = new Thread() {
			public void run() {
				synchronized (object) {
					System.out.println("Succesfully cancelled monitor reservation");
				}
			}
		};
		
		synchronized (object) {
			System.out.println("Succesfully entered monitor before reservation cancelled by another thread");
		}

		thr.start();

		thr.join();

		synchronized (object) {
			System.out.println("Succesfully entered monitor after reservation cancelled by another thread");
		}
		
	}

}
