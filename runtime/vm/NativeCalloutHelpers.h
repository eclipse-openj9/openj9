/*******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 *
 * (c) Copyright IBM Corp. 1991, 2017 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 *******************************************************************************/

#ifndef _NATIVECALLOUTHELPERS_H
#define _NATIVECALLOUTHELPERS_H

#ifdef J9VM_OPT_PANAMA
#include "ffi.h"

typedef struct J9NativeCalloutData {
	ffi_type **arguments;
	ffi_cif *cif;
} J9NativeCalloutData;

typedef struct J9NativeStructData {
	char *layoutString;
	ffi_type *structType;
} J9NativeStructData;
#endif /* J9VM_OPT_PANAMA */

#endif /* NATIVECALLOUTHELPERS_H */
