/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

import java.util.*;

public class TraceGroup{

    private String name = null;
    private ArrayList tplist = null;
    private int count = 0;

    public TraceGroup(String name){  
        this.name = name;
        count = 0;
        tplist = new ArrayList();
    }

    public String getName(){
        return name;
    }

    public synchronized int addTPID(int tpid){
        tplist.add( new Integer(tpid) );
        return count++;
    }

    public int[] getTPIDS(){
        int tpids[] = new int[tplist.size()];
        Object ints[] = tplist.toArray();


        for (int i = 0; i < tplist.size(); i++) {
            tpids[i] = ((Integer)ints[i]).intValue();
        }

        return tpids;
    }

    public int getCount(){
        return count;
    }

}
