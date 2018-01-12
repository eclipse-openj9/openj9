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
package com.ibm.jvm.j9.dump.indexsupport;

import java.util.Vector;

import org.xml.sax.Attributes;

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.j9.JavaHeap;
import com.ibm.dtfj.java.j9.JavaHeapRegion;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 *
 */
public class NodeHeap extends NodeAbstract
{
	private JavaRuntime _javaVM;
	private Vector _completeRegions = new Vector();
	private JavaHeap _heap;
	private JavaHeapRegion _legacyRegion;
	private long _arrayletLeafSize;
	
	public NodeHeap(JavaRuntime runtime, Attributes attributes)
	{
		// OLD: <heap name="object heap" objectAlignment="8" minimumObjectSize="16" start="0x97925000" end="0x97d25000" >
		String name = attributes.getValue("name");
		//note that the start, end, objectAlignment, and minimumObjectSize strings were used in the old format but in the new format they are part of the region
		// NEW:  <heap name="Heap" id="0x0" arrayletIdOffset="4" arrayletIdWidth="4" arrayletIdMask="0x1" arrayletIdResult="1" >

		//hold on to the JavaVM instance since we need it to assemble the JavaHeapRegions
		_javaVM = runtime;
		//assume that we are looking at the old format until we notice an omission which absolutely required in the old format (alignment and minimum size)
		boolean isOldFormat = true;
		
		String idString = attributes.getValue("id");
		String startString = attributes.getValue("start");
		String endString = attributes.getValue("end");
		String alignmentString = attributes.getValue("objectAlignment");
		String minimumObjectSizeString = attributes.getValue("minimumObjectSize");
		if ((null == alignmentString) || (null == minimumObjectSizeString)) {
			isOldFormat = false;
		}
		long id = _longFromString(idString);
		long start = _longFromString(startString);
		long end = _longFromString(endString);
		int alignment = (int) _longFromString(alignmentString);
		int minimumObjectSize = (int) _longFromString(minimumObjectSizeString);

		String arrayletLeafSizeString = attributes.getValue("arrayletLeafSize");
		_arrayletLeafSize = _longFromString(arrayletLeafSizeString);

		String arrayletIdOffsetString = attributes.getValue("arrayletIdOffset");
		int arrayletIdOffset = (int) _longFromString(arrayletIdOffsetString);
		String arrayletIdWidthString = attributes.getValue("arrayletIdWidth");
		int arrayletIdWidth = (int) _longFromString(arrayletIdWidthString);
		String arrayletIdMaskString = attributes.getValue("arrayletIdMask");
		long arrayletIdMask = _longFromString(arrayletIdMaskString);

		String arrayletIdResultString = attributes.getValue("arrayletIdResult");
		long arrayletIdResult = _longFromString(arrayletIdResultString);
		
		int fobjectSize = (int)_longFromString(attributes.getValue("fobjectSize"), runtime.bytesPerPointer());
		int fobjectPointerScale = (int)_longFromString(attributes.getValue("fobjectPointerScale"), 1);
		long fobjectPointerDisplacement = _longFromString(attributes.getValue("fobjectPointerDisplacement"));
		int classOffset = (int)_longFromString(attributes.getValue("classOffset"));
		int classSize = (int)_longFromString(attributes.getValue("classPointerSize"), runtime.bytesPerPointer());

		long classAlignment = _longFromString(attributes.getValue("classAlignment"), 0);
		boolean isSWH =  _longFromString(attributes.getValue("SWH"), 0) >= 1;
		
		_heap = new JavaHeap(runtime, name, runtime.pointerInAddressSpace(id), runtime.pointerInAddressSpace(start), (end - start), arrayletIdOffset, arrayletIdWidth, arrayletIdMask, arrayletIdResult, fobjectSize, fobjectPointerScale, fobjectPointerDisplacement, classOffset, classSize, classAlignment, isSWH);
		_javaVM.addHeap(_heap);
		
		if (isOldFormat) {
			//the legacy format also includes the start and end of the heap (which we need in order to do anything useful here)
			String heapBaseString = attributes.getValue("start");
			long heapStart = _longFromString(heapBaseString);
			String heapEndString = attributes.getValue("end");
			long heapEnd = _longFromString(heapEndString);
			
			_legacyRegion = createJavaHeapRegion(name, id, alignment, minimumObjectSize, heapStart, heapEnd);
			_completeRegions.add(_legacyRegion);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("region")) {
			//note that the legacy region must be null if this has a new world XML shape
			if (null != _legacyRegion) {
				throw new IllegalStateException("Legacy heap region must be null for new world XML shape");
			}
			child = new NodeRegion(this, attributes);
		} else if (qName.equals("objects")) {
			//note that the legacy region must be non-null if this has a legacy XML shape
			if (null == _legacyRegion) {
				throw new IllegalStateException("Legacy heap region cannot be null for legacy XML shape");
			}
			child = new NodeObjects(_legacyRegion, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.NodeAbstract#didFinishParsing()
	 */
	public void didFinishParsing()
	{
		//we collect the regions under the heap in this object so we need to hook in here to write back before we are done
		_heap.setRegions(_completeRegions);
	}
	
	public JavaHeapRegion createJavaHeapRegion(String name, long id, int objectAlignment, int minimumObjectSize, long heapStart, long heapEnd)
	{
		ImagePointer basePointer = null;
		if (0 != heapStart) {
			basePointer = _javaVM.pointerInAddressSpace(heapStart);
		}
		return new JavaHeapRegion(_javaVM, name, _javaVM.pointerInAddressSpace(id), objectAlignment, minimumObjectSize, _arrayletLeafSize, _heap, basePointer, heapEnd - heapStart);
	}

	public void addRegion(JavaHeapRegion region)
	{
		_completeRegions.add(region);
	}
}
