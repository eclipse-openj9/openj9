<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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

# JITServer CHTable design and implementation notes

To begin, this document will describe the vanilla (non-JITServer) implementation of the CHTable, then will go on to describe the
changes made to support JITServer.

## Vanilla CHTable

The CHTable (Class Hierarchy Table) is used to perform codegen based on assumptions about the structure of an application's
class hierarchy. It's split into two (perhaps poorly named) components: the `TR_CHTable` and the `TR_PersistentCHTable`. The
CHTable can be disabled using the JIT option `disableCHOpts`. It is not used for AOT except under rare circumstances.

The `TR_CHTable` (found in `env/CHTable.{hpp,cpp}`) isn't really a class hierarchy table, but instead stores information about
which assumptions have been made during the codegen of a particular method. However, not all of that information is stored in the
`CHTable`. Some of it (virtual guards and side effect guard patch sites) is instead stored in the `Compilation` object, for
reasons which are unclear since they are only ever used by the CHTable. At the end of a compilation, `TR_CHTable::commit` is
called by the compilation thread. The commit operation verifies that all the assumptions made during the compilation still hold,
and if so, it converts them into runtime assumptions which are stored in the method metadata. These assumptions generally
cause a recompilation if they are violated, but can sometimes be fixed by patching the affected method. If any assumptions
have been violated during the compilation before the commit operation, the compilation will be failed.

The `TR_PersistentCHTable` (found in `env/PersistentCHTable.{hpp,cpp}`) is where the class hierarchy is actually stored. There
is a single global instance of it stored in the persistent info, which is created in rossa.cpp. It
contains a hash table holding `TR_PersistentClassInfo` for all loaded classes, keyed by the corresponding `OpaqueClassBlock*`.
The persistent class info holds a list of subclasses as well as various other flags and metadata. It also optionally holds a
`TR_PersistentFieldInfo` for each field in the class, but that is only set by class lookahead, which is disabled under normal
circumstances. The PersisitentCHTable has methods to

- directly find info about a specific class (`findClassInfo[AfterLocking]`), which are dangerously low-level and should probably
be removed (details provided later).
- query specific properties of a class (`findSingleInterfaceImplementor`, `isKnownToHaveMoreThanTwoInterfaceImplementers`, etc).
- update the table with info about newly loaded/unloaded classes (`classGotLoaded`, `methodGotExtended`, etc).
- `commitSideEffectGuards`, which is related to the commit operation of `TR_CHTable` and seemingly has no good reason
to be a member of `TR_PersistentCHTable` instead of being along with the rest of the commit functionality in `TR_CHTable`.
- update the `visited` flag on classes (`resetVisitedClasses`), which is used in `TR_CHTable` for building up lists of classes
and iterating over them, and again should probably be in that class instead.

Most of the operations performed on the `TR_PersistentCHTable` require the class table mutex to be held. The class hash table is
implemented in a peculiar way, where `TR_PersistentClassInfo` inherits from `TR_Link0<TR_PersistentClassInfo>` (CRTP in action!),
effectively making every `TR_PersistentClassInfo` object into a linked list. This object cannot be represented without being in a
linked list! These linked lists are only used in the existing code to store entires within a bucket of the hash table. This is
part of why I claim `findClassInfo` is dangerously low level - it exposes the internal structure of the hash table in an
extremely unusual way, in which it could easily be mistaken by a programmer for being a different data structure. Use caution
when working with this code.

## JITServer CHTable

4 new files were added:

- env/JITServerCHTable.{hpp,cpp}
- env/JITServerPersistentCHTable.{hpp,cpp}

The architecture of CHTable functionality remains similar with JITServer support, with `env/PersistentCHTable.{hpp,cpp}` containing
`JIT{Server,Client}PersistentCHTable`, and `env/JITServerCHTable.{hpp,cpp}` containing commit related functionality. Some changes
were made elsewhere in the codebase to accommodate for JITServer support.

In `rossa.cpp`, `TR_PersistentCHTable` allocation was modified so that the correct JITServer subclass is allocated when in JITServer mode.
The reason why a client override is necessary is due to the heavy use of caching by the JITServer CHTable implementation. The server
holds a cache of each client's `TR_PersistentCHTable`, which is updated with a delta once per compilation. The cache is
initialized with any classes that are loaded early on upon the first interaction with the CHTable. To send the delta, the client
needs to keep track of what changes have been made to the table since the last delta was sent. `JITClientPersistentCHTable`
simply keeps track of changes while deferring functionality to the base class. (De)serialization of deltas is performed by methods
matching `[de]serialize*` in `env/JITServerPersistentCHTable.cpp` and is currently done manually by writing data into a string because
the new tuple serializer had not yet been written at the time (TODO: use the tuple serializer). This is also where the other issue
with `findClassInfo` shows up - we don't know if the class info will be modified following a call to `findClassInfo`, so we have
to conservatively assume it will be modified, which may lead to larger than necessary deltas. These deltas are requested by the
server before within `wrappedCompile` before codegen starts.

Since different clients can have different class hierarchies, a server needs to keep a seperate cache for each client. This is
done by associating caches with client IDs within the `JITServerPersistentCHTable`. The server persistent CHTable overrides
the `findClassInfo` methods to return cached class info, which gets the other query methods working for free. The table update
related methods will never be called on the server since it never loads classes, so we ignore them. The server keeps track of the
last time it contacted each client, and will free the associated memory if it hasn't heard from a client in over 5 minutes.

Normally, the `commit` operation is performed by the compilation thread following a compilation. For JITServer, we cannot perform it in
the same place because creating runtime assumptions on the server is meaningless; the server never loads any class and thus would
not have anything to check the assumptions against. So, we need to perform the commit on the client, using the data generated by
the server during compilation. However, the point at which the server would normally call `commit` is before the client has loaded
and relocated the code, which prevents the runtime assumptions from being relocated. But, the client loads and relocates the code
after the final message sent by the server. So we have to send the data required to perform the commit along with the final
message and have the client perform the commit after loading the code. This is implemented in `JITServerCompilationThread.cpp`. The
call to `computeDataForCHTableCommit` is performed on the server and the data is transferred to the client where
`JITClientCHTableCommit` is called.

Because the vanilla `commit` code uses a bunch of linked lists of complex types including symbol references (which only exist on
the server) and protobuf gives us vectors when deserializing, the commit code has been duplicated to make use of stripped down
data types which only include relevant information. This is unfortunate, but it would be rather complex to support a single
templated implementation which handles both data formats, and it would be inefficient to convert between the two data formats. So
for this "protoype" CHTable implementation, code duplication is the only reasonable way to make things work. Still, the data is
quite complex, so the serialization infrastructure was upgraded to support arbitrarily nested tuples and vectors of data.
