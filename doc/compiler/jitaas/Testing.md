These are the steps to run the tests on your machine.

1. Compile a debug build of openj9 from scratch. To do this pass the flag `--with-debug-level=slowdebug` to `configure`.
2.  Go to the libraries directory:
   ```
   cd $SOME_DIR/openj9-openjdk-jdk9/build/linux-x86_64-normal-server-slowdebug/jdk/lib/compressedrefs
   ```
3. Copy all of the missing libraries into the SDK you are normally using:
For example, I copy them into `/opt/IBM/JDK/xa6480sr6/jre/lib/amd64/compressedrefs`.

    Missing libraries (might be more, will add them to this list, if found):
   - libj9ben.so
   - libjvmtitest.so
   - libgptest.so
   - libsoftmxtest.so
4. Compile  the tests (only need to compile once):
   ```
   cd $OPENJ9_DIR/openj9/test/TestConfig
   export JAVA_VERSION=SE80
   export JAVA_BIN=/your/sdk/jre/bin
   export SPEC=linux_x86-64_cmprssptrs
   make -f run_configure.mk
   make compile
   ```
5. Run the tests! First start a server, then do
   ```
   make _sanity EXTRA_OPTIONS=" -XX:JITaaSClient "
   ```
   NOTE: It's important to put spaces before and after `-XX:JITaaSClient`, otherwise
   some tests will not run properly.
6. To rerun the failed tests, run
   ```
   make _failed EXTRA_OPTIONS=" -XX:JITaaSClient "
   ```
   which tests will be rerun can be modified in `failedtargets.mk`.

   or, if you want to run an individual test, run
   ```
   make <name_of_the_test> EXTRA_OPTIONS=" -XX:JITaaSClient "
   ```
