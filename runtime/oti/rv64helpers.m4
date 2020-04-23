dnl Copyright (c) 2019, 2020 IBM Corp. and others
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

dnl The source needs to be modified to enable JIT against the RISCV Instruction Sets Specification.
dnl include(jilvalues.m4)

changequote({,})dnl

define({FUNC_LABEL},{$1})

define({DECLARE_PUBLIC},{
	.globl FUNC_LABEL($1)
	.type FUNC_LABEL($1),@function
})

define({START_PROC},{
	.text
	DECLARE_PUBLIC($1)
	.align 2
	FUNC_LABEL($1):
})

define({END_PROC})
