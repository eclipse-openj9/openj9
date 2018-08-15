The IProfiler is specialized for JITaaS in two classes: `TR_JITaaSIProfiler` and `TR_JITaaSClientIProfiler`. They are allocated in the method `onLoadInternal` in `rossa.cpp`.

There are two different kinds of entries that we handle: method entries and bytecode entries.

### Method entries

Holds profiling data relating to a method. This data is currently not cached. When the method `searchForMethodSample` is called on the server, it sends a message to the client. The client serializes the data using `TR_ContiguousIPMethodHashTableEntry::serialize` and the server deserializes it with `deserializeMethodEntry`.

### Bytecode entries

Profiling data for individual bytecodes. This data is retrieved during compilation using the method `profilingSample`. It is cached at the server in the client session data.

To reduce the number of messages sent, the client will sometimes decide to send the data for the entire method even when the server only asks for a single bytecode. This is done because it is likely that the server will continue on to request other bytecodes in the same method as the compilation progresses. Currently, this will happen if the method is already compiled or is currently being compiled, but otherwise not (for example, if it is being inlined early). This decision is made by the function `handler_IProfiler_profilingSample` in `JITaaSCompilationThread.cpp`.

In a debug build, extra messages will be sent to verify that the cached data is (mostly) correct. Often, the profiled counters may be slightly wrong and this can produce warnings, but they are safe to ignore as long as the cached value differs only slightly and does not appear to be corrupted. This validation is performed in `TR_JITaaSIProfiler::validateCachedIPEntry`.

Serialization is performed by `TR_JITaaSClientIProfiler::serializeAndSendIProfileInfoForMethod` and deserialization from within `profilingSample`. The code appears quite complex because there are special cases for different types of samples, but if you ignore those cases it's fairly straightforward.
