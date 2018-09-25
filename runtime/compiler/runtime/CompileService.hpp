/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef COMPILE_SERVICE_H
#define COMPILE_SERVICE_H

#include "rpc/J9Server.hpp"
#include "j9.h"
#include "j9nonbuilder.h"
#include "vmaccess.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "control/CompilationRuntime.hpp"
#include "control/CompilationController.hpp"
#include "env/ClassLoaderTable.hpp"

class VMAccessHolder
   {
public:
   VMAccessHolder(J9VMThread *vm): _vm(vm)
      {
      acquireVMAccess(_vm);
      }

   ~VMAccessHolder()
      {
      releaseVMAccess(_vm);
      }

private:
   J9VMThread *_vm;
   };


class J9CompileDispatcher : public JITaaS::J9BaseCompileDispatcher
{
public:
   J9CompileDispatcher(J9JITConfig *jitConfig, J9VMThread *vmThread) : _jitConfig(jitConfig), _vmThread(vmThread) { }

   void compile(JITaaS::J9ServerStream *stream) override;

private:
   J9JITConfig *_jitConfig;
   J9VMThread *_vmThread;
};




#endif
