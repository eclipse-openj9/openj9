# Sockets

Client server communication via TCP sockets is the default communication backend. The implementation mostly resides within the `rpc/raw` directory. There is a base class `J9Stream`, which is specialized for both the client and server in `J9ClientStream` and `J9ServerStream`.

Encryption via TLS (OpenSSL) is optionally supported. See [Usage](Usage.md) for encryption setup instructions.

If a network error occurs, `JITaaS::StreamFailure` is thrown.

### `J9Stream`
Implements basic functionality such as stream initialization (with/without TLS), reading/writing objects to streams, and stream cleanup.

### `J9ClientStream`

One instance per compilation thread. Typically, this is only interacted with through the function `handleServerMessage` in `JITaaSCompilationThread.cpp`. An instance is created for a new compilation inside `remoteCompile`, and the `buildCompileRequest` method is then called on it to begin the compilation.

### `J9ServerStream`

There is one instance per compilation thread. Instances are created by `J9CompileServer::buildAndServe`, a method which loops forever waiting for compilation requests and adding them to the compilation queue using the method `J9CompileDispatcher::compile`.

Accessible on a compilation thread via `TR::CompilationInfo::getStream()`, but beware that a thread local read is performed, so try to avoid calling it in particularly hot code.

Also stores the client ID, accessible via `J9ServerStream::getClientId`.
