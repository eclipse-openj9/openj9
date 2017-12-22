/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;
import java.util.WeakHashMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;

/**
 * Static utility functions for symbols
 * 
 * @author andhall
 *
 */
public class SymbolUtil
{
	private static WeakHashMap<IModule, List<ISymbol>> extraSymbolsMap = new WeakHashMap<IModule, List<ISymbol>>();

	/**
	 * Formats an address a string that shows the containing module and offset from the nearest symbol in that module.
	 * If dtfjFormat is true then symbols taken from internal pointer tables are formatted with fewer internal details.
	 * Otherwise those symbols are formatted in a form that includes the !<structure> command needed to format the
	 * whole pointer table the symbol came from.
	 * 
	 * @param process - The process to look the symbol up in.
	 * @param address -  The address to lookup.
	 * @param dtfjFormat - Format symbols for dtfj, not ddr.
	 * @return
	 * @throws DataUnavailableException
	 * @throws CorruptDataException
	 */
	protected static String getProcedureNameForAddress(IProcess process, long address, boolean dtfjFormat) throws DataUnavailableException, CorruptDataException
	{
		String name = getNameExactMatch(process, address, dtfjFormat);
		if(name == null) {
			name = getNameBestMatch(process, address, dtfjFormat);
		}
		if(name == null) {
			return "<unknown location>";
		} else {
			return name;
		}
	}

	private static String getNameExactMatch(IProcess process, long address, boolean dtfjFormat)  throws DataUnavailableException, CorruptDataException {
		//Find the module
		Collection<? extends IModule> modules = process.getModules();
		
		IModule matchingModule = null;
		
OUTER_LOOP:	for (IModule thisModule : modules) {
			for (IMemoryRange thisRange : thisModule.getMemoryRanges()) {
				if (thisRange.contains(address)) {
					matchingModule = thisModule;
					break OUTER_LOOP;
				}
			}
		}
		
		if (matchingModule == null) {
			return null;
		}
		
		//Find the symbol
		List<ISymbol> symbols = getSymbols(matchingModule);
		
		ISymbol bestMatch = findClosestSymbol(address, symbols);
		
		if (bestMatch != null) {
			long delta = address - bestMatch.getAddress();
			// On Linux some modules seem to include the entire process in their ranges.
			// Assume that a match to a symbol further than 1Mb away probably isn't an *exact* match
			// to prevent this.
			// (This match may be re-found by getNameBestMatch later, in which case it could still be used.)
			if( delta < 1024L*1024L ) {
				return matchingModule.getName() + "::" + getSymbolName(bestMatch, dtfjFormat) + "+0x" + Long.toHexString(delta);
			} else {
				return null;
			}
		} else {
			long delta = getDelta(address, matchingModule, null);
			return matchingModule.getName() + "+0x" + Long.toHexString(delta);
		}
	}
	
	private static String getNameBestMatch(IProcess process, long address, boolean dtfjFormat) {
		IModule bestModule = null;
		ISymbol bestClosest = null;
		long bestDelta = Long.MAX_VALUE;
		String moduleName = null;
		try {
			IModule module = process.getExecutable();
			List<ISymbol> symbols = new ArrayList<ISymbol>();
			if( module != null ) {
				symbols.addAll(getSymbols(module));
			}
			ISymbol closest = findClosestSymbol(address, symbols);
			long delta = getDelta(address, module, closest);
			if (delta >= 0 && delta < bestDelta) {
				bestModule = module;
				moduleName = module.getName();
				bestClosest = closest;
				bestDelta = delta;
			}
		} catch (CorruptDataException e) {
		} catch (DataUnavailableException e) {
		}
		try {
			for (IModule module : process.getModules()) {
				List<ISymbol> symbols = new ArrayList<ISymbol>(getSymbols(module));
				ISymbol closest = findClosestSymbol(address, symbols);
				long delta = getDelta(address, module, closest);
				if (delta >= 0 && delta < bestDelta) {
					bestModule = module;
					moduleName = module.getName();
					bestClosest = closest;
					bestDelta = delta;
				}
			}
		} catch (DataUnavailableException e) {
		} catch (CorruptDataException e) {					
		}
		if (null != bestModule) {

			String deltaString = "";
			if (bestDelta == Long.MAX_VALUE) {
				deltaString = "<offset not available>";
			} else {
				if (bestDelta >= 0) {
					deltaString = "+0x" + Long.toHexString(bestDelta);
				} else {
					deltaString = "-0x" + Long.toHexString(bestDelta);
				}
			}
			if (null != bestClosest) {
				return moduleName + "::" + getSymbolName(bestClosest, dtfjFormat) + deltaString;
			} else {
				return moduleName + deltaString;
			}
		}
		return null;
	}
	
	private static long getDelta(long address, IModule module, ISymbol closest) throws CorruptDataException {
		long delta = 0;
		if (closest != null) {	// the module has symbols
			delta = address - closest.getAddress();
		} else {
			// the module doesn't have any symbol
			delta = address - module.getLoadAddress();
		}
		return delta;
	}
	
	private static ISymbol findClosestSymbol(long address, List<? extends ISymbol> symbols)
	{
		Collections.sort(symbols, new SymbolComparator());
		
		//Need to find the highest symbol that is less than our address
		//TODO think about implementing this as a binary chop
		ISymbol bestMatch = null;
		for (ISymbol symbol : symbols) {
			if (Addresses.lessThanOrEqual(symbol.getAddress(),address)) {
				if((bestMatch ==  null) || (bestMatch.getAddress() != symbol.getAddress())) {
					bestMatch = symbol;
				}
			} else {
				break;
			}
		}
		return bestMatch;
	}
	
	private static final class SymbolComparator implements Comparator<ISymbol> {
		public int compare(ISymbol arg0, ISymbol arg1)
		{
			if( arg0.getAddress() > arg1.getAddress() ) {
				return 1;
			} else if( arg0.getAddress() < arg1.getAddress() ) {
				return -1;
			}
			return 0;
		}
	}

	/* Get the symbols for a module and merge them with any extra ones that may have been
	 * added.
	 */
	private static List<ISymbol> getSymbols(IModule module)
			throws DataUnavailableException {
		
		if( !extraSymbolsMap.containsKey(module)) {
			return new ArrayList<ISymbol>(module.getSymbols());
		}
		
		Collection<? extends ISymbol> moduleSymbols = module.getSymbols();
		List<ISymbol> extraSymbols = extraSymbolsMap.get(module);
		
		List<ISymbol> allSymbols = new ArrayList<ISymbol>(moduleSymbols.size() + extraSymbols.size());
		
		// Merge the symbol lists.
		allSymbols.addAll(moduleSymbols);
		allSymbols.addAll(extraSymbols);
		Collections.sort(allSymbols, new SymbolComparator());
		
		return allSymbols;
	}
	
	private static String getSymbolName(ISymbol symbol, boolean dtfjFormat) {
		if( !dtfjFormat && symbol instanceof DDRSymbol) {
			return ((DDRSymbol)symbol).getddrName();
		} else {
			return symbol.getName();
		}
	}
	
	private static class DDRSymbol extends Symbol {
		
		private String ddrName;

		public DDRSymbol(String dtfjName, String ddrName, long address) {
			super(dtfjName, address);
			this.ddrName = ddrName;
		}

		public String getddrName() {
			return ddrName;
		}
		
	}
	
	/**
	 * Provides an interface to add symbols (or address to name mappings) that are found
	 * by routes other than reading from the modules themselves.
	 * 
	 * @param module - The module to add to.
	 * @param symbol - The symbol to add.
	 */
	public static void addDDRSymbolToModule(IModule module, String dtfjName, String ddrName, long address) {
		
		List<ISymbol> currentSymbols = extraSymbolsMap.get(module);
		ISymbol symbol = new DDRSymbol(dtfjName, ddrName, address);
		if( currentSymbols == null ) {
			currentSymbols = new LinkedList<ISymbol>();
			currentSymbols.add(symbol);
			extraSymbolsMap.put(module, currentSymbols);
		} else {
			currentSymbols = extraSymbolsMap.get(module);
			currentSymbols.add(symbol);
		}
		
		// Sort the symbols for faster lookups.
		Collections.sort(currentSymbols, new SymbolComparator());
	}
}
