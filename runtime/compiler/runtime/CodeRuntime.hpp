/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef CODERUNTIME_HPP
#define CODERUNTIME_HPP

namespace TR { class CodeGenerator; }

namespace TR {

extern void createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void ** CCPreLoadedCodeTable, TR::CodeGenerator *cg);
extern uint32_t getCCPreLoadedCodeSize();

}

struct J9JITConfig;
void initializeCodeRuntimeHelperTable(J9JITConfig *jitConfig, char isSMP);

// Uses of these next three (the JITRT_PRINTF looks a bit weird, but not completely awful):
//   JITRT_LOCK_LOG(jitConfig);
//   JITRT_PRINTF(jitConfig)(jitConfig, "Format string: %s\n", string);
//   JITRT_UNLOCK_LOG(jitConfig);
//

#define   JITRT_PRINTF(jitConfig)     ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->j9jitrt_printf
#define   JITRT_LOCK_LOG(jitConfig)   ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->j9jitrt_lock_log(jitConfig)
#define   JITRT_UNLOCK_LOG(jitConfig) ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->j9jitrt_unlock_log(jitConfig)

#endif // CODERUNTIME_HPP
