/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef PERSISTENT_COLLECTIONS_H
#define PERSISTENT_COLLECTIONS_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

template<typename T>
using PersistentVectorAllocator = TR::typed_allocator<T, TR::PersistentAllocator&>;
template<typename T>
using PersistentVector = std::vector<T, PersistentVectorAllocator<T>>;

template<typename T>
using PersistentUnorderedSetAllocator = TR::typed_allocator<T, TR::PersistentAllocator&>;
template<typename T>
using PersistentUnorderedSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>, PersistentUnorderedSetAllocator<T>>;

template<typename T, typename U>
using PersistentUnorderedMapAllocator = TR::typed_allocator<std::pair<const T, U>, TR::PersistentAllocator&>;
template<typename T, typename U>
using PersistentUnorderedMap = std::unordered_map<T, U, std::hash<T>, std::equal_to<T>, PersistentUnorderedMapAllocator<T, U>>;

template<typename T, typename U>
using UnorderedMapAllocator = TR::typed_allocator<std::pair<const T, U>, TR::Region&>;
template<typename T, typename U>
using UnorderedMap = std::unordered_map<T, U, std::hash<T>, std::equal_to<T>, UnorderedMapAllocator<T, U>>;

template<typename T>
using VectorAllocator = TR::typed_allocator<T, TR::Region&>;
template<typename T>
using Vector = std::vector<T, VectorAllocator<T>>;

#endif
