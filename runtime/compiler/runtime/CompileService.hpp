/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "vmaccess.h" // for acquireVMAccess and releaseVMAccess
#include "net/ServerStream.hpp" // for JITServer::BaseCompileDispatcher
#include "runtime/Listener.hpp"

struct J9JITConfig;
struct J9VMThread;

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

/**
   @class J9CompileDispatcher
   @brief Implementation of the compilation handler which gets invoked every time a connection request from the JITClient is accepted

   This handler, 'compile(ServerStream *)', is executed by the listener thread when a new connection request has been received by JITServer
*/

class J9CompileDispatcher : public BaseCompileDispatcher
{
public:
   J9CompileDispatcher(J9JITConfig *jitConfig) : _jitConfig(jitConfig) { }

   void compile(JITServer::ServerStream *stream) override;

private:
   J9JITConfig *_jitConfig;
};

#endif

