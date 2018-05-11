/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 *  Class to maintain a list of addresses and coverage extents and a
 * symbolic name - then given an input address will convert this into 
 * a string showing offset from the symbol. 
 */ 
public class Symbol {
	
	String symbolName;
	long symbolStart;
	long symbolEnd;  // -1 = unknown
	int symbolType;  // -1 = unknown
	int symbolBinding; // -1 = unknown
	
	protected static HashMap knownSymbolsByName;  
	protected static TreeMap symbolTree;
	
	final static int STB_UNKNOWN = -1;
	final static int STB_LOCAL = 0;
	final static int STB_GLOBAL = 1;
	final static int STB_WEAK = 2;
	
	   // values for st_info field
    static final int STT_NOTYPE = 0;
    static final int STT_OBJECT = 1;
    static final int STT_FUNC = 2;
    static final int STT_SECTION = 3;
    static final int STT_FILE = 4;
    static final int STT_COMMON = 5;
    static final int STT_TLS = 6;
    static final int STT_LOOS = 10;
    static final int STT_HIOS = 12;
    static final int STT_LOPROC = 13;
    static final int STT_HIPROC = 15;

	public Symbol(String name, long address, int extent, int type, int bind) {
		
		// check don't already have this name 
		
		 
		if (null == knownSymbolsByName) {
			knownSymbolsByName = new HashMap();
		}
		Object o = knownSymbolsByName.get(name);
		if (o == null ) {
		
			symbolName = name;
			symbolStart = address;
			symbolType = type;
			symbolBinding = bind;
			// we use -1 for symbols whose extent is unknown 
			if (extent != -1) {
				symbolEnd = address+extent;
			} else {
				symbolEnd = -1;
			}
		
			// Now update the Hash map of symbols
			 
			knownSymbolsByName.put(name,this);
			// ... and into the range tree
			if (null == symbolTree) {
				
				Comparator c = new Symbol.SymbolComparator();
				symbolTree = new TreeMap(c);
			}
			symbolTree.put(Long.valueOf(address),this);
			 
		}
		
	}
	
	public static Symbol getSymbol(String name) {
		Symbol retSym = null;
		
		retSym = (Symbol)knownSymbolsByName.get(name);
		
		return retSym;
		
	}
	
	public static String getSymbolForAddress(long address) {
		String retString = null;
		 
		SortedMap head = (SortedMap) symbolTree.headMap(Long.valueOf(address));
		
		// So now we look at bottom of tail and hopefully we might have a 
		// symbol covering this address.... 
		if (head != null && !head.isEmpty()) {
			 
			Symbol s = (Symbol) symbolTree.get(head.lastKey());
			if (s.symbolEnd == -1) {
				
			} else {
				if (address <= s.symbolEnd && address > s.symbolStart) {
					long diff = address - s.symbolStart;
					retString =  s.symbolName + "+0x" + Long.toHexString(diff); 
				}
			}
		}

		return retString;
	}
	
	static final class SymbolComparator implements Comparator {

		/* (non-Javadoc)
		 * @see java.util.Comparator#compare(java.lang.Object, java.lang.Object)
		 */
		public int compare(Object arg0, Object arg1) {
			 
			long addr0=0;
			long addr1=0;
			// arg0 and arg1 will be Symbol objects 
			if (arg0 instanceof Symbol) {
				Symbol S0 = (Symbol) arg0;
				Symbol S1 = (Symbol) arg1;
				addr0 = S0.symbolStart;
				addr1 = S1.symbolStart;
			} else {
				addr0 = ((Long)arg0).longValue();
				addr1 = ((Long)arg1).longValue();
			}
			
			// both +ve
			if (addr0 >= 0 && addr1 >=0) {
				if (addr0 == addr1) return 0;
				if (addr0 < addr1) return -1;
				return 1;
			}
			
			// both -ve 
			if (addr0 < 0 && addr1 < 0) {
				if (addr0 == addr1) return 0;
				if (addr0 < addr1) return 1;
				return -1;
			}
			
			if (addr0 < 0 && addr1 >=0) {
				return 1;
			}
			
			return -1;
		}
		
	}

	/**
	 * @return Returns the symbolEnd.
	 */
	public long getSymbolEnd() {
		return symbolEnd;
	}
	/**
	 * @return Returns the symbolName.
	 */
	public String getSymbolName() {
		return symbolName;
	}
	/**
	 * @return Returns the symbolStart.
	 */
	public long getSymbolStart() {
		return symbolStart;
	}
	
	public static Iterator getSymbolsIterator() {
		return symbolTree.keySet().iterator();
		//return knownSymbolsByName.keySet().iterator();
		 
	}
	
	public static Symbol getSymbolUsingValue(Long l) {
		Symbol s = (Symbol) symbolTree.get(l);
		return s;
	}
	 
}
