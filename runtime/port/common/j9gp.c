/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

/**
 * @ingroup Port
 * @file
 * Provides platform-neutral signal handling functions.  
 * The @ref j9gp_register_handler function is partially dependent on J9 internal VM structures, and must be called with NULL as a third (userData) parameter.
 * <br><br>
 */

#include "omrcfg.h"
#include "j9port.h"
#include "j9comp.h"
#include "gp.h"

/**
 * Kicks off the new thread by calling the function provided in protected_fn fn.
 *
 * All threads spawned by the vm start here and all OS signals that will be handled by fn must be registered to the OS here. 
 * Upon receiving a signal from the OS, fn is responsible for calling the function specified in @ref j9gp_register_handler if it is determined that a shutdown is required.
 * 
 * @param[in] portLibrary The port library
 * @param[in] fn the function that will be used to kick off the thread
 * @param[in] arg arguments to protected_fn fn
 *
 * @return the return value of the function provided in fn
 * 
 * @note it is a good idea to save the portLibrary in case a registered exception handler cannot be found
 */
uintptr_t
j9gp_protect(struct J9PortLibrary *portLibrary,  protected_fn fn, void *arg )
{
	/* sorry!  No way to protect it generically.  Just run it. */
	return fn(arg);
}
/**
 * Sets the function that is responsible for preserving/outputting the state of the vm and initiating a graceful shutdown resulting from a gp.
 *
 * @param[in] portLibrary The port library
 * @param[in] fn function responsible for preserving/outputting the state of the vm and initiating a graceful shutdown resulting from a gp.
 * @param[in] aUserData The J9JavaVM or NULL for non-J9 consumers.
 *
 * @note vmGPHandler in gphandle.c is currently the only function used for handling gps. 
 * @note  fn is not called by the OS but by the gp handler function specified in the call to @ref j9gp_protect gp module
 * @note above occurs after the OS has passed a signal along to us and it is determined that a shutdown is required.
 */
void
j9gp_register_handler(struct J9PortLibrary *portLibrary,  handler_fn fn, void *aUserData )
{
}
/**
 * Provides the name and value, specified by category/index of the gp information in info. 
 * Returns the kind of information found at category/index specified, or undefined 
 * if the category/index are invalid.  The number of items in the category specified must 
 * equal the count @ref j9gp_info_count returns for that category. 
 *
 * @param[in] portLibrary The port library
 * @param[in] info struct containing all available signal information. Normally includes register values, name of module where crash occured and its base address.
 * @param[in] category the category of signal information that you are querying. 
 * @param[in] index the index of the item in the specified category. The number of items for each category are defined in the sourceTemplate.
 * @param[out] name the name of the item at the specified index.  
 * @param[out] value the value of the item at the specified index
 *
 * @return the kind of info at the specified index. For example, this allows the caller to determine whether the item placed in **value corresponds to a 32/64-bit integer or a pointer to a string. 
 *
 * @note The program counter and module name also have negative indexes as defined by J9GP_CONTROL_PC and J9GP_MODULE_NAME respectively.
 * @note Above allows the handler function registered in @ref j9gp_register_handler to distinguish (and use) them from the other gp items.
 * @note The caller is responsible for allocating and freeing any buffers used by **name, **value.
 */
uint32_t
j9gp_info(struct J9PortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value)
{	
	/* default implementation can't do anything */
	return 0;
}
/**
 * Returns the number of items that exist in the category specified, or zero if the category is undefined.
 *
 * @param[in] portLibrary The port library
 * @param[in] info struct containing all available signal information. Normally includes register values, name of module where crash occured and its base address. 
 * @param[in] category the category in which we want to find the number of items that exist.
 *
 * @note Return value must agree with the number of items that @ref j9gp_info makes available for the category specified. 
*/
uint32_t
j9gp_info_count(struct J9PortLibrary *portLibrary, void *info, uint32_t category)
{
	/* default implementation can't do anything */
	return 0;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref j9gp_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
j9gp_shutdown(struct J9PortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the shared library operation may be created here.  All resources created here should be destroyed
 * in @ref j9gp_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_GP
 *
 * @note Most implementations will simply return success.
 */
int32_t
j9gp_startup(struct J9PortLibrary *portLibrary)
{
	return 0;
}

