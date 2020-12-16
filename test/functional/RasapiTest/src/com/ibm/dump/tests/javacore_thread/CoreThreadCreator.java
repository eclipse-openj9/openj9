/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.javacore_thread;

import com.ibm.dump.tests.javacore_thread.CoreThreadCreator;

import java.lang.Thread.State;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReentrantLock;

public abstract class CoreThreadCreator implements Runnable {

	public static final CoreThreadCreator[] setupJobs = {
		new CoreThreadCreator.BlockedOnSynchronized(), new CoreThreadCreator.BlockedOnJUCLock(),
		new CoreThreadCreator.TimedBlockedOnJUCLock(), new CoreThreadCreator.DeadlockedOnJUCLock(),
		new CoreThreadCreator.BlockedOnFIFOMutex(), new CoreThreadCreator.BlockedOnOwnableFIFOMutex(),
		new CoreThreadCreator.ParkedThread(), new CoreThreadCreator.WaitingOnSynchronized(),
		new CoreThreadCreator.WaitingOnUnowned() };
	
	public abstract String getThreadNameToCheck();

	public String getLockName() {
		return "<unknown>";
	}

	public String getOwnerName() {
		return "<unknown>";
	}

	// Default implementation
	public boolean checkBlockingLine(String line) {

		boolean good = true;
		good &= line.contains("Parked on: " + getLockName());
		// Threads are quoted, unknowns aren't.
		good &= line.contains("Owned by: " + getOwnerName()) || line.contains("Owned by: \"" + getOwnerName() + "\"");
		return good;
	}

	public boolean checkWaitingLine(String line) {

		boolean good = true;
		good &= line.contains("Parked on: " + getLockName());
		// Threads are quoted, unknowns aren't.
		good &= line.contains("Owned by: " + getOwnerName()) || line.contains("Owned by: \"" + getOwnerName() + "\"");
		return good;
	}
	
	public boolean checkJdmpviewInfoThreadLine(String line) {

		boolean good = true;
		good &= line.contains(getLockName() + "@0x");
		good &= line.contains("owner name: \"" + getOwnerName() + "\" owner id: ");
		return good;
	}
	
	public abstract boolean isReady();

	public static class ParkedThread extends CoreThreadCreator {

		private Thread parkingThread = null;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			parkingThread = Thread.currentThread();
			parkingThread.setName(getThreadNameToCheck());
			LockSupport.park();
		}

		@Override
		public String getThreadNameToCheck() {
			return "ParkedThread: parked thread";
		}
		
		public boolean checkJdmpviewInfoThreadLine(String line) {

			boolean good = true;
			good &= line.contains("<unknown>");
			return good;
		}
	}

	public static class BlockedOnJUCLock extends CoreThreadCreator {

		private final ReentrantLock jucLock = new ReentrantLock(true);

		private Thread parkingThread = null;
		private Thread blockingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			blockingThread = new Thread(new Runnable() {
				public void run() {
					jucLock.lock();
					lockTaken = true;
					while (true) {
						try {
							// Hold the lock forever, but don't terminate.
							Thread.sleep(1000);
						} catch (InterruptedException e) {
						}
					}
				}
			});
			blockingThread.setName(getOwnerName());
			blockingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			parkingThread = new Thread(new Runnable() {
				public void run() {
					jucLock.lock();
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "BlockedOnJUCLock: waiting thread";
		}

		public String getLockName() {
			return "java/util/concurrent/locks/ReentrantLock$FairSync";
		}

		public String getOwnerName() {
			return "BlockedOnJUCLock: lock owner";
		}
	}

	public static class TimedBlockedOnJUCLock extends CoreThreadCreator {

		private final ReentrantLock jucLock = new ReentrantLock(true);

		private Thread parkingThread = null;
		private Thread blockingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.TIMED_WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			blockingThread = new Thread(new Runnable() {
				public void run() {
					jucLock.lock();
					lockTaken = true;
					while (true) {
						try {
							// Hold the lock forever, but don't terminate.
							Thread.sleep(1000);
						} catch (InterruptedException e) {
						}
					}
				}
			});
			blockingThread.setName("TimedBlockedOnJUCLock: lock owner");
			blockingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			parkingThread = new Thread(new Runnable() {
				public void run() {
					try {
						// We want a thread that will be in a timed park,
						// but without any risk of it actually waking up.
						jucLock.tryLock(10, TimeUnit.DAYS);
					} catch (InterruptedException e) {
					}
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "TimedBlockedOnJUCLock: waiting thread";
		}
		
		public String getLockName() {
			return "java/util/concurrent/locks/ReentrantLock$FairSync";
		}

		public String getOwnerName() {
			return "TimedBlockedOnJUCLock: lock owner";
		}
	}

	public static class DeadlockedOnJUCLock extends CoreThreadCreator {

		private final ReentrantLock jucLock = new ReentrantLock(true);

		private Thread parkingThread = null;
		private Thread setupThread = null;

		public boolean isReady() {
			if (setupThread != null
					&& setupThread.getState() == State.TERMINATED
					&& parkingThread != null
					&& parkingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			jucLock.lock();
			setupThread = Thread.currentThread();
			setupThread.setName(getOwnerName());
			parkingThread = new Thread(new Runnable() {
				public void run() {
					jucLock.lock();
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "DeadlockedOnJUCLock: waiting thread";
		}

		@Override
		public String getLockName() {
			return "java/util/concurrent/locks/ReentrantLock$FairSync";
		}

		@Override
		public String getOwnerName() {
			return "DeadlockedOnJUCLock: lock owner (dead)";
		}

		@Override
		public boolean checkBlockingLine(String line) {
			boolean good = super.checkBlockingLine(line);
			// Make sure the J9VMThread for this case is null.
			good &= line.contains("Owned by: \"" + getOwnerName()
					+ "\" (J9VMThread:<null>, java/lang/Thread:");
			return good;
		}
		
		@Override
		public boolean checkJdmpviewInfoThreadLine(String line) {
			boolean good = super.checkJdmpviewInfoThreadLine(line);
			// Make sure the J9VMThread for this case is null.
			good &= line.contains("owner name: \"" + getOwnerName() + "\" owner id: <null>");
			return good;
		}
	}

	public static class BlockedOnFIFOMutex extends CoreThreadCreator {

		private final FIFOMutex fifoMutex = new FIFOMutex();

		private Thread parkingThread = null;
		private Thread blockingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			blockingThread = new Thread(new Runnable() {
				public void run() {
					fifoMutex.lock();
					lockTaken = true;
					// Hold the lock forever, but don't terminate.
					while (true) {
						try {
							Thread.sleep(1000);
						} catch (InterruptedException e) {
						}
					}
				}
			});
			blockingThread.setName(getOwnerName());
			blockingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			parkingThread = new Thread(new Runnable() {
				public void run() {
					fifoMutex.lock();
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "BlockedOnFIFOMutex: waiting thread";
		}

		@Override
		public String getLockName() {
			return fifoMutex.getClass().getName().replace('.', '/');
		}

	}

	public static class BlockedOnOwnableFIFOMutex extends CoreThreadCreator {

		private final OwnableFIFOMutex fifoMutex = new OwnableFIFOMutex();

		private Thread parkingThread = null;
		private Thread blockingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			blockingThread = new Thread(new Runnable() {
				public void run() {
					fifoMutex.lock();
					lockTaken = true;
					while (true) {
						try {
							// Hold the lock forever, but don't terminate.
							Thread.sleep(1000);
						} catch (InterruptedException e) {
						}
					}
				}
			});
			blockingThread.setName(getOwnerName());
			blockingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			parkingThread = new Thread(new Runnable() {
				public void run() {
					fifoMutex.lock();
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "BlockedOnOwnableFIFOMutex: waiting thread";
		}

		@Override
		public String getLockName() {
			return fifoMutex.getClass().getName().replace('.', '/');
		}

		@Override
		public String getOwnerName() {
			return "BlockedOnOwnableFIFOMutex: lock owner";
		}
	}

	public static class BlockedOnSynchronized extends CoreThreadCreator {

		private final String lock = "Lock for BlockedOnSynchronized";

		private Thread parkingThread = null;
		private Thread blockingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (parkingThread != null
					&& parkingThread.getState() == State.BLOCKED) {
				return true;
			}
			return false;
		}
		
		public boolean isSynchronizedLock() {
			return true;
		}

		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			blockingThread = new Thread(new Runnable() {
				public void run() {
					synchronized (lock) {
						lockTaken = true;
						while (true) {
							try {
								// Hold the lock forever, but don't terminate.
								Thread.sleep(1000);
							} catch (InterruptedException e) {
							}
						}
					}
				}
			});
			blockingThread.setName(getOwnerName());
			blockingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			parkingThread = new Thread(new Runnable() {
				public void run() {
					synchronized (lock) {
					}
				}
			});
			parkingThread.setName(getThreadNameToCheck());
			parkingThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "BlockedOnSynchronized: blocked thread";
		}

		@Override
		public String getLockName() {
			return lock.getClass().getName().replace('.', '/');
		}

		@Override
		public String getOwnerName() {
			return "BlockedOnSynchronized: lock owner";
		}

		public boolean checkBlockingLine(String line) {

			boolean good = true;
			good &= line.contains("Blocked on: " + getLockName());
			// Threads are quoted, unknowns aren't.
			good &= line.contains("Owned by: " + getOwnerName()) || line.contains("Owned by: \"" + getOwnerName() + "\"");
			return good;
		}
	}

	public static class WaitingOnSynchronized extends CoreThreadCreator {

		private final String lock = "Lock for WaitingOnSynchronized";

		private Thread owningThread = null;
		private Thread waitingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (waitingThread != null
					&& waitingThread.getState() == State.WAITING
					&& owningThread != null
					&& owningThread.getState() == State.TIMED_WAITING) {
				return true;
			}
			return false;
		}

		public boolean isSynchronizedLock() {
			return true;
		}
		
		public void run() {
			// First thread will wait on the object lock
			waitingThread = new Thread(new Runnable() {
				public void run() {
					synchronized (lock) {
						lockTaken = true;
						try {
								lock.wait();
						} catch (InterruptedException e) {
						}
					}
				}
			});
			waitingThread.setName(getThreadNameToCheck());
			waitingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
			// Second thread will take and own the lock forever
			owningThread = new Thread(new Runnable() {
				public void run() {
					synchronized (lock) {
						while (true) {
							try {
								Thread.sleep(1000);
							} catch (InterruptedException e) {
							}
						}
					}
				}
			});
			owningThread.setName(getOwnerName());
			owningThread.start();
		}

		@Override
		public String getThreadNameToCheck() {
			return "WaitingOnSynchronized: waiting thread";
		}

		@Override
		public String getLockName() {
			return lock.getClass().getName().replace('.', '/');
		}

		@Override
		public String getOwnerName() {
			return "WaitingOnSynchronized: lock owner";
		}

		public boolean checkBlockingLine(String line) {

			boolean good = true;
			good &= line.contains("Waiting on: " + getLockName());
			// Threads are quoted, unknowns aren't.
			good &= line.contains("Owned by: " + getOwnerName()) || line.contains("Owned by: \"" + getOwnerName() + "\"");
			return good;
		}
		
		public boolean checkJdmpviewInfoThreadLine(String line) {

			boolean good = true;
			good &= line.contains(getLockName() + "@0x");
			good &= line.contains("owner name: \"" + getOwnerName() + "\" owner id:");
			return good;
		}
	}
	
	public static class WaitingOnUnowned extends CoreThreadCreator {

		private final String lock = "Lock for WaitingOnUnowned";

		private Thread owningThread = null;
		private Thread waitingThread = null;
		private volatile boolean lockTaken = false;

		public boolean isReady() {
			if (waitingThread != null
					&& waitingThread.getState() == State.WAITING) {
				return true;
			}
			return false;
		}

		public boolean isSynchronizedLock() {
			return true;
		}
		
		public void run() {
			// Take the lock, thread will terminate without releasing
			// leaving the second thread blocked.
			waitingThread = new Thread(new Runnable() {
				public void run() {
					synchronized (lock) {
						lockTaken = true;
						try {
								lock.wait();
						} catch (InterruptedException e) {
						}
					}
				}
			});
			waitingThread.setName(getThreadNameToCheck());
			waitingThread.start();
			while (!lockTaken) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
				}
			}
		}

		@Override
		public String getThreadNameToCheck() {
			return "WaitingOnUnowned: waiting thread";
		}

		@Override
		public String getLockName() {
			return lock.getClass().getName().replace('.', '/');
		}

		public String getOwnerName() {
			return "<unowned>";
		}
		
		public boolean checkBlockingLine(String line) {

			boolean good = true;
			good &= line.contains("Waiting on: " + getLockName());
			// Threads are quoted, unknowns aren't.
			good &= line.contains("Owned by: " + getOwnerName()) || line.contains("Owned by: \"" + getOwnerName() + "\"");
			return good;
		}
		
		public boolean checkJdmpviewInfoThreadLine(String line) {

			boolean good = true;
			System.err.println("Checking: |" + line +"|");
			good &= line.contains(getLockName() + "@0x");
			good &= line.contains("owner name: " + getOwnerName());
			return good;
		}
	}

	public boolean isSynchronizedLock() {
		return false;
	}
	
	public boolean isAbstractOwnableSynchronizer() {
		return false;
	}

	
//	public static class DeadlockCycleOnJUCLock extends CoreThreadCreator {
//
//		private final ReentrantLock jucLockA = new ReentrantLock(true);
//		private final ReentrantLock jucLockB = new ReentrantLock(true);
//
//		private Thread threadA = null;
//		private Thread threadB = null;
//
//		public boolean isReady() {
//			if (jucLockA.isLocked() && jucLockB.isLocked() && threadB != null
//					&& jucLockA.isLocked()
//					&& threadA != null
//					&& threadA.getState() == State.WAITING) {
//				return true;
//			}
//			return false;
//		}
//
//		public void run() {
//			parkingThread = new Thread(new Runnable() {
//				public void run() {
//					jucLockB.lock();
//					jucLockA.lock();
//				}
//			});
//			parkingThread.setName(getThreadNameToCheck());
//			parkingThread.start();
//		}
//
//		@Override
//		public String getThreadNameToCheck() {
//			return "DeadlockedOnJUCLock: waiting thread";
//		}
//
//		@Override
//		public String getLockName() {
//			return "java/util/concurrent/locks/ReentrantLock$FairSync";
//		}
//
//		@Override
//		public String getOwnerName() {
//			return "DeadlockedOnJUCLock: lock owner (dead)";
//		}
//
//		@Override
//		public boolean checkBlockingLine(String line) {
//			boolean good = super.checkBlockingLine(line);
//			// Make sure the J9VMThread for this case is null.
//			good &= line.contains("Owned by: \"" + getOwnerName()
//					+ "\" (J9VMThread:<null>, java/lang/Thread:");
//			return good;
//		}
//	}
}