<!--
Copyright IBM Corp. and others 2025

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->


# The JITServer AOT cache

The JITServer AOT cache is a feature of the JITServer that allows a server to share AOT-compiled code with multiple client JVMs. This sort of sharing can save resources at the server, since it can serve cached data in response to a request instead of computing it on-demand, and can as a consequence improve server response times and client performance. The JITServer AOT cache relies on the existing AOT code relocation infrastructure, so we will go over the relevant properties of that mechanism in the next section, before moving on to an overview of the different components of the JITServer AOT cache.

For more information, the [AOT documentation](/doc/compiler/aot/README.md) is a useful guide to AOT compilation in general. The initial JITServer AOT cache design was described in the paper [JITServer: Disaggregated Caching JIT Compiler for the JVM in the Cloud](https://www.usenix.org/conference/atc22/presentation/khrabrov) by Khrabrov et al, though the implementation details have changed somewhat over time. Also, see the [diagram below](#high-level-aotcache-diagram) to understand how all the relevant data structures fit together.

## Local AOT compilation

In local AOT compilations, *relocation records* are created for all the dynamic JVM information that is relevant to the JIT-compiled code and are stored along with the code itself in a local shared class cache (SCC). These records are used in subsequent JVM runs when loading this stored code to verify that assumptions made during code gen still hold. They are also used to update memory addresses and other runtime-dependent information in the code itself so they refer to the correct entities in the new JVM environment.

The most difficult task in relocating compiled code (and the case most relevant for the AOT cache) is finding a `J9Class` in the new JVM performing the load that matches a particular `J9Class` as it existed in the old JVM that generated the AOT code. (Here, "matching" means something like "can be used as a substitute for the old class in the compiled code so that the updated code computes its associated method in the new JVM environment correctly"). The relocation runtime will check that a new class matches by verifying that its *class chain* is equal to the class chain of the old class - see the AOT documentation for how class chains work. It usually gets a candidate class in one of two ways:

1. A series of SVM validations repeated some queries (class at CP index, class by name in defining class loader of already-acquired class, that sort of thing) recorded at compile in the new JVM and has come up with a `J9Class` and associated it to a particular SVM ID.
2. The class was heuristically identified by recording its class chain and the class chain of the first-loaded class of its defining class loader, and the relo runtime got a class loader from the `TR_PersistentClassLoaderTable` and looked up a `J9Class` by name in that loader.

To save space, the relocation records of locally-compiled AOT methods refer to a lot of data, including the data necessary to look up classes, via *offset* into the local SCC. An offset is a `uintptr_t` value that can be (and indeed should be if you're not working on low-level implementation details) considered an opaque "key" that can be used to retrieve data from a particular local SCC. This data is stored in the local SCC during the course of an AOT compilation and the offsets to that data are stored in the relocation records. During a load, the relocation runtime uses the stored offsets to get the associated data from the local SCC, and then uses this data to reconstruct or otherwise look up what it needs to continue relocation.

## Non-shared JITServer AOT compilation

Traditionally, the JITServer compiled AOT code on a per-client basis, and required a local SCC in order to perform these compilations. Whenever a relocation record needed a client offset, it would send the data to the client, have it store the data in its local SCC, get the offset to the data back, and store that offset in the relocation record. The resulting compiled method could be sent to the client and stored in its local SCC without issue, and the client could relocate it normally with its relo runtime and local SCC. This is still what happens when the client does not request that the compilation involve the JITServer AOT cache.

## Serialization records and the problem of sharing AOT code between JITServer clients

The main barrier to sharing AOT code between different JITServer clients with different local SCCs is the offset problem - the relocation runtime of a client must be able to use the offsets contained in the relocation records to look up the data associated to those offsets. If these offsets were somehow assigned deterministically, the AOT cache at the server could just store the compiled code and all data associated to the offsets in its records. It could then send the code and any missing data to the client in response to a compilation request, so the client could relocate the code just like a non-shared JITServer AOT compilation. However, these offsets are not deterministic, so clients with different local SCCs may have conflicting data stored at a particular offset, and so this approach cannot work. Even if the offsets were deterministic, we would also like clients without a suitable local SCC to be able to take advantage of a server's AOT cache. Thus a different approach is required.

To resolve the offset problem, during the course of a compilation intended to be stored in the JITServer AOT cache the server will build up a collection of records that do two things:

1. Specify every place in a method's relocation records (via offset from the start of the relo data) where an SCC offset might be used
2. Specify, for every SCC offset, enough data that we can reconstruct what information that offset refers to

These records are the JITServer AOT cache serialization records, and they act as relocation records for (normal, local AOT) relocation records. They are stored alongside the cached method in the JITServer AOT cache and sent to clients when an AOT cache load is requested. A client will use these serialization records to find the data it needs in the running JVM and update the SCC offsets in the relocation records of the method. This is called *deserialization*, and is performed by the `JITServerAOTDeserializer`. The method and its updated records can then be given to the relocation runtime, which can relocate the method as usual.

Crucial to understanding the structure of the serialization records and the process of deserialization is the fact that the relocation runtime does not care at all about the actual data stored in the local SCC that corresponds to these offsets. What it does is use the API of the `TR_J9SharedCache`, together with the relo record SCC offsets and the results of previous calls to that API, to materialize the individual dynamic JVM entities that it actually cares about. These will be the concrete candidate `J9Class` or `J9Method` objects that it needs to continue relocation. The serialization records are an SCC-independent encoding of the process the relocation runtime uses to get these candidates, a process that is then effectively duplicated by the deserializer.

## Deserializing without a local SCC present

The current default implementation of the `JITServerAOTDeserializer` API is the `JITServerNoSCCAOTDeserializer` class. (It can be explicitly enabled with the command-line option `-XX:+JITServerAOTCacheIgnoreLocalSCC`). In this implementation, the deserializer at a client will ignore any local SCC currently in use (if one is even set up) and will use the serialization records of a method received from the server to look up candidate dynamic JVM entities associated to those records. These entities are then cached, and in this way the deserializer will build up an association between individual serialization records (represented by their identifier and type encoded as a `uintptr_t` ID) and their corresponding dynamic entities. Since this implementation ignores the local SCC (if it is even present), it will replace the offsets in the relocation records with the IDs of their associated serialization records.

Since the offsets in the relocation records will, after deserialization, be totally unrelated to the any local SCC that might exist, the client will hand to `prepareRelocateAOTCodeAndData` an override for the `TR_J9SharedCache` interface that is stored in the `TR_J9VMBase` frontend for that particular compilation thread. This override will be a `TR_J9DeserializerSharedCache`; its methods use the offsets in the relocation records (which are now encoded serialization record IDs) and the cache maintained by the deserializer to satisfy the relocation runtime's requests.

The JITServer itself also exploits the fact that the deserializer caches the results of deserializing individual records; it will keep track of the records sent to particular clients and avoid sending the same record twice to a client.

## Deserializing with a local SCC present

The original implementation of the `JITServerAOTDeserializer` API uses the local SCC to function. This implementation still exists as the `JITServerLocalSCCAOTDeserializer` class, and can be enabled with the option `-XX:-JITServerAOTCacheIgnoreLocalSCC` (off by default). This implementation deserializes serialization records much like the `JITServerNoSCCAOTDeserializer` does, except that it will look up (or store) the persistent representation of the dynamic JVM entities it finds for particular serialization records. It will use the offsets to that data to update the relocation records of methods, and the relocation runtime will be able to use the updated records with the default `TR_J9SharedCache` interface to relocate the AOT code. For the sake of efficiency, this implementation also maintains a map from serialization record IDs to local SCC IDs, so the JITServer can still avoid sending duplicate records to clients.

## Compiling methods for the JITServer AOT cache

Clients of a particular JITServer will decide whether or not a compilation request will involve the server's AOT cache. These clients will also, by default, ignore any existing local SCC for the duration of these compilations. (This behaviour is controlled by the same `-XX:[+|-]JITServerAOTCacheIgnoreLocalSCC` option that controls whether or not the local SCC is ignored during deserialization and relocation). Compilations that get stored in a server's AOT cache in this default scenario generally proceed like this:

1. The client will determine in `TR::CompilationInfoPerThreadBase::preCompilationTasks` if a particular method is a candidate for an AOT cache compilation.
2. The client will perform some additional checks in `remoteCompile()` before indicating in the compilation request that the AOT cache should be used. The client can specify separately that the request should be AOT cache store or load, and specify the name of the server cache to be used.
3. If the server gets an AOT cache request, it will attempt to load a copy of the method from cache named in the request if it is permitted to do so.
   1. If the server gets a cache hit, it will send the stored code and serialization records to the client and end the compilation. The client will deserialize and relocate the method as described above.
   2. Otherwise, if the method can't be found it will set up an AOT cache store compilation (which behaves mostly like a remote AOT compilation not intended for the AOT cache) and continue the compilation.
4. Whenever the server asks the `TR_J9JITServerSharedCache` for the offset of the persistent data for a JVM entity, it will look up (or create) the serialization record for that entity, get its record ID, and return that as the "offset" to the data. None of this requires any communication with the client. Indeed, the server must be careful not to request any SCC information from the client; local overrides will usually involve a test of the `ClientSessionData::useServerOffsets()` property.
5. Whenever the client handles a message from the server, it will be careful never to use its local SCC in any way. This requires a few overrides throughout the code, which will generally be preceded by a test of the `TR::Compilation::ignoringLocalSCC()` property.
6. As the server initializes the relocation records at the end of the compilation, it will note the offsets into the relocation data at which all the individual SCC "offsets" (really serialization record IDs) are referenced. It will group those relo data offsets together by SCC offset, and store all of that in a `SerializedSCCOffset` that the client deserializer can use to update the SCC offsets in the method's relocation records. It will also keep a list of all serialization records needed for the method. Note that it's the individual relocation record setter methods in `RelocationRecord.cpp` (e.g., `TR_RelocationRecordInlinedMethod::setRomClassOffsetInSharedCache()`) that contain the code that does all of this (via methods like `addClassSerializationRecord()`).
7. The server will store the compiled code, serialized SCC offsets, and serialization record IDs together in the named AOT cache, and will also send that and the serialization records themselves to the client to be deserialized and relocated.

If anything involving the JITServer AOT cache fails (e.g., a serialization record can't be created or stored) then the entire compilation must fail. (It may seem obvious that the whole compilation must fail, but if the client is not ignoring the local SCC then the compilation can continue as I mention below). The server can also use flags sent in the `AOTCache_failure` message to indicate that it will never be able to fulfil an AOT cache store or load request.

If the client is not ignoring its local SCC, then an AOT cache store or load will follow roughly the same procedure. Some differences between the not ignoring and ignoring cases include:

1. The criteria for a method being eligible for an AOT cache compilation are stricter, because the method must also be eligible for a local AOT compilation too; when ignoring the local SCC we also ignore SCC-specific eligibility criteria.
2. If the AOT cache fails during the compilation then the server can simply stop using the AOT cache and continue the compilation as an ordinary remote (non-cached) AOT compilation. This can't happen when ignoring the local SCC, because non-cached remote AOT requires that a local SCC be present to function.
3. The local SCC offsets from the client are used in the relocation records of the method, so at the end of the compilation the client will treat the received method as a remote non-cached AOT compilation, more or less.

## The individual serialization record types

The serialization records - the static, immutable records that are stored in the server's caches and sent to clients - are defined in `JITServerAOTSerializationRecords.hpp`. There are also "dynamic" versions used by the server, which wrap these static records (e.g. the dynamic `AOTCacheClassLoaderRecord` wraps the static `ClassLoaderSerializationRecord`) and are more convenient to work with at the server. Each serialization record has an associated type (enumerated in `AOTSerializationRecordType`), and these types are (for the most part) a way of adding type information to SCC offsets; they correspond roughly to the type of thing the relo runtime wants to be able to look up in the JVM during a load given an offset of that type. The records are:


1. `ClassLoader`. The dynamic JVM entity associated to it is a `J9ClassLoader`. This record identifies a loader using the name of the first class it loaded, which is *very* heuristic, and when the record is deserialized into a `J9ClassLoader`, that loader will not be guaranteed to resemble the compile-time loader much at all. The deserializer gets a candidate for this record by looking one up in the persistent class loader table by name.
2. `Class`. The dynamic JVM entity associated to it is a `J9Class`. This record identifies a class using the serialization record ID of the `ClassLoader` record for its defining class loader, the name of the class, and the sha256 of (a normalized representation of) its `J9ROMClass`. The deserializer gets a candidate for this record by looking up the class by name in the class loader that was cached for the loader record ID, and then checking that the hashes of the two classes' ROM classes match. See the `JITServerHelpers::packROMClass()` method for the details of ROM class packing - we need to transform the ROM class before hashing because the ROM class both contains too little information (it can omit unicode strings) and too much information (it can contain intermediate class info, method debug info - these things are irrelevant to the JIT, so stripping them out improves `Class` deserialization and improves AOT cache deserialization success rates).
3. `Method`. The dynamic JVM entity associated to it is a `J9Method`. This record identifies a method using the serialization record ID of the `Class` record for its defining class, and the index of the method in its defining class. The deserializer gets a candidate by looking up the method by index in the `J9Class` that was already cached for the class record ID.
4. `ClassChain`. The dynamic JVM entity associated to it is a "RAM class chain", which is a vector whose first entry is a `J9Class *`, and whose subsequent entries are the entire class/interface hierarchy of that first class. (These are in the same order as local SCC class chains). This record identifies this chain with a list of the `Class` serialization record IDs for these classes. The deserializer resolves this record by retrieving the already-cached classes for those class record IDs and making sure that the resulting chain is equal to the actual chain of the first class. You can think of this record (and its dependencies) as being a JVM-independent arbitrary class validation record.
5. `WellKnownClasses`. This record type is not associated to a particular JVM entity or relo data offset. Rather, it is a list of `ClassChain` record IDs that is used to reconstruct the well-known classes chain of an AOT method compiled with SVM.
6. `AOTHeader`. This record type is not associated to a particular JVM entity or relo data offset. Rather, it records the entire `TR_AOTHeader` of a method to check for compatibility during deserialization.
7. `Thunk`. This record type is not associated to a particular relo data offset. Rather, it records an entire J2I thunk referenced in the compiled code of a method so the client can relocate and install it during deserialization. The thunk will then be available for the relo runtime to find during relocation.

The relo record types are partially ordered by dependency according to the rules:

- `ClassLoader < Class < ClassChain < WellKnownClasses`
- `Class < Method`

The types are otherwise incomparable (including with themselves, so a class record never depends on a class, for instance). Serialization records are stored and processed in dependency order (with `ClassLoader` first) so that the dependencies of records will be available when processing those records.

If you want to cache new things at the server, one approach would be to create one or more new entries in `AOTSerializationRecordType` and new kinds of serialization record that contain enough JVM-independent information (possibly helped by referencing other serialization records) that you can find candidates for those things at a client. You will also need to add handling for the new records by: defining a dynamic wrapper for it at the server; creating and caching the record at the server; adding handling for it in the AOT cache persistence mechanism; making sure it's listed in the record IDs of cached AOT methods when they need it; deserializing and caching the result at the client; recording the relo data offsets at the server to any new SCC offsets in new relo records (if applicable). A simple example of this process is the `Thunk` record type, which added support for J2I thunk caching at the server, since it is not associated with any SCC offset and has no dependencies.

## Deserializer invalidation, resetting, and re-caching

Since the client's deserializer builds up various maps associating serialization records to JVM entities, it must also on occasion invalidate entries in these maps. Cache invalidation must happen at three times at the moment:

1. When a `J9ClassLoader` is being unloaded, the deserializer is notified in `jitHookClassLoaderUnload()` and will remove the entries linked to that loader from its cache.
2. When a `J9Class` is being unloaded or redefined, the deserializer is notified in `jitHookClassUnload()` and `jitClassesRedefined()` respectively and will remove the entries linked to that class from its cache. It will also remove all the entries linked to the methods of that class at the same time.
3. When a client loses connection to a server and connects to a new one, the deserializer must purge its caches completely. This must be done because, like the local cache, the serialization records and record IDs are valid only with respect to the AOT cache of a particular server, and so the now-stale data cannot be kept around.

It is worth going into a little more detail about (3), because the current implementation is a little subtle. We would like the client to be able to keep and use its existing deserializer data up to the moment when it knows that it will start communicating with a new server with a different AOT cache. The client first registers this fact when one of its compilation threads gets the `getUnloadedClassRangesAndCHTable` message from the server, and calls the `JITServerAOTDeserializer::reset()` method to purge the deserializer's caches. (The server sends this when it receives the first compilation request from a new client). This method also sets a flag in every compilation thread that indicates that deserializer reset has occurred; it does this because other threads may be using the deserializer concurrently (to deserialize or relocate methods) and they must be notified of the reset so they can abort their current operations (and potentially the entire compilation). You will notice that most public deserializer operations follow a pattern of first acquiring one of the deserializer's locks and then immediately checking `JITServerAOTDeserializer::deserializerWasReset()`. This is how the other threads detect a concurrent reset. The only operations that do not need to do this are ones that are guaranteed never to happen concurrently with a reset; for now, the only examples are the ones that invalidate cache entries, because these can only be run when there are no other compilation threads active. Finally, the per-thread reset flag needs to be cleared using `JITServerAOTDeserializer::clearDeserializerWasReset()` just before a new remote compilation starts, so new resets can be detected. The placement of that method call is also precise; the note next to where it is used goes into the details.

The old `JITServerLocalSCCAOTDeserializer` does cache invalidation (but not resetting) slightly differently, so it can support re-caching. Since that implementation also stores the local SCC offsets corresponding to particular serialization records, it only needs to throw away the pointers to the dynamic entities themselves (the `J9Class` and `J9ClassLoader` pointers) and not the entire cached entries; the local SCC offsets will never be invalidated. When a new method is being deserialized, it can then attempt to use the information in the SCC corresponding to these offsets to find new pointers to cache and so restore the complete entry. The new `JITServerNoSCCAOTDeserializer` does not keep enough information around to re-cache invalidated entries. It could be modified to do so, or to tell the server which entries were invalidated so it would know to send those serialization records to the client again for caching and not continue to skip sending them. (The benefits of supporting this were unclear when the `JITServerNoSCCAOTDeserializer` was being implemented).

## AOT cache persistence

This feature of the JITServer AOT cache, controlled at the server with the `-XX:[+|-]JITServerAOTCachePersistence` option, enables the server to save its caches to disk and to load caches from disk. It is intended to support deployments that use the AOT cache and also expect to start and stop JITServer instances with some regularity (cloud autoscaling, for instance), by letting subsequent server instances benefit from the AOT caches built up by previous instances.

To save its caches, the server will save each named AOT cache at a configurable time interval to a single file in a specified directory. Specifically, it saves a header describing the cache and every serialization record (not the fully dynamic `AOTCacheRecord`) in the maps of that cache. To support multiple servers using the same cache files at the same time, if a cache file already exists the server will peform some basic checking to see if its own cache is "better" than what's already there. It will skip the update if its cache isn't better, and otherwise will write out its entire cache to disk again and replace the existing file.

Loading from a cache file will be triggered when a server receives an AOT cache compilation request for a cache that isn't currently loaded. If the server can find a cache file with that name, it will trigger the asynchronous loading of that cache. During this process, the serialization records will be re-linked into full `AOTCacheRecord`s.

One implementation quirk to note is that a dummy compilation request is used for both the saving and loading of caches. This is done so that a single server compilation thread (and not the one that received an AOT cache request, notably) will be assigned to perform the persistence operation.

## High Level AOTCache Diagram

![Figure 1. AOTCache and related data structures](Datastructures.png)
