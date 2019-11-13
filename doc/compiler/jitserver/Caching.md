<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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
# Caching in JITServer
Caching is an important aspect of `JITServer` and one of the main reasons for why `JITServer` client consumes less CPU time compared to local compilation. In this document I will explain why caching is important, what kind of entities we cache, and how some specific caches work.

# Why caching is important
When server is compiling a method, it needs to know a lot of information that is located on the client side, e.g. the addresses of RAM classes, GC mode, class hierarchy information, interpreter profiling data and much more. To obtain this information, server will make a remote call over the network to the client, client will find it, and send it back to the server. During the course of a compilation server will make hundreds of such calls (on average), assuming no caching is involved.
Why is it bad? First of all, remote calls are expensive, both in terms of latency and CPU, especially in terms of CPU. If the server makes hundreds of requests to the client just for one compilation, client will spend more CPU time just sending and receiving data over the network than it would take for it to compile a method locally. What makes it worse is that in order for the client to fetch the requested data it needs to do some work, which additionally increases CPU consumption.
Caching helps alleviate this problem by storing the result of frequent remote calls on the server, so that if the same data is needed again, it can accesss it from the cache, instead of making a remote call.

# Types of caching
There are 2 types of caching that `JITServer` does:
- Global (in persistent memory) and per-compilation (on the heap). 
Global caching (in persistent memory) is done for entities that will not change (or very unlikely to change) over the lifetime of a client JVM, e.g. GC mode, IProfiler data for compiled methods, class hierarchy.
- Local caching (on the compilation heap) is done for entities that are not going to change during the current compilation, but might change in-between compilations or are just unique for each compilation, e.g. resolved methods are created anew for each compilation. We also use local caching for entities that can change, but are unlikely to do so during the limited life span of the current compilation, e.g. IProfiler data for interpreted methods. Since method is still interpreted, new profiling data might be added, but it's unlikely to change significantly enough to affect performance over the duration of the current compilation.

Both types of caching are done on per-client basis, that is, if multiple clients are connected to the same server, they will not share caches, as that would make entities very complicated.
Whenever possible, caching should be done globally, because hit rates will be higher, but one should be careful and make sure that the client data will not actually change.

# Important caches
## `ClientSessionData`
Stores all of the globally cached data; for a detailed description read [this](ClientSession.md).
## `CompilationInfoPerThreadRemote`
Most of the per-compilation caches are stored in `CompilationInfoPerThreadRemote`. Since most caches have very similar structure, i.e. hash map, we added templated methods for working with these caches. The methods are `initializePerCompilationCache`, `cacheToPerCompilationMap`, `getCachedValueFromPerCompilationMap`, `clearPerCompilationCache`. If a new cache is added, it should use these methods. At the end of compilation all local caches need to be reset by adding a call to `TR::CompilationInfoPerThreadRemote::clearPerCompilationCaches`.
Some important per-compilation caches:
### `TR_ResolvedMethodInfoCache`
This cache stores pointers to resolved methods created for the current compilaton. It is important, because messages requesting the creation of resolved methods are some of the most frequent messages. Unfortunately, persistent caching does not seem possible, since every compilation creates its own resolved methods.
### `IPTableHeap_t`
This cache stores IProfiler data for methods whose profiling data might get updated. It is one of the two caches used by IProfiler.
- Per-compilation cache: stores data for interpreted methods and a method currently being compiled. It is a nested hash table. The outer cache uses `IPTableHeap_t` hash table type, and takes `J9Method *` as a key. The inner table uses `IPTableHeapEntry`, and takes bytecode index as a key, and stores profiling data as a value.
It is possible for this cache to contain outdated profiling data, because IProfiler might collect additional data on the client after we cache it. However, the lifetime of a compilation is pretty short, so using suboptimal profiling information does not affect performance.
- Persistent cache: stores data for already compiled methods, because their interpreter profiling data will definitely not change.
Uses a slightly different hash table type `IPTable_t`, because it's located inside entries of `J9MethodInfo`, so it only takes
bytecode index as a key.
## `TR_ResolvedJ9JITServerMethod`
Some per-compilation caches are stored inside resolved methods, they store results of some resolved method query. These caches do not need to be explicitly cleared, since resolved methods only exist for the duration of one compilation. I am not sure if it is a good thing to have local caches spread out across 2 files, maybe it would be a good idea to move all caches from resolved method to `CompilationInfoPerThreadRemote`.

