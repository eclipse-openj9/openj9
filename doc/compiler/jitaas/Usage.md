JITaaS adds an additional two *personas* to the `java` executable: client mode and server mode. There are two new command line options that can be used to select a persona: `-XX:JITaaSServer` and `-XX:JITaaSClient`.

In server mode, the JVM will halt after startup and begin listening for compilation requests from clients. No Java application is given to the server on the command line.

```
$ java -XX:JITaaSServer

JITaaS server ready to accept incoming requests
```

Clients can then be started. For this example, we assume the client and server are running on the same machine. The server needs to continue running, so you will need to start clients in a new terminal.

```
$ java -XX:JITaaSClient -version
java version "1.8.0"
Java(TM) SE Runtime Environment (build 1.8.0-foreman_2018_06_05_16_31-b00)
IBM J9 VM (build 2.9, JRE 1.8.0 Linux amd64-64-Bit Compressed References 20180616_389602 (JIT enabled, AOT enabled)
OpenJ9   - 83fa164
OMR      - 6b22c53
IBM      - d207010)
```
Note that `java -version` is itself a small Java application! If you have some application you'd like to test, you can substitute it in for `-version`. Run it as usual, but with the flag `-XX:JITaaSClient` before the application name.

You might have noticed that running the client without any server to connect to still appears to work. This is because the client will fall back to running the interpreter (slow path) if it cannot connect to a server. To ensure that everything is really working as intended, it is a good idea to enable some logging. It's often most convenient on the server side, because log messages will not interfere with application output, but logging can be added to either the server or the client.

With verbose logging, if a client connects successfully then server output should look something like this:
```
$ java -XX:JITaaSServer -Xjit:verbose=\{JITaaS\}

#JITaaS: JITaaS Server Mode. Port: 38400
#JITaaS: Started JITaaSServer listener thread: 000000000064DD00 
JITaaS server ready to accept incoming requests

#JITaaS: Server allocated data for a new clientUID 7273413081043723869
#JITaaS: Server received request to compile sun/reflect/Reflection.getCallerClass @ cold
#JITaaS: Server queued compilation for sun/reflect/Reflection.getCallerClass
#JITaaS: Server cached clientSessionData=00007FFFDA1973E0 for clientUID=7273413081043723869 compThreadID=0
#JITaaS: Server has successfully compiled sun/reflect/Reflection.getCallerClass()Ljava/lang/Class;
#JITaaS: Server received request to compile java/lang/Double.longBitsToDouble @ cold
#JITaaS: Server queued compilation for java/lang/Double.longBitsToDouble
#JITaaS: Server cached clientSessionData=00007FFFDA1973E0 for clientUID=7273413081043723869 compThreadID=1
#JITaaS: Server has successfully compiled java/lang/Double.longBitsToDouble(J)D
#JITaaS: Server received request to compile java/lang/System.getEncoding @ cold
...
```

### Configuration

#### Hostname
On the client, you can specify the server hostname or IP.
```
$ java -XX:JITaaSClient:server=example.com
```

#### Port
By default, communication occurs on port 38400. You can change this by specifying the `port` suboption as follows:
```
$ java -XX:JITaaSServer:port=1234
$ java -XX:JITaaSClient:port=1234 MyApplication
```

#### Timeout
If your network connection is flaky, you may want to specify a timeout. By default there is no timeout. Timeout is given in milliseconds. Client and server timeouts do not need to match, but there's usually no reason for them to differ.
```
$ java -XX:JITaaSServer:timeout=5000
$ java -XX:JITaaSClient:timeout=5000 MyApplication
```

#### Encryption (TLS)
By default, communication is not encrypted. If messages sent between the client and server need to traverse some untrusted network, you may want to set up encryption. Encryption reduces performance, so consider whether it is required for your use case.

Encryption support is currently experimental. **The implementation has not yet undergone a security audit.** Additionally, only certain modes of operation are supported. If you require encryption but run into problems with the current implementation, then please consider opening an issue with your requirements.

To generate some keys for testing, try:
```
$ openssl genrsa -out key.pem 2048
$ openssl req -new -x509 -sha256 -key key.pem -out cert.pem -days 3650
```
Make sure you specify localhost for the common name, but other fields don't matter (they can be left blank). **Keys generated in this manner should NOT be used in production!**

On the server, there are two options `sslKey` and `sslCert`, which are used to specify the filenames of PEM-encoded keypair.
```
$ java -XX:JITaaSServer:sslKey=key.pem,sslCert=cert.pem
```
The client currently accepts a single option `sslRootCerts`, which is the filename of a PEM-encoded certificate chain.
```
$ java -XX:JITaaSClient:sslRootCerts=cert.pem -version
```
