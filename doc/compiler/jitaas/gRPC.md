The initial prototype for JIT-as-a-service used gRPC for facilitating
communication between the server and the client. The streaming API is used on
both ends because the client and server need a persistent connection for the
duration of the compilation, to send and receive an arbitrary amount of data.

**gRPC is no longer used by default. See [Sockets](Sockets.md) for details
on the new communication backend.** If you still wish to compile with gRPC
support, you can set the environment variable `JITAAS_USE_GRPC`.

## Asynchronous Streams

Streams in gRPC can be synchronous or asynchronous. Because of the nature of the
compiler, we need to communicate in a synchronous manner, i.e. a wait is needed
after every write or read on the stream to ensure the read/write is completed
before continuing. However, gRPC's synchronous streaming API provides much less
control to the application in comparison to the Async API which prevents us from
using the Sync API on the server. In brief the synchronous stream API:

1. defines its own threading model and can create an unbounded number of
threads (the compiler can however only have 7 Compilation Threads)
2. does not allow specifying a timeout after individual reads and writes
3. ends a stream when the rpc handler returns, thus making it harder to hand the
stream object to another thread.

On the client, only the second restriction truly applies. As such, the client
may use synchronous bidi streams if the ability to set timeouts per read/write
is not needed.

Reads and writes on a asynchronous stream return immediately, an entry is added
to the `CompletionQueue` for that stream whenever the read/write actually
completes. Asynchronous streams can thus be made to behave like synchronous
streams by calling `CompletionQueue::Next` or `CompletionQueue::AsyncNext` after
every read or write.

The server has a listener thread and a dispatch thread. The listener thread
waits on a single `ServerCompletionQueue` for new connections. Entries in this
queue are tagged with a stream number (from 0 to 6 for up to 7 streams). Once a
client connects the listener thread hands off a stream (based on the tag number)
to the dispatch thread. This stream has its own `CompletionQueue` and all
further communication for this RPC will create entries in this `CompletionQueue`
instead of the `ServerCompletionQueue`. For JIT-as-a-service, we use a single
RPC for a single compilation. Once the RPC is complete, the stream is renewed
and calls `RequestCompile` to let the listener thread know that it is ready to
receive more client connections.

**Side Note:** It is important for all completion queues to call `Shutdown()`
and to be drained of entries before they can be deleted. The server re-uses
queues in between compilations instead of deleting them.

## During a compilation

The client initiates a stream by sending over the initial data, which includes
the bytecode for the method to be compiled. Immediately after, the client waits
in a loop to read and respond any messages sent by the server - most of which
are requests for additional data. The client exits this loop once the server
sends over the compiled method.

On the server, once the dispatch thread acquires a compilation request, it
generates additional needed detail and places an entry in the compilation
queue. This entry contains both the `TR_MethodToBeCompiled` and the stream
objects. Once a `CompilationThread` grabs this entry off the queue, compilation
proceeds as normal - with the compiler requesting information that it does not
have using the stream object's `write()` and `read()` functions and sends over
the final compiled body using `finishCompilation()`.

If in the middle of a compilation the stream is temporarily broken, the
compilation is terminated.
