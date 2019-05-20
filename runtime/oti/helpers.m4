dnl Copyright (c) 2018, 2019 IBM Corp. and others
dnl
dnl This program and the accompanying materials are made available under
dnl the terms of the Eclipse Public License 2.0 which accompanies this
dnl distribution and is available at https://www.eclipse.org/legal/epl-2.0/
dnl or the Apache License, Version 2.0 which accompanies this distribution and
dnl is available at https://www.apache.org/licenses/LICENSE-2.0.
dnl
dnl This Source Code may also be made available under the following
dnl Secondary Licenses when the conditions for such availability set
dnl forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
dnl General Public License, version 2 with the GNU Classpath
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
divert([-1])


# Prevent `format` macro expanding without arguments
define([_real_format], defn([format]))
undefine([format])
define([format], [ifelse($#, 0, [[format]], [_real_format($@)])])

# mshift(NUM, ARGS ...)
# equivalent to NUM repeated calls to shift() on ARGS
define([mshift],
	[ifelse($1,0,[shift($@)],[mshift([decr($1)],shift(shift($@)))])])

# get_arg_type(arg_str)
# Given a C-style function argument string, get the argument type
# eg. get_arg_type(int foo) => int
define([get_arg_type],
	[ifelse(
		[$1],
		[void],
		[void],
		[patsubst(patsubst([$1],[[^ *]*$]),[\(^\| \)const ])])])

# get_arg_name(arg_str)
# Given a C-tyle argument string, get the name of the argument
# eg. get_arg_name(int foo) => foo
define([get_arg_name],[ifelse([$1],[void],[],[substr([$1],regexp([$1],[[^ *]*$]))])])


# args_names_list(ARGS ...)
# Given a list of C-style argument strings, return a list of the argument names separated by ","
# eg . arg_names_list("int foo", "char* bar", "double baz") => "foo, bar, baz"
define([arg_names_list], [get_arg_name([$1])[]ifelse(eval($# >= 2),[1],[, arg_names_list(shift($@))])])

# strip_r(str)
# remove trailing whitespace from a string
define([strip_r], [patsubst([$1], [ *$])])

# strip(str)
# remove leading and trailing whitespace from a string
define([strip],
	[patsubst(strip_r([$1]),[^ *])])


# invokePrefix(return_type)
# if return_type == "void" evaluate to empty string, else evaluate to "return"
# NOTE: should probably be named better, but this is how it was named in the freemarker templates
define([invokePrefix], [ifelse(strip($1),[void],,[return ])])

# type_is_ptr(c_type)
# Given a C type (eg "int"), determine if it is a pointer type
define([type_is_ptr],[ifelse(index($1,[*]),[-1],[0],[1])])

# type_is_wide(typename)
# Given a C type determine if it is a wide type (8 bytes)
define([type_is_wide],[ifelse($1,[jlong],[1],$1,[jdouble],1,0)])

# get_arg_size_impl(c_type)
# Helper function for get_arg_size(). Given a C type (eg "int"), returns its size in bytes
define([get_arg_size_impl],[ifelse(type_is_ptr($1),[1],[4],type_is_wide($1),[1],[8],ifelse($1,[void],0,4))])

# get_arg_size(c_arg_str)
# given a C-Style argument string (eg. "int x"), get its size in bytes
define([get_arg_size],[get_arg_size_impl(strip(get_arg_type($1)))])


define([get_function_arg_size_impl], [get_arg_size($1) ifelse(eval($# >= 2),[1],[+ get_function_arg_size_impl(shift($@))])])

# get_function_arg_size(ARGS ...)
# Given a list of C-style argument strings, calculate the size of all the arguments
define([get_function_arg_size], [eval(get_function_arg_size_impl($@))])

# decorate_impl(func_name, ignored, ignored, ignored, ARGS ...)
# Given a function name and a list of C-style argument strings, get the decorated function name.
# This uses Microsoft-style C name mangling for stdcall functions.
# see https://docs.microsoft.com/en-us/cpp/build/reference/decorated-names#FormatC
# ex: decorate_impl("foo", ignored, ignored,ignored, "int foo", "int bar") => "_foo@8"
define([decorate_impl], [_$1@get_function_arg_size(mshift(4,$@))])

# decorate_function_name(func_name, ignored, decorate, ignored, ARGS ..)
# if decorate == true, evaluate to the decorated function name, else evaluate to func_name
# see decorate_impl
define([decorate_function_name], [ifelse($3,[true],decorate_impl($@),$1)])


# join(separator, ARGS ...)
# evaluate to all of ARGS separated by separator
define([join], [$2[]ifelse([$#],[2],[],[$1join([$1],mshift(2,$@))])])

divert[]dnl
