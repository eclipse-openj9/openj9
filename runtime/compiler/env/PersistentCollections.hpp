/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef PERSISTENT_COLLECTIONS_H
#define PERSISTENT_COLLECTIONS_H

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "env/PersistentAllocator.hpp"
#include "env/Region.hpp"


template<typename T>
using PersistentVectorAllocator = TR::typed_allocator<T, TR::PersistentAllocator &>;
template<typename T>
using PersistentVector = std::vector<T, PersistentVectorAllocator<T>>;

template<typename T>
using PersistentListAllocator = TR::typed_allocator<T, TR::PersistentAllocator &>;
template<typename T>
using PersistentList = std::list<T, PersistentListAllocator<T>>;

template<typename T>
using PersistentUnorderedSetAllocator = TR::typed_allocator<T, TR::PersistentAllocator &>;
template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>>
using PersistentUnorderedSet = std::unordered_set<T, H, E, PersistentUnorderedSetAllocator<T>>;

template<typename K, typename V>
using PersistentUnorderedMapAllocator = TR::typed_allocator<std::pair<const K, V>, TR::PersistentAllocator &>;
template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using PersistentUnorderedMap = std::unordered_map<K, V, H, E, PersistentUnorderedMapAllocator<K, V>>;


template<typename T>
using VectorAllocator = TR::typed_allocator<T, TR::Region &>;
template<typename T>
using Vector = std::vector<T, VectorAllocator<T>>;

template<typename T>
using UnorderedSetAllocator = TR::typed_allocator<T, TR::Region &>;
template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>>
using UnorderedSet = std::unordered_set<T, H, E, UnorderedSetAllocator<T>>;

template<typename K, typename V>
using UnorderedMapAllocator = TR::typed_allocator<std::pair<const K, V>, TR::Region &>;
template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using UnorderedMap = std::unordered_map<K, V, H, E, UnorderedMapAllocator<K, V>>;


namespace std
   {
   template<typename T0, typename T1> struct hash<std::pair<T0, T1>>
      {
      size_t operator()(const std::pair<T0, T1> &k) const noexcept
         {
         return std::hash<T0>()(k.first) ^ std::hash<T1>()(k.second);
         }
      };

   template<typename T0, typename T1> struct hash<std::tuple<T0, T1>>
      {
      size_t operator()(const std::tuple<T0, T1> &k) const noexcept
         {
         return std::hash<T0>()(std::get<0>(k)) ^ std::hash<T1>()(std::get<1>(k));
         }
      };

   template<typename T0, typename T1, typename T2> struct hash<std::tuple<T0, T1, T2>>
      {
      size_t operator()(const std::tuple<T0, T1, T2> &k) const noexcept
         {
         return std::hash<T0>()(std::get<0>(k)) ^ std::hash<T1>()(std::get<1>(k)) ^ std::hash<T2>()(std::get<2>(k));
         }
      };

   template<typename T0, typename T1, typename T2, typename T3> struct hash<std::tuple<T0, T1, T2, T3>>
      {
      size_t operator()(const std::tuple<T0, T1, T2, T3> &k) const noexcept
         {
         return std::hash<T0>()(std::get<0>(k)) ^ std::hash<T1>()(std::get<1>(k)) ^
                std::hash<T2>()(std::get<2>(k)) ^ std::hash<T3>()(std::get<3>(k));
         }
      };
   }


#endif /* PERSISTENT_COLLECTIONS_H */
