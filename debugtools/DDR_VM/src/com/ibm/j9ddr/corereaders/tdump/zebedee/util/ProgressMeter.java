/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

/**
 * This class is used to provide feedback on progress for long running (eg greater than one
 * second) tasks. 
 */

public class ProgressMeter {
    /** One of these per thread */
    private static ThreadLocal local = new ThreadLocal() {
        protected Object initialValue() {
            return new ProgressMeter();
        }
    };
    /** The message associated with the current phase. */
    public String message = "initializing";
    /** The amount of progress as a percentage. */
    public int percentage;

    public static void set(String message, int current, int max) {
        ProgressMeter meter = (ProgressMeter)local.get();
        meter.message = message;
        meter.percentage = (current * 100)/max;
    }

    public static void set(String message, int percentage) {
        ProgressMeter meter = (ProgressMeter)local.get();
        meter.message = message;
        meter.percentage = percentage;
    }

    public static void set(String message) {
        ProgressMeter meter = (ProgressMeter)local.get();
        meter.message = message;
    }

    public static void set(int percentage) {
        ProgressMeter meter = (ProgressMeter)local.get();
        meter.percentage = percentage;
    }

    public static ProgressMeter get() {
        return (ProgressMeter)local.get();
    }
}
