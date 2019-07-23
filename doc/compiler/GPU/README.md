# GPU Code Generation and Execution with OpenJ9

## Summary
In this document, we describe the GPU code generation feature in OpenJ9. Essentially, the just-in-time (JIT) compiler can compile and optimize parallel loops expressed as [Java 8's parallel stream API](https://docs.oracle.com/javase/8/docs/api/java/util/stream/package-summary.html) for GPU execution as well as multi-core CPU execution. Currently, IBM POWER CPUs (ppc64le) and Intel CPUs (x86_64) plus [NVIDIA GPUs](https://developer.nvidia.com/cuda-gpus) are supported.

## How it works
The GPU code generation feature is implemeted in OpenJ9's JIT compiler. Consider the following Java program:
```
void heavy() {
  IntStream.range(0, N).parallel().forEach(i -> {...});
}
```

The ```heavy()``` method contains Java's Parallel Stream API and is at first interpreted on the JVM. The JVM periodically monitors the *hotness* of the method, and, when needed, generates an optimized native CPU program. The JIT compiler at first performs a limited set of compiler optimizations over intermediate representation (IR) so as not to hinder the main program execution. However, depending on the hotness, it may recompile the method with more aggressive optimizations including GPU code generation. The figure below illustrates an overview of the GPU code generation:

![An overview of the GPU code generation.](./figs/gpucodegen.png "An overview of the GPU code generation." )

When the JIT compiler identifies the parallel stream API in the IR, it first applies a set of GPU-aware optimizations, then generates [NVVM IR](https://docs.nvidia.com/cuda/nvvm-ir-spec/index.html), and eventually generates a GPU kernel binary through [the ```libNVVM``` library](https://docs.nvidia.com/cuda/libnvvm-api/index.html) as well as the host CPU code (```GPU Runtime``` in the figure) that is responsible for device memory allocations, data transfers between the host and the device, and kernel invocations.

For more details, refer to our paper on compiling and optimizing Java 8 programs for GPU execution: [link](https://ieeexplore.ieee.org/document/7429325).

## How to build OpenJ9 with the GPU code generation enabled
To build OpenJ9 with the GPU code generation enabled, it requires a few more steps than [the original OpenJ9 build instruction](https://www.eclipse.org/openj9/oj9_build.html). Essentially, you need to 1) modify the Dockerfile to build a GPU-aware docker image, and 2) give a few variables when building OpenJDK with OpenJ9 (when doing ```make all```). The followings describe the steps to building OpenJ9 with the GPU code generation enabled:

### Prepare your system

#### Enable a GPU-aware docker image
First of all, when preparing your system, make sure that you have installed [the NVIDIA Container Runtime for Docker](https://github.com/NVIDIA/nvidia-docker) as well as [the NVIDIA driver](https://github.com/NVIDIA/nvidia-docker/wiki/Frequently-Asked-Questions#how-do-i-install-the-nvidia-driver) and [a supported version of Docker](https://github.com/NVIDIA/nvidia-docker/wiki/Frequently-Asked-Questions#which-docker-packages-are-supported).

Before proceeding to the next step, it is a good idea to make sure the installations are successfull. Let's make sure that your GPUs are recognized and the CUDA compiler ```nvcc``` is available within a docker container. In the following example, we create a docker container based on the ```nvidia/cuda:9.2-devel-ubuntu16.04``` image, and perform two commands: 1) the ```nvidia-smi``` command, which gives you detailed information on NVIDIA GPU devices, and 2) the ```nvcc``` command with the ```--version``` option. In this case, ```nvidia-smi``` shows the NVIDIA driver version 410.48 and the TITAN Xp GPU are found. Also, the version of the ```nvcc``` compiler is 9.2.

**Note:** you can pick any CUDA version that is supported in your GPU, but always make sure you pick a ```devel``` image because the GPU code generation feature requires the CUDA compiler toolchain. A full list of the available CUDA docker images can be found at [the Docker Hub](https://hub.docker.com/r/nvidia/cuda/). For ```ppc64le``` users, see [here](https://hub.docker.com/r/nvidia/cuda-ppc64le/).

```
$ docker run --runtime=nvidia --rm nvidia/cuda:9.2-devel-ubuntu16.04 /bin/bash -c "nvidia-smi; nvcc --version"
Tue May 21 20:59:06 2019
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 410.48                 Driver Version: 410.48                    |
|-------------------------------+----------------------+----------------------+
| GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|===============================+======================+======================|
|   0  TITAN Xp            Off  | 00000000:01:00.0 Off |                  N/A |
| 23%   33C    P8    17W / 250W |     10MiB / 12196MiB |      0%      Default |
+-------------------------------+----------------------+----------------------+

+-----------------------------------------------------------------------------+
| Processes:                                                       GPU Memory |
|  GPU       PID   Type   Process name                             Usage      |
|=============================================================================|
|  No running processes found                                                 |
+-----------------------------------------------------------------------------+
nvcc: NVIDIA (R) Cuda compiler driver
Copyright (c) 2005-2018 NVIDIA Corporation
Built on Tue_Jun_12_23:07:04_CDT_2018
Cuda compilation tools, release 9.2, V9.2.148
```

#### Edit the Dockerfile
Once you have confirmed that your GPU is recognized and the CUDA compiler ```nvcc``` is available within a docker container, pick one of the JDK Dockerfiles in "Prepare your system" in [the original OpenJ9 build instruction](https://www.eclipse.org/openj9/oj9_build.html). Then, edit the picked Dockerfile to change the base image to GPU-aware one.

For example, if you choose the Dockerfile for [Linux 64-bit (x86_64)](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu16/Dockerfile), look for ```FROM``` and update the line like this:
```
(Original) FROM ubuntu:16.04
(Updated)  FROM nvidia/cuda:9.2-devel-ubuntu16.04
```

For [Linux on Power systems 64-bit (ppc64)](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/ppc64le/ubuntu16/Dockerfile), look for ```FROM``` and update the line like this:
```
(Original) FROM ubuntu:16.04
(Updated)  FROM nvidia/cuda-ppc64le:9.2-devel-ubuntu16.04
```

Again, you can pick any CUDA version that is supported in your GPU, but always make sure you pick a ```devel``` image because the GPU code generation feature requires the CUDA compiler toolchain. A full list of the available CUDA docker images can be found at [the Docker Hub](https://hub.docker.com/r/nvidia/cuda/). For ```ppc64le``` users, see [here](https://hub.docker.com/r/nvidia/cuda-ppc64le/).

Also, look for "Create links for c++,g++,cc,gcc" and update the line like this:
```
# Create links for c++,g++,cc,gcc
(Original)
RUN ln -s g++ /usr/bin/c++ \
  && ln -s g++-4.8 /usr/bin/g++ \
  && ln -s gcc /usr/bin/cc \
  && ln -s gcc-4.8 /usr/bin/gcc

(Updated)
RUN ln -sf g++ /usr/bin/c++ \
  && ln -sf g++-4.8 /usr/bin/g++ \
  && ln -sf gcc /usr/bin/cc \
  && ln -sf gcc-4.8 /usr/bin/gcc
```

#### Build and run a GPU-aware docker image
Now you're ready to build the Dockerfile and run a container from the image:
```
$ docker build -t openj9.cuda9 -f Dockerfile .
$ docker run --runtime=nvidia -it openj9.cuda9
```

Also, it is a good idea to double check if the GPU and ```nvcc``` are visible in the container:
```
// Within the docker container
# nvidia-smi
# nvcc --version
```

### Get the source
Follow the steps described in "Get the source" in [the original build instruction](https://www.eclipse.org/openj9/oj9_build.html).

### Configure
Follow the steps described in "Configure" in [the original build instruction](https://www.eclipse.org/openj9/oj9_build.html).

### Build
Now you're ready to build OpenJDK with OpenJ9 with the GPU code generation capability enabled. When doing ```make```, you essentially need to give ```ENABLE_GPU=1``` and the path to ```CUDA_HOME``` and ```GDK_HOME```:

```
make all ENABLE_GPU=1 CUDA_HOME=/usr/local/cuda GDK_HOME=/usr/include/nvidia/gdk
```

Make sure that they are recoginized by the ```make``` command:

```
$ make all ENABLE_GPU=1 CUDA_HOME=/usr/local/cuda GDK_HOME=/usr/include/nvidia/gdk
...
Compiling OpenJ9 in /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm
...
J9_VERSION:           29
TR_BUILD_NAME:        c6fffc33a
J9SRC:                /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm/compiler/..
JIT_SRCBASE:          /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm/compiler/..
JIT_OBJBASE:          /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm/compiler/../objs
JIT_DLL_DIR:          /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm/compiler/..
OMR_DIR:              /openj9-openjdk-jdk8/build/linux-ppc64le-normal-server-release/vm/compiler/../omr
BUILD_CONFIG:         prod
NUMBER_OF_PROCESSORS:
VERSION_MAJOR:        8
ENABLE_GPU:           1
CUDA_HOME:            /usr/local/cuda
GDK_HOME:             /usr/include/nvidia/gdk
```

### Test
To test the GPU code generation feature, see "How to write and runs GPU programs" below.

## How to write and run GPU programs
The ```example``` directory contains Vector Addition and Matrix Multiplication. The example implementations with the parallel stream API are as follows:


### Example 1: Vector Addition
```
IntStream.range(0, n).parallel().forEach( i -> { C[i] = A[i] + B[i]; } );
```
### Example 2: Matrix Multiplication
```
IntStream.range(0, n*n).parallel().forEach ( idx -> {
  int i = idx / n;
  int j = idx % n;
  double sum = 0;
  for (int k = 0; k < n; k++) {
    sum += A[i*n + k] * B[k*n + j];
  }
  C[idx] = sum;
});
```

To enable the GPU code generation, since the GPU code generation capability is disabled by default, you are required to give the ```enableGPU``` option to the JIT compiler - i.e.,  ```-Xjit:enableGPU```. Also, you can give additional flags to it: ```enableGPU={details}```, ```enableGPU={verbose}```,and ```enableGPU={enableMath}```. Note that multiple flags can be specified by concatenating with ```|``` (e.g., ```enableGPU={details|verbose|enableMath}```). The semantics of each flag is described below. The complete options for the above benchmarks can be found in ```run.sh``` in the ```example/VecAdd``` and ```example/MatMult``` directory.

#### The ```verbose``` flag
```verbose``` outputs detailed information on GPU execution:

```
[IBM GPU JIT]: 	Device Number  0: name=Tesla P100-SXM2-16GB, ComputeCapability=6.0
[IBM GPU JIT]: 	Device Number  1: name=Tesla P100-SXM2-16GB, ComputeCapability=6.0
[IBM GPU JIT]: [time.ms=1563907801105]: Launching parallel forEach in VecAdd.runGPULambda()V at line 47 on GPU
[IBM GPU JIT]: [time.ms=1563907801105]: Finished parallel forEach in VecAdd.runGPULambda()V at line 47 on GPU
```

#### The ```details``` flag

```details``` outputs more detailed information on GPU compilation and execution:

```
[IBM GPU JIT]: NVML path to load libnvidia-ml.so
[IBM GPU JIT]:  Device Number  0: name=Tesla P100-SXM2-16GB, ComputeCapability=6.0
[IBM GPU JIT]:  Device Number  1: name=Tesla P100-SXM2-16GB, ComputeCapability=6.0
[IBM GPU JIT]: Found forEach in VecAdd.runGPULambda()V
[IBM GPU JIT]: Found GPU scope 169 in VecAdd.runGPULambda()V (single kernel type)
[IBM GPU JIT]: Found forEach in VecAdd.runGPULambda()V
[IBM GPU JIT]: Found GPU scope 61 in VecAdd.runGPULambda()V (single kernel type)
[IBM GPU JIT]: Detected affine load in VecAdd.runGPULambda()V at line 32
[IBM GPU JIT]: Detected affine load in VecAdd.runGPULambda()V at line 32
[IBM GPU JIT]:  forEach in VecAdd.runGPULambda()V at line 31 does not have any loops
[IBM GPU JIT]:  Device Number  0: ComputeCapability=6.0
[IBM GPU JIT]:  Creating NVVM program
[IBM GPU JIT]:  Adding NVVM module size=3859
[IBM GPU JIT]:  Added NVVM module size=3859
[IBM GPU JIT]:  NVVM Version: 1.5
[IBM GPU JIT]:  Compiling NVVM program with options: -opt=0 -ftz=1 -prec-sqrt=1 -prec-div=1 -fma=0 -arch=compute_35   5.442 msec
...
```

#### The ```enableMath``` flag
The  ```enableMath``` flag should be given when the body of lambda expression performs math operations. The current implementation supports many of ```java.lang.Math``` methods.

#### The ```enforce``` flag
The ```enforce``` flag forces the GPU runtime to use the GPU version when avaiable. Without this option, the current GPU runtime tries to select a preferable device from the CPU and GPU by human created performance heuristics.

#### The ```adaptive``` flag
The ```adaptive``` flags let the GPU runtime to speculatively run both the CPU and GPU versions and select a faster device in a statistically rigorous way.

#### The ```safeMT``` flag
This option asserts that arrays accessed within lambda expressions are not written by any other threads. This allows JIT to send more lambda expresssions to GPU.

### Limitations

- Reduction operations are not supported.
- Only accesses primitive types, and one-dimensional arrays of primitive types.
- No access to static scalar variables: only locals, parameters, or instance variables.
- No unresolved or native methods.
- No creating new heap Objects (```new``` ...), exceptions (```throw``` …), or ```instanceof```.
- Intermediate stream operations like map or filter are not supported.
- Unless ```safeMT``` is specified, writes to arrays have to be unconditional and contiguous, for example, the following code is not allowed without ```safeMT```:
```
if (...)
   a[i*4] = ...
```

## Further readings
"Compiling and Optimizing Java 8 Programs for GPU Execution." Kazuaki Ishizaki, Akihiro Hayashi, Gita Koblents, Vivek Sarkar. 24th International Conference on Parallel Architectures and Compilation Techniques (PACT), October 2015. https://ieeexplore.ieee.org/document/7429325
