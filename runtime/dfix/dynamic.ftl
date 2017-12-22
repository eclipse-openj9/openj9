<#--
   Copyright (c) 2011, 2017 IBM Corp. and others

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
<#-- Don't forget to export the function by adding to dfix-exports.xml. -->
<#assign functions = 
[
	{ "name": "dfix_zero_memory", "return": "void", "args": "void* address, size_t size", "cc": ""},
	{ "name": "dfix_memcpy", "return": "void", "args": "void* dest, const void* src, size_t size", "cc": ""}
]
>