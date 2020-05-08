package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

/**
 * This test is to verify matching behavior to the RI from the following implementation change
 * discussed in the Java 14 release notes:
 * 
 * "The specification for java.lang.Thread::interrupt allows for an implementation to only track the 
 * interrupt state for live threads, and previously this is what occurred. As of this release, the 
 * interrupt state of a Thread is always available, and if you interrupt a thread t before it is started, 
 * or after it has terminated, the query t.isInterrupted() will return true."
 */

@Test(groups = { "level.sanity" })
public class Test_Thread {

    /* interrupt thread after it has ended */
    @Test
    public void test_interruptAfterRun() throws Throwable {
        String name = "interruptAfterRun: ";
        Thread t = new Thread();

        /* run thread */
        t.start();
        t.join();

        /* Verify thread is dead and has not been interrupted */
        AssertJUnit.assertFalse(name + "thread should not be alive", t.isAlive());
        AssertJUnit.assertFalse(name + "interrupt flag should not be set", t.isInterrupted());
        
        t.interrupt();

        /* Verify that the thread was successfully interrupted. */
        AssertJUnit.assertTrue(name + "thread that has ended was was interrupted", t.isInterrupted());    
    }

    /* Verify that thread was sucessfully interrupted before it is started, and that the interrupt flag is 
     * still set after it is run. */
    @Test
    public void test_interruptAtStartSetAfterRun() throws Throwable {
        String name = "interruptAtStartSetAfterRun: ";
        Thread t = new Thread();

        /* Verify thread is dead and has not been interrupted */
        AssertJUnit.assertFalse(name + "thread should not be alive", t.isAlive());
        AssertJUnit.assertFalse(name + "interrupt flag should not be set", t.isInterrupted());

        t.interrupt();

        /* Verify that the thread was successfully interrupted. */
        AssertJUnit.assertTrue(name + "thread that has not started should have interrupt flag set", t.isInterrupted());

        /* run thread */
        t.start();
        t.join();

        /* Verify thread is dead and has been interrupted */
        AssertJUnit.assertFalse(name + "thread should be dead", t.isAlive());
        AssertJUnit.assertTrue(name + "interrupt flag should be set", t.isInterrupted()); 
    }

    /* Interrupt thread during run, verify that interrupt flag is not set when thread stops running. 
     * isInterrupted becomes false after InterruptedException is thrown. */
    private volatile boolean bool_interruptDuringRun = false;
    @Test
    public void test_interruptDuringRun() throws Throwable {
        String name = "interruptDuringRun: ";
        Thread t = new Thread() {
            public void run() {
                synchronized(this) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                        /* expected */
                        bool_interruptDuringRun = true;
                    }
                }
            }
        };

        /* run thread */
        t.start();
        AssertJUnit.assertFalse(name + "thread not yet interrupted during run", t.isInterrupted());
        t.interrupt();
        t.join();

        AssertJUnit.assertTrue(name + "InterruptedException was thrown during run", bool_interruptDuringRun);

        /* Verify thread is dead and has not been interrupted */
        AssertJUnit.assertFalse(name + "thread should be dead", t.isAlive());
        AssertJUnit.assertFalse(name + "interrupt flag should not be set", t.isInterrupted()); 
    }

    /* Interrupt thread at start and during run, verify that interrupt flag is not set when thread stops running.
     * isInterrupted becomes false after InterruptedException is thrown. */
    private volatile boolean bool_interruptBeforeAndDuringRun = false;
    @Test
    public void test_interruptBeforeAndDuringRun() throws Throwable {
        String name = "interruptBeforeAndDuringRun: ";
        Thread t = new Thread() {
            public void run() {
                synchronized(this) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                        bool_interruptBeforeAndDuringRun = true;
                    }
                }
            }
        };

        /* Verify thread is dead and has not been interrupted */
        AssertJUnit.assertFalse(name + "thread should not be alive", t.isAlive());
        AssertJUnit.assertFalse(name + "interrupt flag should not be set", t.isInterrupted());

        /* run thread */
        t.start();
        t.interrupt();
        t.join();

        AssertJUnit.assertTrue(name + "InterruptedException was thrown during run", bool_interruptBeforeAndDuringRun);

        /* Verify thread is dead and has not been interrupted */
        AssertJUnit.assertFalse(name + "thread should be dead", t.isAlive());
        AssertJUnit.assertFalse(name + "interrupt flag should be cleared", t.isInterrupted()); 
    }

    /* If thread is interrupted before start, Thread.interrupted is set */
    @Test
    public void test_interruptedStart() throws Throwable {
        String name = "interruptedStart: ";
        Thread t = new Thread(){
            public void run() throws AssertionError {
                AssertJUnit.assertTrue(Thread.interrupted());
            }
          };

        /* interrupt before start */
        t.interrupt();

        t.start();
        t.join();

        AssertJUnit.assertFalse(name + "interrupt flag should be cleared", t.isInterrupted()); 
    }
}

