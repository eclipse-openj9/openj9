Usually the [Testarossa problem determination guide](https://lynx.torolab.ibm.com/wiki/J9_JIT_Problem_Determination_Guide_for_Java_6) can be followed for JITaaS. But there are a few JITaaS specific tips and tricks.

If the client crashes in `handleServerMessage` after receiving some bad data from the server, you will need to find out what the server was doing when it sent the message. The easiest way to do this is by running both the server and client in gdb, then sending the server a Ctrl-C after the client crashes. From the gdb prompt on the server you can type `thread apply all backtrace` and find the appropriate compilation thread to determine where the server was.

Please add to this file!!
