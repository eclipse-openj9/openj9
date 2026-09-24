/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "ras/Logger.hpp"
#include "codegen/Snippet.hpp"

TR::X86GuardedDevirtualSnippet *J9::X86::Snippet::getGuardedDevirtualSnippet() { return NULL; }

void J9::X86::Snippet::printName(OMR::Logger *log)
{
    const char *name;

    switch (getKind()) {
        case TR::Snippet::IsCall:
            name = "Call Snippet";
            break;
        case TR::Snippet::IsVPicData:
            name = "VPic Data";
            break;
        case TR::Snippet::IsIPicData:
            name = "IPic Data";
            break;
        case TR::Snippet::IsForceRecompilation:
            name = "Force Recompilation Snippet";
            break;
        case TR::Snippet::IsRecompilation:
            name = "Recompilation Snippet";
            break;
        case TR::Snippet::IsGuardedDevirtual:
            name = "Guarded Devirtual Snippet";
            break;
        default:
            name = NULL;
            break;
    }

    if (name) {
        log->prints(name);
    } else {
        // Delegate to superclass to print name
        J9::Snippet::printName(log);
    }
}
