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

import org.xml.sax.Attributes;

import com.ibm.dtfj.image.j9.Image;
import com.ibm.dtfj.image.j9.ImageAddressSpace;
import com.ibm.dtfj.image.j9.ImageProcess;

/**
 * @author jmdisher
 * 
 * Example:
 * <j9dump endian="little" size="32" version="2.3" stamp="20060714_07189_lHdSMR" uuid="16994021750723430015" format="1.1" arch="x86" cpus="2" memory="1593016320" osname="Linux" osversion="2.6.15-1-686-smp" environ="0x80569c0">
 */
public class NodeJ9Dump extends NodeAbstract
{
	private XMLIndexReader _parent;
	private ImageAddressSpace _addressSpace;
	private String _vmVersion;
	private ImageProcess _process;
	private Image _image;
	
	public NodeJ9Dump(XMLIndexReader reader, Attributes attributes)
	{
		//<j9dump endian="little" size="32" version="2.3" stamp="20050823_02904_lHdSMR" 
		//uuid="16827002721828718167" format="1.1" arch="x86" cpus="1" memory="536330240" 
		//osname="Windows XP" osversion="5.1 build 2600 Service Pack 1" environ="0x7c38c8f4">
		
		String osType = attributes.getValue("osname");
		String osSubType = attributes.getValue("osversion");
		String cpuType = attributes.getValue("arch");
		_vmVersion = attributes.getValue("version");
		long environ = _longFromString(attributes.getValue("environ"));
		//Jazz 4961 : chamlain : NumberFormatException opening corrupt dump
		String cpus = attributes.getValue("cpus");
		int cpuCount = cpus != null ? Integer.parseInt(cpus) : 0;
		String memory = attributes.getValue("memory");
		long bytesMem = memory != null ? Long.parseLong(memory) : 0;
		int pointerSize = Integer.parseInt(attributes.getValue("size"));
		Image[] iRef = new Image[1];
		ImageAddressSpace[] asRef = new ImageAddressSpace[1];
		ImageProcess[] pRef = new ImageProcess[1];
		_parent = reader;
		_parent.setJ9DumpData(environ, osType, osSubType, cpuType, cpuCount, bytesMem, pointerSize, iRef, asRef, pRef);
		_image = iRef[0];
		_addressSpace = asRef[0];
		_process = pRef[0];
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("gpf")) {
			child = new NodeGPF(_process, attributes);
		} else if (qName.equals("net")){
			child = new NodeNet(_image, attributes);
		} else if (qName.equals("javavm")){
			child = new NodeJavaVM(_parent, _process, _addressSpace, _vmVersion, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
}
