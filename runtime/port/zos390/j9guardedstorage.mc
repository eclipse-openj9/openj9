/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
/* 
 * This file is used to generate the HLASM corresponding to the C calls 
 * that use the IEAGSF macro for guarded storage.
 * 
 * This file is compiled manually using the METAL-C compiler that was 
 * introduced in z/OS V1R9. The generated output (j9gs_authorize.s) is then
 * inserted into j9gs_authorize.s which is compiled by our makefiles.
 * 
 * j9gs_authorize.s indicates where to put the contents of j9gs_authorize.s.
 * Search for:
 * 		Insert contents of j9gs_authorize.s below
 * 
 * 
 * It should be obvious, however, just to be clear be sure to omit the 
 * first two lines from j9gs_authorize.s which will look something like:
 *  
 *          TITLE '5650ZOS V2.1 z/OS XL C
 *                     ./j9guardedstorage.c'
 *
 *  
 * To compile:
 * 31: xlc -S  -qnosearch  -I /usr/include/metal/ -qmetal -qlongname j9guardedstorage.c
 * 64: xlc -S  -qnosearch  -I /usr/include/metal/ -qmetal -Wc,lp64 -qlongname j9guardedstorage.c
 * 
 * z/OS V1R12 z/OS V1R12 Metal C Programming Guide and Reference: 
 *   http://publibz.boulder.ibm.com/epubs/pdf/ccrug120.pdf
 * 
 * Specifically, page 24 has the sample macros for re-entrant code
 * that is used below (wAtuh and lAuth) stuff.
 */
#include <stdint.h>

__asm(" IEAGSF PLISTVER=MAX,MF=(L,SPARAM)": "DS"(sParam));

#pragma prolog(enable_guarded_storage_facility,"MYPROLOG")
#pragma epilog(enable_guarded_storage_facility,"MYEPILOG")

uintptr_t enable_guarded_storage_facility(void * init_controls) {
	uintptr_t ieagsf_retcode = 0;
	
	__asm(" IEAGSF PLISTVER=MAX,MF=(L,SPARAM)": "DS"(wEnable));
	wEnable = sParam;

	__asm(" IEAGSF START,INITIALCONTROLS=(%0),MF=(E,(%1)),"\
			"RETCODE=(%2)"\
			::"r"(init_controls),"r"(&wEnable),"r"(ieagsf_retcode));


	return ieagsf_retcode;
}


#pragma prolog(disable_guarded_storage_facility,"MYPROLOG")
#pragma epilog(disable_guarded_storage_facility,"MYEPILOG")


uintptr_t disable_guarded_storage_facility() {

	uintptr_t ieagsf_retcode = 0;

	__asm(" IEAGSF STOP,RETCODE=(%0)"\
			::"r"(ieagsf_retcode));

	return ieagsf_retcode;

}



