package org.openj9.test.java.security;

/*
 * Copyright IBM Corp. and others 2019
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
 */

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.util.Arrays;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public final class DigestLoop extends Thread {

    static final int len = 16;
    static final String alg = "SHA-1";
    static final int ITERATIONS = 1000000;

    static byte[] data;
    static Logger logger = Logger.getLogger(DigestLoop.class);
    static final AtomicInteger cloneCount = new AtomicInteger();

    @Test
    public void testMessageDigest() throws InterruptedException {
        data = new byte[len];
        Random r = new Random(10);
        r.nextBytes(data);

        try {
            MessageDigest md = MessageDigest.getInstance(alg);
            logger.info("Using Provider " + md.getProvider().getName());
            logger.info("Payload size: " + data.length + " bytes");
        } catch (Exception e) {
            logger.debug(e);
            Assert.fail("unexpected: " + e);
        }
        int num_threads = 20;

        Thread[] threads = new Thread[num_threads];
        for (int i = 0 ; i < num_threads ; i++) {
            threads[i] = new DigestLoop();
            threads[i].start();
        }

        for (int i = 0 ; i < num_threads ; i++) {
            threads[i].join();
        }

        System.runFinalization();
    }

    public void run() {
        try {
            loop(ITERATIONS);
        } catch (Exception e) {
            logger.debug(e);
        }
    }

    protected long loop(long numIterations) throws InterruptedException {

        Thread currentThread = Thread.currentThread();
        byte[] result = null;
        Random r2 = new Random(currentThread.getId());
        MessageDigest md = null;
        try {
            md = MessageDigest.getInstance(alg);
        } catch (NoSuchAlgorithmException e) {
            logger.debug(e);
            Assert.fail("unexpected: " + e);
        }
        int switch_int;
        for (int i = 0; i < numIterations; i++) {
            int a = r2.nextInt();
            switch_int = ((a >= 0) ? a : -a) % 6;
            try {
                if (switch_int == 1) {
                    // md = MessageDigest.getInstance(alg, java_provider);
                } else if (switch_int == 2) {
                    md.reset();
                } else if (switch_int == 3) {
                    result = md.digest(data);
                } else if (switch_int == 4) {
                     md.clone();
                } else if (switch_int == 5) {
                     md = (MessageDigest) md.clone();
                }
                if (switch_int >= 4) {
                    int count = cloneCount.getAndIncrement();
                    // Ensure the clones are being finalized to free native and heap memory.
                    if ((count % 2000) == 0) {
                        System.runFinalization();
                    }
                }
            } catch (Exception e) {
                logger.debug(e);
            }
        }
        return numIterations;
    }
}
