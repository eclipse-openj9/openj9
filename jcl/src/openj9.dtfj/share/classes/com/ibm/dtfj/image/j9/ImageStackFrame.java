/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageModule;

public class ImageStackFrame implements com.ibm.dtfj.image.ImageStackFrame
{
	private ImageAddressSpace _space;
	private String _procedureName = null;
	private ImagePointer _procedureAddress;
	private ImagePointer _basePointer;

	public ImageStackFrame(ImageAddressSpace space, ImagePointer procedureAddress, ImagePointer basePointer) {
		_space = space;
		_procedureAddress = procedureAddress;
		_basePointer = basePointer;
	}

	public ImagePointer getProcedureAddress() throws CorruptDataException
	{
		return _procedureAddress;
	}

	public ImagePointer getBasePointer() throws CorruptDataException
	{
		return _basePointer;
	}


	public String getProcedureName() throws CorruptDataException
	{
		if (null == _procedureName) {
			ImageModule bestModule = null;
			ImageSymbol bestClosest = null;
			long bestDelta = Long.MAX_VALUE;

			for (Iterator i = this._space.getProcesses(); i.hasNext(); ) {
				Object nexti = i.next();
				if (nexti instanceof CorruptData) continue;
				ImageProcess process = (ImageProcess)nexti;
				if ((_procedureAddress.getAddress() != ((1L << process.getPointerSize()) - 1))) {
					try {
						ImageModule module = process.getExecutable();
						ImageSymbol closest = getClosestSymbolFrom(module);
						long delta = getDelta(module, closest);
						if (delta >= 0 && delta < bestDelta) {
							bestModule = module;
							bestClosest = closest;
							bestDelta = delta;
						}
					} catch (DataUnavailable e) {
					} catch (CorruptDataException e) {					
					}
					try {
						for (Iterator j = process.getLibraries(); j.hasNext(); ) {
							Object next = j.next();
							if (next instanceof CorruptData) continue;
							ImageModule module = (ImageModule)next;
							ImageSymbol closest = getClosestSymbolFrom(module);
							long delta = getDelta(module, closest);
							if (delta >= 0 && delta < bestDelta) {
								bestModule = module;
								bestClosest = closest;
								bestDelta = delta;
							}
						}
					} catch (DataUnavailable e) {
					} catch (CorruptDataException e) {					
					}
				}
			}

			ImageSymbol closest = bestClosest;
			ImageModule module = bestModule;
			long delta = bestDelta;
			if (null != module) {

				String deltaString = "";
				if (delta == Long.MAX_VALUE) {
					deltaString = "<offset not available>";
				} else {
					if (delta >= 0) {
						deltaString = "+0x" + Long.toHexString(delta);
					} else {
						deltaString = "-0x" + Long.toHexString(delta);
					}
				}
				if (null != closest) {
					_procedureName = module.getName() + "::" + closest.getName() + deltaString;
				} else {
					_procedureName = module.getName() + deltaString;
				}
			}

		}

		// If we haven't found the symbol, we might as well record that fact.
		if (null == _procedureName) {
			_procedureName = "<unknown location>";
		}
		return _procedureName;
	}

	/**
	 * @param module
	 * @param closest
	 * @return
	 * @throws CorruptDataException
	 */
	private long getDelta(ImageModule module, ImageSymbol closest)
			throws CorruptDataException {
		long delta = 0;
		if (closest != null) {
			// the module has symbols
			delta = getProcedureAddress().getAddress() - closest.getAddress().getAddress();
		} else {
			// the module doesn't have any symbol
			try {
				//as soon as getLoadAddress will be exposed by the ImageModule interface, the following casts can be removed
				delta = getProcedureAddress().getAddress() - ((com.ibm.dtfj.image.j9.ImageModule)module).getLoadAddress();
			} catch (DataUnavailable e) {
				delta = Long.MAX_VALUE;
			}
		}
		return delta;
	}
	
	
	private ImageSymbol getClosestSymbolFrom(ImageModule module) {
		long pc = _procedureAddress.getAddress();
		long maxDifference = Long.MAX_VALUE;
		ImageSymbol closestSymbol = null;
		for (Iterator iter = module.getSymbols(); iter.hasNext();) {
			Object next = iter.next();
			if (next instanceof CorruptData)
				continue;
			ImageSymbol currentSymbol = (ImageSymbol) next;
			long symbolAddress = currentSymbol.getAddress().getAddress();
			if (symbolAddress <= pc && pc - symbolAddress < maxDifference) {
				maxDifference = pc - symbolAddress;
				closestSymbol = currentSymbol;
			}
		}
		return closestSymbol;
	}



// riccole commented out as not used (let me know if you need this code)
//	/*
//	 * Returns the ImageModule pointed to by this ImageStackFrame.
//	 * The returned module is the one for which the following conditions hold:
//	 * 1. the module's load address is less than or equal to this frame's procedureAddress;
//	 * 2. the difference between this frame's procedureAddress and the module's load address is minimal.
//	 * If, for any module, the load address is not available, then the difference between this frame's procedureAddress and the module's 
//	 * first symbol address is taken into account, but this strategy is used as a backup, since it's expensive.
//	 * At the moment, the only platform for which we're not able to determine the load address of a module (actually, of any module) 
//	 * is z/OS, but on z/OS we don't really need it, since the stack frame's procedureName is built using DsaStackFrames.  
//	 * 
//	 * Returns null if  no module has a load address AND no module has any symbol.
//	 */
//	private ImageModule getModule() {
//		ImageProcess process = _space.getCurrentProcess();
//		long pc = _procedureAddress.getAddress();
//		long delta = Long.MAX_VALUE; 
//		ImageModule module = null;
//		if ((null != process) && (pc != ((1L << process.getPointerSize()) - 1))) {
//			try {
//				//as soon as getLoadAddress will be exposed by the ImageModule interface, the following casts can be removed
//				com.ibm.dtfj.image.j9.ImageModule executable = (com.ibm.dtfj.image.j9.ImageModule) process.getExecutable();
//				try {
//					if (executable.getLoadAddress() < pc && pc-executable.getLoadAddress()<delta) {
//						delta = pc-executable.getLoadAddress();
//						module = executable;
//					}
//				} catch (DataUnavailable e) {
//					//if a module's load address is not available, then use the module's first symbol address
//					long firstSymbolAddress = getFirstSymbolAddressFrom(executable);
//					if (firstSymbolAddress < pc && pc-firstSymbolAddress<delta) {
//						delta = pc-firstSymbolAddress;
//						module = executable;
//					}
//					
//				}
//			} catch (DataUnavailable e) {
//				// Ignore
//			} catch (CorruptDataException e) {
//
//			}
//			
//			try {
//				for (Iterator iter = process.getLibraries(); iter.hasNext();) {
//					Object candidate = iter.next();
//					//this might be corrupt data so do this check, first
//					if (candidate instanceof com.ibm.dtfj.image.j9.ImageModule) {
//						//as soon as getLoadAddress will be exposed by the ImageModule interface, the following casts can be removed
//						com.ibm.dtfj.image.j9.ImageModule library = (com.ibm.dtfj.image.j9.ImageModule) candidate;
//						try {
//							if (library.getLoadAddress() < pc && pc-library.getLoadAddress()<delta) {
//								delta = pc-library.getLoadAddress();
//								module = library;
//							}
//						} catch (DataUnavailable e) {
//							//if a module's load address is not available, then use the module's first symbol address
//							long firstSymbolAddress=getFirstSymbolAddressFrom(library);
//							if (firstSymbolAddress < pc && pc-firstSymbolAddress<delta) {
//								delta = pc-firstSymbolAddress;
//								module = library;
//							}
//						}
//					}
//				}
//			} catch (DataUnavailable e) {
//				// Ignore
//			} catch (CorruptDataException e) {
//
//			}
//		}
//		return module;
//	}
	
//	riccole commented out as not used (let me know if you need this code)
//	private long getFirstSymbolAddressFrom(ImageModule module) {
//		long firstSymbolAddress = Long.MAX_VALUE;
//		for (Iterator iter = module.getSymbols(); iter.hasNext();) {
//			Object next = iter.next();
//			if (next instanceof CorruptData)
//				continue;
//			ImageSymbol currentSymbol = (ImageSymbol) next;
//			long symbolAddress = currentSymbol.getAddress().getAddress();
//			if (symbolAddress < firstSymbolAddress) {
//				firstSymbolAddress=symbolAddress;
//			}
//		}
//		return firstSymbolAddress;
//	}
	
}
