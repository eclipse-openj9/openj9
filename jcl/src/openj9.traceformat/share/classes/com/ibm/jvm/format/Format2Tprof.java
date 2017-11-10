/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.format;

/**
 * Trace Tool routine for generating profile format output 
 * 
 */

import java.io.*;
import java.util.*;

public class Format2Tprof {
	 final String Trc_JIT_Component_Name = "j9jit";
	 final int    Trc_JIT_Sampling_Id = 13;
	 final int    Trc_JIT_Sampling_Detail_Id = 14;
    final char   FS = '\t';   // field separator
    final String TraceEntryPrefix = "SamplingLog";  // common to both ids
    
    static int totalCount = 0;

    public class MethodEntry implements Comparable {
       public String name = null;
       public String offset = null;
       public int    count = 0;
       public         MethodEntry(String n, String o)  { name = n;  offset = o; }
       public         MethodEntry(String n)            { name = n; }
       /* offset won't be compared */
       public boolean equals(Object o)       { MethodEntry e = (MethodEntry) o; return name.equals(e.name);     }
       public int     compareTo(Object o)    { MethodEntry e = (MethodEntry) o; return name.compareTo(e.name);  }
    }

    public void run(String args[]) {
    	  if (args.length < 1 || args.length > 2) {
            System.err.println("Format2Tprof <trace format input> [tprof output]"); 
            return;
        }
        LinkedList sampleMethodTable = new LinkedList();
        if (!readAndFormat(args, sampleMethodTable)) {
           System.err.println("No method sampling event found in the trace.  No output will be written.");
           return;
        }
        Collections.sort(sampleMethodTable);        
        // System.err.println(sampleMethodTable);
        totalCount = sampleMethodTable.size();
        countSampleMethod(sampleMethodTable);  // dup method names will be removed
        Collections.sort(sampleMethodTable, new Comparator()
              {
                 public int compare(Object o1, Object o2) {
                    MethodEntry m1 = (MethodEntry) o1;
                    MethodEntry m2 = (MethodEntry) o2;
                    return m2.count - m1.count;   // in decending order
                  }
               }
            );
        try {
           PrintStream ps = System.out;  // default
           if (args.length == 2) 
              ps = new PrintStream(new FileOutputStream(args[1]));
           generateTprofOutput(ps, sampleMethodTable);
           if (args.length == 2)
              ps.close();
        }
        catch (FileNotFoundException e) {
      	  System.err.println("Error opening output file");
           e.printStackTrace();
        }
    }
    
    public static void main(String args[]) {
        Format2Tprof pf = new Format2Tprof();
        pf.run(args);
    }
    
    public void countSampleMethod(Collection s) {
       Iterator iter = s.iterator();
       MethodEntry pe = (MethodEntry) iter.next();
       pe.count++;
       while (iter.hasNext())
       {
          MethodEntry e = (MethodEntry) iter.next();
          if (pe.equals(e))
             iter.remove();
          else
             pe = e;
          pe.count++;
       }
    }

    public void generateTprofOutput(PrintStream out, Collection s) {
       Iterator iter = s.iterator();
       while (iter.hasNext())
       {
          MethodEntry e = (MethodEntry) iter.next();
          float percent = (float) (e.count * 100) / totalCount;
          String percentStr = (new Float(percent)).toString().substring(0,4) + "%";
          out.println(percentStr + " " + e.count + " " + e.name);
       }
    }

    public MethodEntry getJittedMethod(String msg) {
      int firstTabPos = msg.indexOf(FS);
      int secondTabPos = (firstTabPos != -1) ? msg.indexOf(FS, firstTabPos+1) : -1;
      if (firstTabPos == -1 || secondTabPos == -1)
         return null;
      String name = msg.substring(firstTabPos+1,secondTabPos-1);
      int thirdTabPos = msg.indexOf(FS, secondTabPos+1);
      if (thirdTabPos == -1)
         return new MethodEntry(name);
      int fourthTabPos = msg.indexOf(FS, thirdTabPos+1);
      if (fourthTabPos == -1)
         return new MethodEntry(name, msg.substring(thirdTabPos+1));
      return new MethodEntry(name, msg.substring(thirdTabPos+1,fourthTabPos-1));
    }

   public boolean readAndFormat(String args[], List methodTable) {
   	boolean rc = false;
      try {
      	BufferedReader in = new BufferedReader(new FileReader(args[0]));
         String line = null;
         while ((line = in.readLine()) != null) {
            int pos;        	  
            if ((pos = line.indexOf(TraceEntryPrefix)) != -1) {
               String formattedData = line.substring(pos);
               MethodEntry me = getJittedMethod(formattedData);
               if (me != null) {
               	 rc = true;
                   methodTable.add(me);  
               }            
             }
         }
         in.close();
      }
      catch (FileNotFoundException e) {
      	System.err.println("Error opening trace file");
         e.printStackTrace();
      }
      catch (IOException e) {
      	System.err.println("Error processing trace file");
      	e.printStackTrace();
      }
      return rc;
   }
}



