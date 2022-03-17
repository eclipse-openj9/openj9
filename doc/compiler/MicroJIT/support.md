<!--
Copyright (c) 2022, 2022 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

MicroJIT does not currently support the compilation of:
- System methods
- Methods which call System methods
- Methods which call JNI methods
- Methods which use any of the following bytecodes:
- - 01 (0x01) aconst_null
- - 18 (0x12) ldc
- - 19 (0x13) ldc_w
- - 20 (0x14) ldc2_w
- - 46 (0x2e) iaload
- - 47 (0x2f) laload
- - 48 (0x30) faload
- - 49 (0x31) daload
- - 50 (0x32) aaload
- - 51 (0x33) baload
- - 52 (0x34) caload
- - 53 (0x35) saload
- - 79 (0x4f) iastore
- - 80 (0x50) lastore
- - 81 (0x51) fastore
- - 82 (0x52) dastore
- - 83 (0x53) aastore
- - 84 (0x54) bastore
- - 85 (0x55) castore
- - 86 (0x56) sastore
- - 168 (0xa8) jsr
- - 169 (0xa9) ret
- - 170 (0xaa) tableswitch
- - 171 (0xab) lookupswitch
- - 182 (0xb6) invokevirtual HK
- - 183 (0xb7) invokespecial
- - 185 (0xb9) invokeinterface
- - 186 (0xba) invokedynamic
- - 187 (0xbb) new
- - 188 (0xbc) newarray
- - 189 (0xbd) anewarray
- - 190 (0xbe) arraylength
- - 191 (0xbf) athrow
- - 192 (0xc0) checkcast
- - 193 (0xc1) instanceof
- - 194 (0xc2) monitorenter
- - 195 (0xc3) monitorexit
- - 196 (0xc4) wide
- - 197 (0xc5) multianewarray
- - 200 (0xc8) goto_w
- - 201 (0xc9) jsr_w
- - 202 (0xca) breakpoint
- - 254 (0xfe) impdep1
- - 255 (0xff) impdep2
- MicroJIT is also currently known to have issue with methods that are far down the call stack which contain live object references