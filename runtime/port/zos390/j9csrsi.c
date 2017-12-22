/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
/* CSRSIC include file is part of SYS1.SAMPLIB data set. */
#include "//'SYS1.SAMPLIB(CSRSIC)'"
#include "j9csrsi.h"

#define CSRSI_DSECT_LGTH                    0x4040U
#define CSRSI_PARAM_LGTH                    4U

struct j9csrsi_t {
	CSRSIRequest request;               /**< Input - CSRSI request type */
	CSRSIInfoAreaLen infoarea_len;      /**< Input - length of the buffer
						area for returning data */
	CSRSIReturnCode ret_code;           /**< Output - CSRSI return code */
	char infoarea[CSRSI_DSECT_LGTH];    /**< Input/Output - supplied buffer
						area that gets filled upon
						service return */
	uint32_t cplist[CSRSI_PARAM_LGTH];  /**< Storage for constructing
						4-argument parameter list */
};

const j9csrsi_t *
j9csrsi_init(int32_t *ret)
{
	struct j9csrsi_t *sess = NULL;

	/* CSRSI is amode31 only so we need memory below the 2GB */
	sess = (struct j9csrsi_t *) __malloc31(sizeof(*sess));

	if (NULL != sess) {
		memset(sess, 0, sizeof(*sess));

		/* Refer to CSRSIIDF programming interface information in
		 * MVS Data Areas Volume 1 (ABEP-DDRCOM). SIV2V3 DSECT maps
		 * the data for LPAR and VM.
		 */
		sess->request = (CSRSI_REQUEST_V1CPC_MACHINE |
						 CSRSI_REQUEST_V2CPC_LPAR | CSRSI_REQUEST_V3CPC_VM);
		sess->infoarea_len = sizeof(sess->infoarea);

		j9csrsi_wrp(sess);

		if (NULL != ret) {
			*ret = sess->ret_code;
		}

		/* Did CSRSI fail? */
		if (CSRSI_SUCCESS != sess->ret_code) {
			free(sess);
			sess = NULL;
		}
	}

	return sess;
}

BOOLEAN
j9csrsi_is_hw_info_available(int32_t ret)
{
	/* CSRSI_STSINOTAVAILABLE should never happen because STSI is available
	 * on all platforms we support (see comment 12 in JAZZ 90568).
	 */
	return !((CSRSI_STSINOTAVAILABLE == ret)
			 || (CSRSI_SERVICENOTAVAILABLE == ret));
}

void
j9csrsi_shutdown(const j9csrsi_t *session)
{
	if (NULL != session) {
		free(session);
	}

	return;
}

BOOLEAN
j9csrsi_is_lpar(const j9csrsi_t *session)
{
	BOOLEAN ret = FALSE;

	if (NULL != session) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		ret = (BOOLEAN)
			  (SI00CPCVARIETY_V2CPC_LPAR == hdr->si00cpcvariety)
			  && hdr->si00validsi22v2;
	}

	return ret;
}

BOOLEAN
j9csrsi_is_vmh(const j9csrsi_t *session)
{
	BOOLEAN ret = FALSE;

	if (NULL != session) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		ret = (BOOLEAN)
			  (SI00CPCVARIETY_V3CPC_VM == hdr->si00cpcvariety)
			  && hdr->si00validsi22v3;
	}

	return ret;
}

int32_t
j9csrsi_get_vmhpidentifier(const j9csrsi_t *session, uint32_t position,
						   char *buf, uint32_t len)
{
	int32_t ret = -1;

	if ((NULL != session)
		&& (NULL != buf)
		&& (0U < len)
		&& j9csrsi_is_vmh(session)
	) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si22v3 *const info =
			(const si22v3 *) &(ia->siv1v2v3si22v3);

		if (info->si22v3dbcountfield._si22v3dbcount > position) {
			const si22v3db *const db = &(info->si22v3dbe[position]);
			const char *const str =
				&(db->si22v3dbvmhpidentifier[0U]);
			const uint32_t str_sz =
				sizeof(db->si22v3dbvmhpidentifier);

			if (str_sz < len) {
				(void) strncpy(buf, str, str_sz);
				buf[str_sz] = '\0';

				ret = __etoa(buf);
			}
		}
	}

	return ret;
}

int32_t
j9csrsi_get_cpctype(const j9csrsi_t *session, char *buf, uint32_t len)
{
	int32_t ret = -1;

	if ((NULL != session)
		&& (NULL != buf)
		&& (0U < len)
	) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		if (hdr->si00validsi11v1
			&& (
				(SI00CPCVARIETY_V1CPC_MACHINE == hdr->si00cpcvariety)
				|| (SI00CPCVARIETY_V2CPC_LPAR == hdr->si00cpcvariety)
				|| (SI00CPCVARIETY_V3CPC_VM   == hdr->si00cpcvariety))
		) {
			const si11v1 *const info =
				(const si11v1 *) &(ia->siv1v2v3si11v1);
			const char *const str = &(info->si11v1cpctype[0U]);
			const uint32_t str_sz = sizeof(info->si11v1cpctype);

			if (str_sz < len) {
				(void) strncpy(buf, str, str_sz);
				buf[str_sz] = '\0';

				ret = __etoa(buf);
			}
		}
	}

	return ret;
}
