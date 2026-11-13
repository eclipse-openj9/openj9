/*
 * Copyright IBM Corp. and others 2026
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 * Assisted-by: IBM Bob
 */
package org.openj9.test.jep442;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.ref.Reference;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import org.testng.Assert;
import org.testng.annotations.Test;

/**
 * Closes a shared arena while other threads are reading from it through ByteBuffer views.
 *
 * ByteBuffer.get on a buffer created by MemorySegment.asByteBuffer reaches a @Scoped
 * method that the JIT inlines into the reading loop. Closing the arena must notice the
 * accessing threads even though the @Scoped method is inlined, so that the memory is never
 * freed while it is still being read. The reading loop, number of threads and delay
 * before the close are the same as the BufferAccessor case in the OpenJDK test
 * java/foreign/TestHandshake.java, so that the loop runs long enough to be compiled at
 * the highest optimization level.
 *
 * Correct outcomes: an IllegalStateException in a worker (Java 22+: the close
 * interrupts the access) or from close() (Java 21: close fails while the arena is
 * in use). Anything else in a worker, such as a NullPointerException from
 * reading freed memory, is a failure.
 */
@Test(groups = { "level.sanity" })
public class SharedArenaCloseTests {
	private static final int SIZE = 1_000_000;
	private static final int ROUNDS = 20;
	private static final int MAX_DELAY_MILLIS = 500;
	private static final int NUM_WORKERS = Math.min(10, Runtime.getRuntime().availableProcessors());

	private static void readBuffer(ByteBuffer bb) {
		int sum = 0;
		for (int i = 0; i < bb.capacity(); i++) {
			sum += bb.get(i);
		}
	}

	private static void readBufferHoldingOther(ByteBuffer bb, MemorySegment other) {
		/* Keep the other segment's session live in this compiled frame while the @Scoped
		 * reads of bb run, without accessing the other segment.
		 */
		MemorySegment.Scope otherScope = other.scope();
		int sum = 0;
		for (int i = 0; i < bb.capacity(); i++) {
			sum += bb.get(i);
		}
		Reference.reachabilityFence(otherScope);
	}

	@Test
	public void test_closeArenaDuringAccess() throws Throwable {
		runRounds(false);
	}

	@Test
	public void test_closeUnrelatedArenaDuringAccess() throws Throwable {
		runRounds(true);
	}

	/**
	 * @param closeOther if false, close the arena being read; if true, close a
	 *        second arena whose session the workers hold live without accessing it.
	 *        In the second case the check in closeScope0 is conservative, so the
	 *        workers may either continue normally or get an IllegalStateException.
	 */
	private static void runRounds(boolean closeOther) throws Throwable {
		for (int round = 0; round < ROUNDS; round++) {
			Arena readArena = Arena.ofShared();
			Arena otherArena = closeOther ? Arena.ofShared() : readArena;
			MemorySegment segment = readArena.allocate(SIZE);
			MemorySegment other = otherArena.allocate(SIZE);

			CountDownLatch started = new CountDownLatch(1);
			AtomicReference<Throwable> unexpected = new AtomicReference<>();
			AtomicBoolean stop = new AtomicBoolean(false);
			Thread[] workers = new Thread[NUM_WORKERS];
			for (int w = 0; w < NUM_WORKERS; w++) {
				ByteBuffer bb = segment.asByteBuffer();
				workers[w] = new Thread(() -> {
					try {
						while (!stop.get()) {
							if (closeOther) {
								readBufferHoldingOther(bb, other);
							} else {
								readBuffer(bb);
							}
							started.countDown();
						}
					} catch (IllegalStateException expected) {
						/* The arena was closed while (or before) this thread accessed it. */
					} catch (Throwable t) {
						unexpected.compareAndSet(null, t);
					} finally {
						started.countDown();
					}
				});
				workers[w].start();
			}
			started.await();
			Thread.sleep(ThreadLocalRandom.current().nextInt(MAX_DELAY_MILLIS));

			Arena toClose = closeOther ? otherArena : readArena;
			try {
				toClose.close();
			} catch (IllegalStateException inUse) {
				/* Java 21: close fails while another thread is accessing the arena. */
			}

			/* Stop the workers (on Java 21 the close above may have failed, so they
			 * may still be running), then close both arenas.
			 */
			stop.set(true);
			for (Thread worker : workers) {
				worker.join();
			}
			closeQuietly(readArena);
			closeQuietly(otherArena);

			if (null != unexpected.get()) {
				Assert.fail("Unexpected exception in worker (round " + round + ")", unexpected.get());
			}
		}
	}

	private static void closeQuietly(Arena arena) {
		try {
			arena.close();
		} catch (IllegalStateException ignored) {
			/* Already closed. */
		}
	}
}
