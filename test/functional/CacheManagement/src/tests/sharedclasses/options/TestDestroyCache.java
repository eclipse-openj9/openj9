/*******************************************************************************
 * Copyright IBM Corp. and others 2010
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

package tests.sharedclasses.options;

import tests.sharedclasses.*;
import java.io.*;

/*
 * Check various scenarios handled in j9shr_destroy_cache() 
 * with respect to deleting old and current generation cache.
 * For details see JAZZ design 37044.
 */
public class TestDestroyCache extends TestUtils {
	String cacheName = "testcache";
	String destroyPersistentCache, destroyNonPersistentCache;
	String infiniteLoopOutput = "Running infinite loop";

	public static void main(String args[]) {
		TestDestroyCache tdc = new TestDestroyCache();
		tdc.destroyPersistentCache = getCommand(DestroyPersistentCacheCommand, tdc.cacheName);
		tdc.destroyNonPersistentCache = getCommand(DestroyNonPersistentCacheCommand, tdc.cacheName);
		tdc.testCase1();
		tdc.testCase2();
		tdc.testCase3();
		tdc.testCase4();
		tdc.testCase5();
		tdc.testCase6();
	}

	private void testCase1() {
		String expectedErrorMessages[] = new String[] { 
			"JVMSHRC428I" /* Removed older generation of shared class cache "<cache name>" */ 
		};
		
   		runDestroyAllCaches();
   		
   		/* When only older generation cache exists and is not in use */
   		if (isMVS() == false) {
			runSimpleJavaProgramWithPersistentCache(cacheName, "createOldGen");
			RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
   		}
   		
   		if (realtimeTestsSelected() == false) {
			runSimpleJavaProgramWithNonPersistentCache(cacheName, "createOldGen");
			RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
   		}
   		
		runDestroyAllCaches();
	}
	
	private void testCase2() {
		Process p1 = null;
		String expectedErrorMessages[];
		
		runDestroyAllCaches();
		
		/* When only current generation cache exists and is in use */
   		if (isMVS() == false) {
			try {
				if (isWindows()) {
					expectedErrorMessages = new String[] {
						"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
					};
				} else {
					/* Linux, AIX allow to destroy an active persistent cache */
					expectedErrorMessages = new String[] {
						"has been destroyed" /* Persistent shared cache "<cache name>" has been destroyed */
					};
				}
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */ 
					runSimpleJavaProgramWithPersistentCache(cacheName);
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (realtimeTestsSelected()) {
					destroyPersistentCache(cacheName);
				}
			}
   		}
   		
		if (realtimeTestsSelected() == false) {
			try {
				expectedErrorMessages = new String[] {
					"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
				};
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		}
		
		runDestroyAllCaches();
	}
	
	private void testCase3() {
		Process p1 = null;
		String expectedErrorMessages[];
		
		runDestroyAllCaches();

		/* When only older generation cache exists and is in use */
   		if (isMVS() == false) {
			try {
				if (isWindows()) {
					expectedErrorMessages = new String[] { 
						"JVMSHRC429I" /* Failed to remove older generation of shared class cache "<cache name>" */
					};
				} else {
					/* Linux, AIX allow to destroy an active persistent cache */
					expectedErrorMessages = new String[] { 
						"JVMSHRC428I" /* Removed older generation of shared class cache "<cache name>" */
					};
				}
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */
					runSimpleJavaProgramWithPersistentCache(cacheName, "createOldGen");
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (realtimeTestsSelected()) {
					destroyPersistentCache(cacheName);
				}
			}
   		}
   		
		if (realtimeTestsSelected() == false) {
			try {
				expectedErrorMessages = new String[] { 
					"JVMSHRC429I" /* Failed to remove older generation of shared class cache "<cache name>" */
				};
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		}
		
		runDestroyAllCaches();
	}
	
	private void testCase4() {
		Process p1 = null;
		String expectedErrorMessages[];
		
		runDestroyAllCaches();
		
		/* When older generation cache is not in use but current generation cache is in use */
		if (isMVS() == false) {
			try {
				if (isWindows()) {
					expectedErrorMessages = new String[] { 
						"JVMSHRC428I",/* Removed older generation of shared class cache "<cache name>" */
						"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
					};
				} else {
					/* Linux, AIX allow to destroy an active persistent cache */
					expectedErrorMessages = new String[] { 
						"JVMSHRC428I",/* Removed older generation of shared class cache "<cache name>" */
						"has been destroyed" /* Persistent shared cache "<cache name>" has been destroyed */
					};
				}
				runSimpleJavaProgramWithPersistentCache(cacheName, "createOldGen");
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */
					runSimpleJavaProgramWithPersistentCache(cacheName);
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (realtimeTestsSelected()) {
					destroyPersistentCache(cacheName);
				}
			}
		}
		
		if (realtimeTestsSelected() == false) {
			try {
				expectedErrorMessages = new String[] { 
					"JVMSHRC428I",/* Removed older generation of shared class cache "<cache name>" */
					"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
				};
				runSimpleJavaProgramWithNonPersistentCache(cacheName, "createOldGen");
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		}
		
		runDestroyAllCaches();
	}
	
	private void testCase5() {
		Process p1 = null;
		String expectedErrorMessages[];		
		
		runDestroyAllCaches();
		
		/* When older generation is in use and current generation cache is not in use */
		if (isMVS() == false) {
			try {
				if (isWindows()) {
					expectedErrorMessages = new String[] { 
						"JVMSHRC429I",/* Failed to remove older generation of shared class cache "<cache name>" */
						"has been destroyed" /* Persistent shared cache "<cache name>" has been destroyed */
					};
				} else {
					/* Linux, AIX allow to destroy an active persistent cache */
					expectedErrorMessages = new String[] { 
						"JVMSHRC428I",/* Removed older generation of shared class cache "<cache name>" */
						"has been destroyed" /* Persistent shared cache "<cache name>" has been destroyed */
					};
				}
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */
					runSimpleJavaProgramWithPersistentCache(cacheName, "createOldGen");
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				runSimpleJavaProgramWithPersistentCache(cacheName);
				RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (realtimeTestsSelected()) {
					destroyPersistentCache(cacheName);
				}
			}
		}
		
		if (realtimeTestsSelected() == false) {
			try {
				expectedErrorMessages = new String[] { 
					"JVMSHRC429I",/* Failed to remove older generation of shared class cache "<cache name>" */
					"is destroyed" /* Shared cache "<cache name>" is destroyed */
				};
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				runSimpleJavaProgramWithNonPersistentCache(cacheName);
				RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		}
		
		runDestroyAllCaches();
	}
	
	private void testCase6() {
		Process p1 = null;
		Process p2 = null;
		String expectedErrorMessages[];

		runDestroyAllCaches();
		
		/* When both older generation cache and current generation cache are in use */
		if (isMVS() == false) {
			try {
				if (isWindows()) {
					expectedErrorMessages = new String[] { 
						"JVMSHRC429I",/* Failed to remove older generation of shared class cache "<cache name>" */
						"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
					};
				} else {
					/* Linux, AIX allow to destroy an active persistent cache */
					expectedErrorMessages = new String[] { 
						"JVMSHRC428I",/* Removed older generation of shared class cache "<cache name>" */
						"has been destroyed" /* Persistent shared cache "<cache name>" has been destroyed */
					};
				}
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */
					runSimpleJavaProgramWithPersistentCache(cacheName, "createOldGen");
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				if (realtimeTestsSelected()) {
					/* Need to create realtime cache before it is used by InfiniteLoop. Otherwise, it won't be written to disk */
					runSimpleJavaProgramWithPersistentCache(cacheName);
				}
				runInfiniteLoopJavaProgramWithPersistentCache(cacheName, infiniteLoopOutput);
				p2 = RunCommand.getLastProcess();
				RunCommand.execute(destroyPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (null == p2) {
					p2 = RunCommand.getLastProcess();
				}
				if (null != p2) {
					p2.destroy();
					try {
						p2.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (realtimeTestsSelected()) {
					destroyPersistentCache(cacheName);
				}
			}
		}
		
		if (realtimeTestsSelected() == false) {
			try {
				expectedErrorMessages = new String[] { 
					"JVMSHRC429I",/* Failed to remove older generation of shared class cache "<cache name>" */
					"JVMSHRC430I" /* Failed to remove current generation of shared class cache "<cache name>" */
				};
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, "createOldGen", infiniteLoopOutput);
				p1 = RunCommand.getLastProcess();
				runInfiniteLoopJavaProgramWithNonPersistentCache(cacheName, infiniteLoopOutput);
				p2 = RunCommand.getLastProcess();
				RunCommand.execute(destroyNonPersistentCache, null, expectedErrorMessages, false, false, null);
			} finally {
				if (null == p1) {
					p1 = RunCommand.getLastProcess();
				}
				if (null != p1) {
					p1.destroy();
					try {
						p1.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
				if (null == p2) {
					p2 = RunCommand.getLastProcess();
				}
				if (null != p2) {
					p2.destroy();
					try {
						p2.waitFor();
					} catch (java.lang.InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		}
		
		runDestroyAllCaches();
	}
}
