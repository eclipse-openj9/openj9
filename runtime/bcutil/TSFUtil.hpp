/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
/*
 * TSFUtil.hpp
 */




#ifndef TSFUTIL_HPP_
#define TSFUTIL_HPP_
#include "j9comp.h"

struct J9TenantGlobals;

class TSFUtil
{
    public:
        TSFUtil();
        ~TSFUtil();
        static bool isFilterClass(J9TenantGlobals* tenantGlobals, const UDATA nameLen, const U_8 *className);
		static bool isTclinit(const UDATA nameLen, const U_8 *name, const UDATA descLen, const U_8* desc);
		static bool isClinit(const UDATA nameLen, const U_8 *name, const UDATA descLen, const U_8* desc);
    protected:
        static UDATA U_8_len(const U_8 *str);
        enum CompareResult{ contained = -1, missMatch = 0, equal = 1, header = 2};

        static CompareResult compare(const UDATA strLen, const U_8 *str, const U_8 *target);

        static bool isHeader(const UDATA strLen, const U_8 *str, const U_8 *header){return 0 < compare(strLen, str, header);};
        static bool isEqual(const UDATA strLen, const U_8 *str, const U_8 *target){return equal == compare(strLen, str, target);};

    private:
		static const U_8* METHOD_TCLINIT;
		static const U_8* DESC_TCLINIT;
		static const U_8* METHOD_CLINIT;
		static const U_8* DESC_CLINIT;
};


#endif // TSFUTIL_HPP_

