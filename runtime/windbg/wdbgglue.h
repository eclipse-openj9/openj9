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


#ifndef WDBGGLUE_H
#define WDBGGLUE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "j9dbgext.h"
#include "jni.h"
#include "j9port.h"
#include "j9comp.h"
#include "omrthread.h"
#include "thrdsup.h"
#include "thrtypes.h"

#include "j9socket.h"
#include "j9protos.h"
#include "j9cp.h"
#ifdef J9VM_OPT_ZIP_SUPPORT
#include "zip_api.h"
#endif

#define DECLARE_API(s) void s(IDATA hCurrentProcess,IDATA hCurrentThread,UDATA dwCurrentPc,UDATA dwProcessor,char* args)


DECLARE_API(j9help);

DECLARE_API(findvm);

DECLARE_API(trprint);

DECLARE_API(setvm);

#endif
