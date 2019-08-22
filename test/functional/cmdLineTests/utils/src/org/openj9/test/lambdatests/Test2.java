/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.lambdatests;

import java.util.Comparator;

/* The function func() is being called from Test1.java to test whether lambda classes 
 * still get stored correctly and are used correctly or not when another file is involved.
 */

public class Test2 {
        
    public void func() {
        Comparator<String> strComparator = (String o1, String o2) -> o1.compareTo(o2);
        int strComparison = strComparator.compare("coffee", "cup");
        System.out.println(strComparison);
            
        Comparator<String> strComparator2 = (String o1, String o2) -> o2.compareTo(o1) + 1523;
        int strComparison2 = strComparator2.compare("a_cup_of", "tea");
        System.out.println(strComparison2);
    }
}

