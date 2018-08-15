ResolvedMethod is a wrapper around a Java method which exists for the duration of a single compilation. It collects and manages information about a method and related classes. We need to extend it 

On the server, we extend `TR_ResolvedJ9JITaaSServerMethod` from `TR_ResolvedJ9Method`. Upon instantiation, a mirror instance is created on the client. Instantiation happens either directly via the `createResolvedMethod` family, or indirectly using one of multiple `getResolvedXXXMethod` methods, which perform operations to locate a method of interest and then create the corresponding `ResolvedMethod`.

Many method calls which require VM access are relayed to the client-side mirror. However, some values are cached at the server to avoid sending remote messages. Since all resolved methods are destroyed at the end of the compilation, this is a good choice as a cache for data which may be invalidated by class unloading/redefinition.
