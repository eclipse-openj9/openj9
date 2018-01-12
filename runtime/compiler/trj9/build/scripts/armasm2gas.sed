#!/bin/sed

# Copyright (c) 2000, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

# Do comments first since they can appear anywhere
s/;/@/g

# Fix up directives
s/^[[:space:]]*DCD[[:space:]]\+\(.*\)$/.long \1/g
s/^[[:space:]]*DCW[[:space:]]\+\(.*\)$/.short \1/g
s/^[[:space:]]*DCB[[:space:]]\+\(.*\)$/.byte \1/g
s/^[[:space:]]*\([[:alnum:]_]\+\)[[:space:]]\+[Ee][Qq][Uu][[:space:]]\+\(.*\)$/ \1 = \2/g
s/^[[:space:]]*AREA.*CODE.*$/.text/g
s/^[[:space:]]*AREA.*DATA.*$/.data/g
s/^[[:space:]]*IMPORT[[:space:]]\+\(.*\)$/.globl \1/g
s/^[[:space:]]*EXPORT[[:space:]]\+\(.*\)$/.globl \1/g
s/^[[:space:]]*ALIGN[[:space:]]\+\(.*\)$/.balign \1/g
s/^[[:space:]]*include[[:space:]]\+\(.*$\)/.include "\1"/g
s/^[[:space:]]*END[[:space:]]*$//g

# Ok, now look for labels
s/^\([[:alpha:]_][[:alnum:]_]*\)\(.*\)/\1:\2/g

# Fix up non-local branches to append "(PLT)"
s/^\([[:space:]]\+[Bb][Ll]\{0,1\}\(eq\|ne\|cs\|cc\|mi\|pl\|vs\|vc\|hi\|ls\|lt\|gt\|le\|ge\|al\)\{0,1\}[[:space:]]\+\)\([[:alpha:]_][[:alnum:]_]*\)\(.*\)$/\1\3(PLT)\4/g
s/^\([[:space:]]\+[Bb][Ll]\{0,1\}\(eq\|ne\|cs\|cc\|mi\|pl\|vs\|vc\|hi\|ls\|lt\|gt\|le\|ge\|al\)\{0,1\}[[:space:]]\+\)\(L_[[:alnum:]_]*\)(PLT)\(.*\)$/\1\3\4/g

