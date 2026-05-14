/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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
#if !defined(JFRTYPEMAPPINGS_HPP_)
#define JFRTYPEMAPPINGS_HPP_

#include "jfrEventEnum.hpp"
#include "jfrTypeEnum.hpp"

struct EventMapping {
	const char *name;
	int id;
};

static const EventMapping EVENT_MAPPINGS[] = {
#include "jfrEventMapping.hpp"
};

struct TypeMapping {
	const char *name;
	int id;
};

static const TypeMapping TYPE_MAPPINGS[] = {
  { "java/lang/Class", TYPE_CLASS },
  { "java/lang/Thread", TYPE_THREAD },
  { "long", TYPE_LONG },
  { "int", TYPE_INT },
  { "short", TYPE_SHORT },
  { "byte", TYPE_BYTE },
  { "double", TYPE_DOUBLE },
  { "float", TYPE_FLOAT },
  { "boolean", TYPE_BOOLEAN },
  { "char", TYPE_CHAR },
  { "java/lang/String", TYPE_STRING },
  { "jdk/types/StackTrace", TYPE_STACKTRACE },
};

#endif /* !defined(JFRTYPEMAPPINGS_HPP_) */
