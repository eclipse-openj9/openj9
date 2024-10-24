/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Runtime instrumentation
 */
#include "j9port.h"
#include "ut_j9prt.h"
#include "j9ri.h"
#define MMCR0           779
#define MMCR0_FC        0x80000000
#define MMCR0_PMAE      0x04000000

/* Please keep this definition in sync with the one in PPCHWProfilerPrivate.hpp. */
#if defined(__xlC__) || defined(__ibmxl__) || defined(__open_xl__)
#define MTSPR(spr, src) __mtspr(spr, src)
#elif defined(__GNUC__)
#define MTSPR(spr, src) asm("mtspr %0,%1\n\t" : : "i" (spr), "r" (src))
#endif

/* Functions added for Jazz design 55557 */

/**
 * Initialize the supplied J9RIParameters with default values: controlBlock=riControlBlock, all flags cleared.
 *
 * @param[in] portLibrary The port library
 * @param[in] riParams pointer to generic runtime instrumentation parameters structure.
 * @param[in] riControlBlock pointer to machine architecture dependent control block
 */
void
j9ri_params_init(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams, void *riControlBlock)
{
	Trc_PRT_ri_params_init_Entry();
	riParams->flags = 0;
	riParams->controlBlock = riControlBlock;
	Trc_PRT_ri_params_init_Exit();
}

int32_t
j9ri_enableRISupport(struct J9PortLibrary *portLibrary)
{
	Trc_PRT_ri_enableSupport_Entry();

	Trc_PRT_ri_enableSupport_Exit();

	return 0;
}

int32_t
j9ri_disableRISupport(struct J9PortLibrary *portLibrary)
{
	Trc_PRT_ri_disableSupport_Entry();

	Trc_PRT_ri_disableSupport_Exit();

	return 0;
}

/**
 * Set up hardware and/or operating structures for runtime instrumentation.
 * This must be called on the current thread before calling j9ri_enabled.
 *
 * This sets the J9PORT_RI_INITIALIZED in riParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] riParams pointer to generic runtime instrumentation parameters structure.
 */
void
j9ri_initialize(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	Trc_PRT_ri_initialize_Entry();

	Trc_PRT_ri_initialize_Exit();
}

/**
 * Destroys hardware and/or operating system runtime instrumentation structures created by j9ri_initialize.
 * It must be called to release resources allocated by j9ri_initialize.
 *
 * This clears the J9PORT_RI_INITIALIZED bit in riParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] riParams pointer to generic runtime instrumentation parameters structure.
 */
void
j9ri_deinitialize(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	Trc_PRT_ri_deinitialize_Entry();

	Trc_PRT_ri_deinitialize_Exit();
}

/**
 * Enable runtime instrumentation on the calling hardware thread.
 *
 * This sets the J9PORT_RI_ENABLED bit in riParams->flags. Calling this on an already enabled thread
 * may result in undefined behaviour.
 *
 * @param[in] portLibrary The port library
 * @param[in] riParams pointer to generic runtime instrumentation parameters structure.
 */
void
j9ri_enable(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	Trc_PRT_ri_enable_Entry();

#if defined(MTSPR)
	if (J9_ARE_ALL_BITS_SET(riParams->flags, J9PORT_RI_INITIALIZED)) {
		MTSPR(MMCR0, MMCR0_PMAE);
		riParams->flags |= J9PORT_RI_ENABLED;
	}
#endif /* defined(MTSPR) */

	Trc_PRT_ri_enable_Exit();
}

/**
 * Disable runtime instrumentation on the calling hardware thread.
 *
 * The thread must already be authorized.  It is harmless
 * to call this if instrumentation is already disabled.
 * This clears the J9PORT_RI_ENABLED bin in riParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] riParams pointer to generic runtime instrumentation parameters structure.
 */
void
j9ri_disable(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	Trc_PRT_ri_disable_Entry();

#if defined(MTSPR)
	if (J9_ARE_ALL_BITS_SET(riParams->flags, J9PORT_RI_INITIALIZED)) {
		MTSPR(MMCR0, MMCR0_FC);
		riParams->flags &= ~J9PORT_RI_ENABLED;
	}
#endif /* defined(MTSPR) */

	Trc_PRT_ri_disable_Exit();
}
