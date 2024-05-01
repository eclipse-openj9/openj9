/*
 * Copyright IBM Corp. and others 2022
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
package org.openj9.test.fips;

import java.security.NoSuchProviderException;
import java.security.Provider;
import java.security.Security;
import java.util.Arrays;
import java.util.Set;
import java.util.HashSet;

public class FIPSProvidersTest {

    public static void main(String args[]) throws Exception {
        System.out.println("Verify providers");

        // Make sure there are 4 providers available when FIPS mode is enabled.
        Provider[] providers = Security.getProviders();

        /*
         * There should be four providers when FIPS mode is enabled.
         * SunPKCS11-NSS-FIPS
         * SUN
         * SunEC
         * SunJSSE
         */

        // remove duplicate
        Set<Provider> set = new HashSet<>();
        for(Provider p : providers) {
            set.add(p);
        }

        assertEquals("Security provider list length", set.size(), 4);

        String[] fipsProvidersName = {"SunPKCS11-NSS-FIPS", "SUN", "SunEC", "SunJSSE"};

        for(String fpn : fipsProvidersName) {
            Provider provider = Security.getProvider(fpn);
            if(provider == null || !set.contains(provider)) {
                throw new NoSuchProviderException("Could not find provider " + fpn);
            }
        }
        System.out.println("Verify providers in FIPS mode COMPLETED");
    }

    private static void assertEquals(String name, int value, int expected) {
        if (expected != value) {
            error(name + " = " + value + " (expected " + expected + ")");
        }
    }

    private static void error(String msg) {
        System.out.println("ERR: " + msg);
    }
}
