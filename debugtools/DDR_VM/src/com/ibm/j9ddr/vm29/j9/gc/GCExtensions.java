/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCExtensions {
	
	private static MM_GCExtensionsPointer gcExtensions;
	private static boolean isSegregatedHeap;
	private static boolean isMetronomeGC;
	private static boolean isStandardGC;
	private static boolean isVLHGC;
	private static boolean scavengerEnabled;
	private static UDATA softMx;
	
	/* TODO: lpnguyen, version this */
	static 
	{
		try {
			gcExtensions = GCBase.getExtensions();
						
			if(J9BuildFlags.gc_combinationSpec) {
				isSegregatedHeap = gcExtensions._isSegregatedHeap();
				isMetronomeGC = gcExtensions._isMetronomeGC();
				isStandardGC = gcExtensions._isStandardGC();
				isVLHGC = gcExtensions._isVLHGC();
			} else {
				isSegregatedHeap = J9BuildFlags.gc_segregatedHeap;
				isMetronomeGC = J9BuildFlags.gc_realtime;
				isStandardGC = J9BuildFlags.gc_modronStandard;
				isVLHGC = J9BuildFlags.gc_vlhgc;
			}
			
			if (J9BuildFlags.gc_modronScavenger || J9BuildFlags.gc_vlhgc) {
				scavengerEnabled = gcExtensions.scavengerEnabled();
			} else {
				scavengerEnabled = false;
			}
			softMx = gcExtensions.softMx();
			
		} catch (CorruptDataException cde) {
			raiseCorruptDataEvent("Error initializing GCExtensions", cde, true);
			gcExtensions = null;
		}
	}
	
	public static boolean isSegregatedHeap()
	{
		return isSegregatedHeap;
	}

	public static boolean isMetronomeGC()
	{
		return isMetronomeGC;
	}
	
	public static boolean isStandardGC()
	{
		return isStandardGC;
	}
	
	public static boolean isVLHGC()
	{
		return isVLHGC;
	}

	public static boolean scavengerEnabled()
	{
		return scavengerEnabled;
	}
	
	public static UDATA softMx()
	{
		return softMx;
	}

	public static MM_GCExtensionsPointer getGCExtensionsPointer()
	{
		return gcExtensions;
	}
}
