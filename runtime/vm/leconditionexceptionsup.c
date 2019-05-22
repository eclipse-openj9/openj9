/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9port.h"

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
#include "leconditionhandler.h"
#include "leconditionexceptionsup.h"
#include "util_api.h"
#include "atoe.h"
#include "omrceetbck.h"

#define MAX_NAME	256

/* Create something that can be used if there is a problem getting the feedback structure
 * This must be as large as the AMODE64 _FEEDBACK structure, which is 16 bytes */
struct {
	U_32 dummyOne;
	U_32 dummyTwo;
	U_32 dummyThree;
	U_32 dummyFour;
} dummy_feedback = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

/*
 * Walk the C stack until we reach the builder callin frame corresponding to the DSA in builderDSA.
 * Reach into the frame that the builder callin frame called out to and grab its return address.
 *
 * Note that DSA is z/OS speak for sp.
 *
 * @param builderDSA	the DSA of the builder callin frame that called out to the JNI native.
 *
 * @return	-1 on error, otherwise the the targetAddress
 * 
 * @see "z/OS Language Environment Vendor Interfaces" for documentation on __dsa_prev().
 *
 */
long
getReturnAddressOfJNINative(void *builderDSA) {

	void *dsaptr;
	void *callee_dsaptr = NULL;
	_INT4 dsa_format, callers_dsa_format, callee_dsa_format;
	long DSASaveArea;
	long targetAddress = -1;

	/* Input parameters to CEETBCK */
	void *caaptr = (void *)_gtca();
	void *ceetbckDSAPtr;
	_INT4 ceetbckDSAFormat;

	/* Output parameters to CEETBCK */
	_INT4 member_id;
	_INT4 program_unit_name_length = MAX_NAME;
	char program_unit_name[MAX_NAME];
	_INT4 program_unit_address;
	_INT4 call_instruction_address = 0;
	_INT4 entryNameLength = MAX_NAME;
	_INT4 entry_address = 0;
	char entryName[MAX_NAME];
	_INT4 callers_call_instruction_address;
	void *callers_dsaptr;
	char statement_id[MAX_NAME];
	_INT4 statement_id_length = MAX_NAME;
	void *cibptr;
	_INT4 main_program;
	struct _FEEDBACK fc;

	long ceetbckTargetAddress;

	dsaptr = (void *)getdsa();

	/* walk all the DSAs until we get to the DSA for RUNCALLINMETHOD (the VM stack frame) */
	
	/* How do we know dsa_format of this frame is __EDCWCCWI_DOWN?
	 * 	- getReturnAddressOfJNINative is only called from within a condition handler
	 * 	- The condition handler uses the linkage of the routine that registered it.
	 * 	- We registered the handler from an XPLINK routine, so we know our dsa_format is XPLINK (__EDCWCCWI_DOWN) */ 
	dsa_format = __EDCWCCWI_DOWN;	

	while( (dsaptr != builderDSA)) {
		callee_dsaptr = dsaptr;
		callee_dsa_format = dsa_format;
		dsaptr = __dsa_prev(dsaptr, __EDCWCCWI_PHYSICAL , (int)dsa_format, NULL, NULL, (int*)&callers_dsa_format, NULL, NULL);
		dsa_format = callers_dsa_format;
	}

	/* 
	 * Use ceetbck() to find the address of the instruction that called out of RUNCALLINMETHOD
	 * 
	 * There appears to be a couple LE bugs in ceetbck (need to verify):
	 * 
	 * 		ceetbck(dsa_ptr) where dsa_ptr is set to dsa RUNCALLINMETHOD:
	 *
	 * 			Bug 1: 		call_instruction_address (of RUNCALLINMETHOD) is not correct if the callee is XPLINK and had used CEEHDLR
	 * 			Workaround: set dsa_ptr to callee (JNI native) and use the callers_call_instruction_address
	 *
	 * 		ceetbck(dsa_ptr) where dsa_ptr is set to dsa of CEEVRONU:
	 *
	 * 			Bug 2:		callers_call_instruction_address is not correct if we're in CEEVRONU
	 * 			Workaround:	set dsa_ptr to caller (RUNCALLINMETHOD) and use call_instruction_address
	 */
	if (__EDCWCCWI_DOWN == callee_dsa_format) {
		ceetbckDSAPtr = callee_dsaptr;	/* JNI native */
		ceetbckDSAFormat = callee_dsa_format;
	} else {
		ceetbckDSAPtr = dsaptr;	/* RUNCALLINMETHOD */
		ceetbckDSAFormat = dsa_format;
	}

	/* call CEETBCK */
	ceetbck(
		&ceetbckDSAPtr,
		&ceetbckDSAFormat,
		&caaptr,
		&member_id,
		program_unit_name,
		&program_unit_name_length,
		&program_unit_address,
		&call_instruction_address,
		entryName,
		&entryNameLength,
		&entry_address,
		&callers_call_instruction_address,
		&callers_dsaptr,
		&callers_dsa_format,
		statement_id,
		&statement_id_length,
		&cibptr,
		&main_program,
		&fc
	);

	if (0 != _FBCHECK(fc, CEE000)) {
		return -1;
	}

	/* See above for details of potential CEETBCK bugs */
	if (__EDCWCCWI_DOWN == callee_dsa_format) {
		ceetbckTargetAddress = (long)callers_call_instruction_address;
	} else {
		ceetbckTargetAddress = (long)call_instruction_address;
	}

	/* The callout instruction is one of BASR, BALR or BASSM, as documented by CEETBCK in "z/OS Language Environment Vendor Interfaces "
	 * The size of each of these instructions is 2 bytes, per "The Enterprise Systems Architecture/390 Principles of Operation " */
	ceetbckTargetAddress = ceetbckTargetAddress + 2;

	/* don't infer from this define that getReturnAddressOfJNINative will work on 64-bit z/OS
	 * 	- it's just that setting the high bit clearly only applies to 31-bit z/OS */
#if !defined(J9VM_ENV_DATA64)
	ceetbckTargetAddress = ceetbckTargetAddress | 0x80000000; /* __far_jump() expects 31-bit z/OS addresses to the have high bit */
#endif
	targetAddress = ceetbckTargetAddress;

	return targetAddress;
}

/*
 * Fills in ceArgs, which contains the parameters required by com.ibm.le.conditionhandling.ConditionException
 *
 *
 * @param portLibrary
 * @param gpInfo		cookie used by j9sig_info() to return details of the current condition/signal
 * @param cib			the Condition Information Block, used to query LE for details of the condition
 * @param ceArgs		the parameters required by the constructor for com.ibm.le.conditionhandling.ConditionException.ConditionException.
 * 							this function allocates memory for ceArgs->failingRoutine. It is the responsibility of the caller to free this memory
 * 								If ceArgs is not NULL, the memory can be freed using free().
 */
void
getConditionExceptionConstructorArgs(struct J9PortLibrary* portLibrary, void *gpInfo, struct ConditionExceptionConstructorArgs *ceArgs)
{
	_FEEDBACK rcFc;
	_INT4 offset;
	void *infoValue;
	const char *infoName;
	U_32 infoType;

	char *nameASCII;
	_CHAR80 nameEBCDIC;

	_FEEDBACK newfc;
	_INT2 c_1,c_2,cond_case,sev,control;
	_CHAR3 facid;
	_INT4 isi, heapid, size;
	_POINTER address;
	PORT_ACCESS_FROM_PORT(portLibrary);

	memset(nameEBCDIC, 0, sizeof(_CHAR80));
	memset(&rcFc, 0, sizeof(_FEEDBACK));
	memset(ceArgs, 0, sizeof(struct ConditionExceptionConstructorArgs));

	/* get the failing routine */
	nameASCII = NULL;
	CEE3GRN(nameEBCDIC, &rcFc);
	if ( _FBCHECK(rcFc, CEE000) == 0 ) {
		char *firstBlank = NULL;

		/* CEE3GRN left justifies the name and fills everything to the right of it with blanks, so the result of e2a_func
		 * is a char array that is the same size as nameEBCDIC */
		nameEBCDIC[sizeof(_CHAR80)-1] = 0; /* force null-termination of ebcdic string*/
		nameASCII = e2a_func(nameEBCDIC, sizeof(_CHAR80));	/* e2a_func() uses malloc, caller to free */
		nameASCII[sizeof(_CHAR80)-1] = '\0'; /* force null-termination of ascii string */

		/* remove the trailing blank characters */
		firstBlank = strchr(nameASCII, ' ');
		if (NULL != firstBlank) {
			*firstBlank = '\0';
		}
	}
	ceArgs->failingRoutine = nameASCII;

	/* get the offset into the failing routine */
	CEE3GRO(&offset, &rcFc);
	if ( _FBCHECK(rcFc, CEE000) != 0 ) {
		/* CEE3GR0 failed, make it obvious that the offset is not correct */
		offset = -1;
	}
	ceArgs->offset = (U_64)offset;

	/* get the FEEDBACK token */
	infoType = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_FEEDBACK_TOKEN, &infoName, &infoValue);
	if (J9PORT_SIG_VALUE_ADDRESS != infoType) {
		ceArgs->rawTokenBytes = (void*)&dummy_feedback;
		ceArgs->lenBytesRawTokenBytes = sizeof(_FEEDBACK);
	} else {
		ceArgs->rawTokenBytes = *(void **)infoValue;
		ceArgs->lenBytesRawTokenBytes = sizeof(_FEEDBACK);
	}

	/* get the facility ID */
	infoType = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID, &infoName, &infoValue);
	if (J9PORT_SIG_VALUE_STRING != infoType) {
		ceArgs->facilityID = "";
	} else {
		ceArgs->facilityID = (char *)infoValue;
	}

	/* get the rest, ignore facid as we already have it in ASCII */
	CEEDCOD((_FEEDBACK *)ceArgs->rawTokenBytes,&c_1,&c_2,&cond_case,&sev,&control,facid, &isi,&newfc);

	if (_FBCHECK(newfc, CEE000) == 0) {
		ceArgs->c1 = (U_32)c_1;
		ceArgs->c2 = (U_32)c_2;
		ceArgs->caze = (U_32)cond_case;
		ceArgs->severity = (U_32)sev;
		ceArgs->control = (U_32)control;
		ceArgs->iSInfo = (U_64)isi;
	}

	return;
}

#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
