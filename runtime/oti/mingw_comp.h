/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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

#ifndef mingw_comp_h
#define mingw_comp_h

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "stddef.h"
#endif /* defined(__MINGW32__) || defined(__MINGW64__) */


/*
 * definitions in this header file ignored a lot of check. this is because we use
 * this header file only in such conditions:
 * 1. Target code is for MS windows 32/64 bits only,
 * 2. MinGW GCC compile BytecodeInterpreter.cpp and DebugBytecodeInterpreter.cpp
 *    only,
 * 3. MSVC compiler links MinGW generated objects only.
 */

#if defined(WIN32) && defined(__GNUC__)

#define __int8  char
#define __int16 short
#define __int32 int
#define __int64 long long

#if defined(_WIN32)
#define _W64
#endif

/*
 * wchar_t is C++ builtin type, not C.
 * C has its definition in stddef.h
 * therefore, if _cplusplus, don't use C definition
 */
#if defined(__cplusplus)
#define _WCHAR_T_DEFINED
#endif

/*
 * convert msvc specific syntax
 */
#define __pragma(x) _Pragma(#x)

#ifndef _CRTNOALIAS
#define _CRTNOALIAS
#endif /* _CRTNOALIAS */

#if !defined(VS12AndHigher)
#ifndef _CRTRESTRICT
#define _CRTRESTRICT
#endif  /* _CRTRESTRICT */
#else /* !defined(VS12AndHigher) */
/* The inlined stdio methods confuse mingw */
#define _NO_CRT_STDIO_INLINE
#endif /* !defined(VS12AndHigher) */

#endif /* WIN32 && __GNUC__ */

#endif /* mingw_comp_h */
