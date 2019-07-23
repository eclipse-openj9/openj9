#!/bin/bash -x

export LD_LIBRARY_PATH=/usr/local/cuda/nvvm/lib64:/usr/local/cuda/lib64

JAVAC="${JAVA_HOME}/bin/javac"
JFLAGS="-J-Xcompressedrefs -J-Xnojit -g"
J9_EXEC="${JAVA_HOME}/jre/bin/java"
JITFLAG="{*.runGPULambda*}(traceFull),enableGPU={details|verbose|enforce},disableAsyncCompilation,dontInline={*.runGPULambda*}"
J9_FLAGS="-Xcompressedrefs -XX:-EnableHCR -Xjit:${JITFLAG},log=test.trc"

CLASS=VecAdd
echo compiling ${CLASS}
${JAVAC} ${JFLAGS} ${CLASS}.java
echo running ${CLASS}
${J9_EXEC} ${J9_FLAGS} ${CLASS}
