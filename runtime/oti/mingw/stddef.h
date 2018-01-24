/*******************************************************************************
 *
 * This file is derived from the musl libc (http://www.musl-libc.org) version 1.1.4.
 * Original file https://github.com/cloudius-systems/musl/blob/master/include/stddef.h
 * The copyright (https://github.com/cloudius-systems/musl/blob/master/COPYRIGHT) states:
 * 		All public header files (include/<star> and arch/<star>/bits/<star>) should be
 *		treated as Public Domain as they intentionally contain no content
 *		which can be covered by copyright. Some source modules may fall in
 *		this category as well. If you believe that a file is so trivial that
 *		it should be in the Public Domain, please contact the authors and
 *		request an explicit statement releasing it from copyright.
 *
 *		The following files are trivial, believed not to be copyrightable in
 *		the first place, and hereby explicitly released to the Public Domain:
 *
 *		All public headers: include/<star>, arch/<star>/bits/<star>
 *		Startup files: crt/*
 */

#ifndef _STDDEF_H
#define _STDDEF_H

#if __GNUC__ > 3
#define offsetof(type, member) __builtin_offsetof(type, member)
#else
#define offsetof(type, member) ((size_t)( (char *)&(((type *)0)->member) - (char *)0 ))
#endif

#endif
