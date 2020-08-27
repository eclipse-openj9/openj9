/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

#ifndef J9ARGSCAN_H
#define J9ARGSCAN_H

#include "j9port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 * @param portLibrary
 * @param input
 * @return char *
 */
char * trim(J9PortLibrary *portLibrary, char * input);

/**
* @brief
* @param portLibrary
* @param module
* @param *scan_start
* @return void
*/
void scan_failed(J9PortLibrary *portLibrary, const char* module, const char *scan_start);


/**
* @brief
* @param portLibrary
* @param module
* @param *scan_start
* @return void
*/
void scan_failed_incompatible(J9PortLibrary *portLibrary, char* module, char *scan_start);


/**
* @brief
* @param portLibrary
* @param module
* @param *scan_start
* @return void
*/
void scan_failed_unsupported(J9PortLibrary *portLibrary, char* module, char *scan_start);


/**
* @brief
* @param **scan_start
* @param result
* @return uintptr_t
*/
uintptr_t scan_hex(char **scan_start, uintptr_t* result);


/**
* @brief
* @param **scan_start
* @param uppercaseFalg
* @param result
* @return uintptr_t
*/
uintptr_t scan_hex_caseflag(char **scan_start, BOOLEAN uppercaseAllowed, uintptr_t* result);

/**
* @brief
* @param **scan_start
* @param *result
* @return uintptr_t
*/
uintptr_t scan_hex_u64(char **scan_start, uint64_t* result);


/**
* @brief
* @param **scan_start
* @param *result
* @return uintptr_t
*/
uintptr_t scan_hex_caseflag_u64(char **scan_start, BOOLEAN uppercaseAllowed, uint64_t* result);

/**
* @brief
* @param **scan_start
* @param *result
* @return uintptr_t
*/
uintptr_t scan_idata(char **scan_start, intptr_t *result);


/**
* @brief
* @param portLibrary
* @param **scan_start
* @param delimiter
* @return char *
*/
char *scan_to_delim(J9PortLibrary *portLibrary, char **scan_start, char delimiter);

/** 
 * Scan the next double number off of the argument string.
 * Store the scanned double value in *result
 *
 * @param[in/out] scan_start pointer to char * representing the string to be scanned,
                  on success, *scan_start is updated to point to the character after the last character converted to double
 * @param[out] result pointer to double, on success contains the scanned double value
 * @return on success returns OPTION_OK, on overflow returns OPTION_OVERFLOW,
 *         if no conversion is performed return OPTION_MALFORMED
 */
uintptr_t scan_double(char **scan_start, double *result);

/**
* @brief
* @param **scan_start
* @param result
* @return uintptr_t
*/
uintptr_t scan_udata(char **scan_start, uintptr_t* result);


/**
* @brief
* @param **scan_start
* @param result
* @return uintptr_t
*/
uintptr_t scan_u64(char **scan_start, uint64_t* result);


/**
* @brief
* @param **scan_start
* @param result
* @return uint32_t
*/
uintptr_t scan_u32(char **scan_start, uint32_t* result);

/**
* @brief
* @param **scan_start
* @param result
* @return uintptr_t
*/
uintptr_t scan_u64_memory_size(char **scan_start, uint64_t* result);

/**
* @brief
* @param **scan_start
* @param result
* @return uintptr_t
*/
uintptr_t scan_udata_memory_size(char **scan_start, uintptr_t* result);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* J9ARGSCAN_H */
