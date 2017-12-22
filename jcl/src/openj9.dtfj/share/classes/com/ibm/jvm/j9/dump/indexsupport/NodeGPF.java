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

import com.ibm.dtfj.image.j9.ImageProcess;

/**
 * @author jmdisher
 * 
 * Example:
 * <gpf failingThread="0x806a600" nativeFailingThread="0xb7e286c0">
J9Generic_Signal_Number=00000004 Signal_Number=0000000b Error_Value=00000000 Signal_Code=00000001
Handler1=B7DADB8F Handler2=B7D7B0C6 InaccessibleAddress=FFFFFFFB
EDI=083639B0 ESI=09010101 EAX=96C5879E EBX=082EACDC
ECX=00000000 EDX=00000000
EIP=96C587A1 ES=C010007B DS=C010007B ESP=BFAB3634
EFlags=00210246 CS=00000073 SS=0000007B EBP=BFAB3634
Module=/tmp/rtj_jextract/jre/bin/libgptest.so
Module_base_address=96C58000 Symbol=Java_VMBench_GPTests_GPTest_gpWrite
Symbol_address=96C5879E
</gpf>
 */
public class NodeGPF extends NodeAbstract
{
	private ImageProcess _process;
	
	public NodeGPF(ImageProcess process, Attributes attributes)
	{
		long faultingNativeID = _longFromString(attributes.getValue("nativeFailingThread"));
		process.setFaultingThreadID(faultingNativeID);
		_process = process;
	}

	public void stringWasParsed(String string)
	{
		int signalNumber = (int)_longByResolvingRawKey(string, "Signal_Number");
		if (0 == signalNumber) {
			// This is not great but there are various occasions when the gpf node only contains 
			// info on the generic signal.  To reduce the level of confusion we map the generic signal
			// number to a 'normalised' number at this point.
			signalNumber = resolveGenericSignal((int)_longByResolvingRawKey(string, "J9Generic_Signal"));
		}
		_process.setSignalNumber(signalNumber);
	}
	// These flag definitions came from j9port.h
	private final static int J9PORT_SIG_FLAG_SIGSEGV 	= 4;
	private final static int J9PORT_SIG_FLAG_SIGBUS 	= 8;
	private final static int J9PORT_SIG_FLAG_SIGILL 	= 16;
	private final static int J9PORT_SIG_FLAG_SIGFPE 	= 32;
	private final static int J9PORT_SIG_FLAG_SIGTRAP 	= 64;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED7 = 0x80;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED8 = 0x100;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED9 = 0x200;
	//private final static int J9PORT_SIG_FLAG_SIGALLSYNC 	= 124;
	private final static int J9PORT_SIG_FLAG_SIGQUIT 	= 0x400;
	private final static int J9PORT_SIG_FLAG_SIGABRT 	= 0x800;
	private final static int J9PORT_SIG_FLAG_SIGTERM 	= 0x1000;
	//private final static int J9PORT_SIG_FLAG_SIGRECONFIG 		= 0x2000;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED14 	= 0x4000;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED15 	= 0x8000;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED16 	= 0x10000;
	//private final static int J9PORT_SIG_FLAG_SIGRESERVED17 	= 0x20000;
	//private final static int J9PORT_SIG_FLAG_SIGALLASYNC 		= 0x3C00;
	private final static int J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO 	= 0x40020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO = 0x80020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW 	= 0x100020;
	
	private int resolveGenericSignal(int num) {
		if ((num & J9PORT_SIG_FLAG_SIGQUIT) != 0) 	return 3;
		if ((num & J9PORT_SIG_FLAG_SIGILL) != 0) 	return 4;
		if ((num & J9PORT_SIG_FLAG_SIGTRAP) != 0) 	return 5;
		if ((num & J9PORT_SIG_FLAG_SIGABRT) != 0) 	return 6;
		if ((num & J9PORT_SIG_FLAG_SIGFPE) != 0) {
			if (num == J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO) 		return 35;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO) 	return 36;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW) 	return 37;
			return 8;
		}
		if ((num & J9PORT_SIG_FLAG_SIGBUS) != 0) 	return 10;
		if ((num & J9PORT_SIG_FLAG_SIGSEGV) != 0) 	return 11;
		if ((num & J9PORT_SIG_FLAG_SIGTERM) != 0) 	return 15;
		return num;
	}
}
