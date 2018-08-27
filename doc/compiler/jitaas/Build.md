There are currently 3 supported build procedures. Select the one that best suits your needs.

- [Building only the JIT](#jit). Choose this if you have no need to modify the VM or JCL. This only builds OMR and everything in the `runtime/compiler` directory. Output artifact is `libj9jit29.so`.
- [Building the whole VM](#vm). The complete build process. Takes much longer but produces a complete JVM as output (including the JIT).
- [Building in Docker](#docker). Choose this if you don't want to deal with build dependencies. Takes the longest.

Either way, you will need protobuf installed.

If you run into issues during this process, please document any solutions here.

## JIT

```
$ cd runtime/compiler
$ make -f compiler.mk -C . -j$(nproc) J9SRC=$JAVA_HOME/jre/lib/amd64/compressedrefs/ JIT_SRCBASE=.. BUILD_CONFIG=debug J9_VERSION=29 JIT_OBJBASE=../objs

```

The only new addition to the build process for JITaaS is to protbuf schema files. Thesee are supposed to be build automatically, but it keeps getting broken by upstream changes to openJ9. So, if you get errors about missing files named like `compile.pb.h`, try building the make target `proto` by adding the word `proto` to the end of the make command above.

## VM

See https://www.eclipse.org/openj9/oj9_build.html. The only difference is you need to check out the JITaaS code instead of upstream OpenJ9.


## DOCKER

Follow the [README.md](https://github.ibm.com/runtimes/Jaas_tr.open/blob/milestone_8/buildenv/docker/jitaas/jdk8/x86_64/ubuntu18/README.md) section `build/Dockerfile` for building JITaaS in Docker.
