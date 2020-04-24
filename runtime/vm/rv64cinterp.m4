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

dnl We temporarily use rv64cinterpnojit to invoke interpreter without JIT.
dnl The assembly here needs to be modified to enable JIT against the RISCV Instruction Sets Specification.

include(rv64helpers.m4)

	.file "rv64cinterp.s"

DECLARE_PUBLIC(cInterpreter_placeholder)
DECLARE_PUBLIC(cInterpreter)

START_PROC(cInterpreter_placeholder)
cInterpreter:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	nop
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
END_PROC(cInterpreter_placeholder)
