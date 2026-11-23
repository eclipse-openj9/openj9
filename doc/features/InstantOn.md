<!--
Copyright IBM Corp. and others 2026

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
Assisted-by: IBM Bob
-->

# Instructions for testing OpenJ9 InstantOn inside containers

## Overview

InstantOn is an OpenJ9 feature built on [CRIU](https://criu.org/) that checkpoints a JVM
process mid-startup and stores the checkpoint inside a container image. When the image later starts
a container, the JVM is restored from the checkpoint rather than cold-started, dramatically reducing
startup time.

InstantOn is supported across multiple Java versions (such as Java 17, 21, and newer) and Linux
distributions (including RHEL UBI 8, UBI 9, UBI 10, and UBI-minimal variants).

### Identifying InstantOn-Capable Images

Not all Semeru and Liberty container images support InstantOn:
- **Semeru images:** A Semeru container image or Dockerfile supports InstantOn if it includes a `RUN`
  command that downloads, verifies, and installs CRIU (e.g., fetching `criu.tar.gz` via `CRIU_URL`).
  Dockerfiles lacking CRIU installation steps (such as standard Ubuntu images) cannot be used for
  InstantOn.
- **Open Liberty images:** The Liberty images that support InstantOn are the subset of images built
  directly upon InstantOn-capable Semeru base images and packaged with the necessary runtime helper
  scripts (`checkpoint.sh`, `restore-server.sh`, and `docker-server.sh`).

Open Liberty is currently the only officially supported InstantOn "client": it contains explicit
code that hooks into OpenJ9's CRIU API (`org.eclipse.openj9.criu.CRIUSupport` in the `openj9.criu`
module) and provides companion scripts to coordinate the checkpoint and restore processes. While
InstantOn can theoretically be tested independently of Liberty—for example, using a handcrafted Java
application calling the OpenJ9 CRIU API directly paired with custom wrapper scripts—Liberty serves
as the standard, production-ready framework for testing and deploying InstantOn in containers.

Taking the checkpoint requires Linux capabilities (`CHECKPOINT_RESTORE`, `SETPCAP`, `SYS_PTRACE`).
Because **Podman** supports granting capabilities at build time via `podman build --cap-add=...`,
it can produce the checkpointed image directly from a `Dockerfile`.

**Docker**, on the other hand, does not support `--cap-add` during `docker build`. To use Docker,
we build an intermediate "pre-checkpoint" image, run a container from it with the required
capabilities and security options to take the checkpoint, and then `docker commit` the stopped
container into the final restored image.

Both paths are documented below.

---

## Choosing Your Starting Point (Short-Circuiting Steps)

Depending on what component you are testing, you do not need to build every layer from scratch. You
can use pre-built container images from the IBM Container Registry (`icr.io`) to skip earlier steps:

```
  Layer                       Build From Scratch              Short-Circuit Image
  ─────────────────────────────────────────────────────────────────────────────────────────────────────
  1. JVM + CRIU               semeru-containers (Step 1)      icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro>
  2. Open Liberty             ci.docker (Step 2)              icr.io/appcafe/open-liberty:full-java<version>-openj9-<distro>
  3. Application              guide-getting-started (Step 3)  (Your application source / WAR)
  4. Checkpoint Image         Podman or Docker (Step 4)       (Pre-built checkpointed image)
```

*(Where `<version>` is a supported Java language version such as `17` or `21`, and `<distro>` is a supported distribution such as `ubi` or `ubi9`.)*

### Scenario A: Testing a custom OpenJ9 JDK build (Fast Layering)
Instead of building images from scratch, you can rapidly overlay your local test `jdk/` on top of an
existing container image:

- **Option A1: Overlay on a Semeru base image (Bypasses Step 1)**
  ```dockerfile
  FROM icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro>
  USER root
  COPY jdk /opt/java/openjdk
  USER 1001
  ```
  Build this as `instanton-jvm:test` and proceed to **Step 2**.

- **Option A2: Overlay on an Open Liberty image (Bypasses Steps 1 and 2)**
  ```dockerfile
  FROM icr.io/appcafe/open-liberty:full-java<version>-openj9-<distro>
  USER root
  COPY jdk /opt/java/openjdk
  USER 1001
  ```
  Build this as `instanton-liberty:test` and proceed directly to **Step 3**.

- **Option A3: Overlay on an existing Application image (Bypasses Steps 1, 2, and 3)**
  ```dockerfile
  FROM <your-existing-app-image>
  USER root
  COPY jdk /opt/java/openjdk
  USER 1001
  ```
  Build this as `instanton-app:pre-checkpoint` and proceed directly to **Step 4** (checkpoint) and
  **Step 5** (restore & test).

> **Important Caveat — Shared Classes Cache (SCC) Invalidation:**
> OpenJ9 Shared Classes Caches (SCC) are **not portable across different JVM builds**.
>
> In the Semeru and Liberty container ecosystem, SCCs are built up in layers:
> 1. **Semeru base images** pre-generate a system bootclasses SCC layer in `/opt/java/.scc`.
> 2. **Open Liberty images** add a second SCC layer containing Liberty runtime and feature classes.
> 3. **Liberty application images** following Liberty container guidelines (e.g., via `RUN configure.sh`)
>    add a third SCC layer containing application classes.
>
> When you overlay a new or custom JDK build onto a Semeru, Liberty, or application image, the JVM
> detects that the existing SCC layer hierarchy was produced by a different JVM build and is
> incompatible. Because `nonFatal` is configured by default in `JAVA_TOOL_OPTIONS` and
> `OPENJ9_JAVA_OPTIONS` (e.g. `-Xshareclasses:name=openj9_system_scc,cacheDir=/opt/java/.scc,readonly,nonFatal`),
> the JVM does not abort; instead, it **silently ignores and bypasses** all incompatible SCC layers.
>
> As a result, cold-startup performance, class loading, memory footprint (due to unshared class
> metadata), and AOT/JIT optimizations will reflect an execution without SCC acceleration. If your
> test scenario specifically evaluates JVM performance, footprint, or SCC behavior alongside
> InstantOn, you should regenerate the SCC layers using your custom JDK build (building from scratch
> via Steps 1–3) or adjust your SCC options accordingly.

### Scenario B: Testing Open Liberty InstantOn changes (Standard JVM)
If you are developing or testing changes in `ci.docker` or Open Liberty itself (and do not need a
custom JVM build):
- **Skip Step 1 entirely.**
- In **Step 2**, keep the original `FROM icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro>`
  lines in the Liberty `Dockerfile` and build `instanton-liberty:test`.
- Proceed to **Step 3**.

### Scenario C: Testing Application compatibility with InstantOn
If you are testing whether an application runs correctly with InstantOn using official, released
versions of Semeru and Liberty:
- **Skip Steps 1 and 2 entirely.**
- In **Step 3b**, point your application `Dockerfile` directly to an official Open Liberty image
  matching your Java version:
  ```dockerfile
  FROM icr.io/appcafe/open-liberty:full-java<version>-openj9-<distro>
  # or FROM icr.io/appcafe/open-liberty:kernel-slim-java<version>-openj9-<distro>
  ```
- Proceed directly to **Step 4** (checkpoint) and **Step 5** (restore & test).

### Scenario D: Testing CRIU restore & container runtime environment
If you already have a pre-built or checkpointed application image and want to test CRIU restore,
kernel compatibility, security profiles, or Kubernetes pod deployments:
- **Skip Steps 1–4 entirely.**
- Proceed directly to **Step 5** to test running the container with the required capabilities.

---

## Prerequisites

### JVM build under test

You need a local OpenJ9 JDK build to test, extracted to a directory (referred to below as
`/path/to/jdk`). Check `jdk/release` to verify the Java version and confirm it contains
`openj9.criu` in the `MODULES` list — that module is required for InstantOn support. Example:

```
$ grep -E 'JAVA_VERSION|MODULES' /path/to/jdk/release
JAVA_VERSION="17.0.x"    # or 21.0.x, etc.
MODULES="... openj9.criu ..."
```

### Git repositories required

Clone the following into a common parent directory. All paths below use `~/` as that parent:

```sh
git clone https://github.com/ibmruntimes/semeru-containers.git
git clone https://github.com/OpenLiberty/ci.docker.git
git clone https://github.com/OpenLiberty/guide-getting-started.git
```

### Host JDK

A JDK must be available on the host to compile the application with Maven in Step 3. Set
`JAVA_HOME` to your JDK under test, or ensure a JDK of matching version is on `PATH`:

```sh
export JAVA_HOME=/path/to/jdk
```

### Container tooling

Either **Docker** or **Podman** can be used throughout Steps 1–3 and Step 5. Step 4 details the
workflow difference between the two runtimes.

### CRIU authentication secrets file

The `semeru-containers` Dockerfile downloads a pre-built CRIU tarball from an internal Artifactory
server that requires a Bearer token. Create a file called `criu_secrets` anywhere on the host:

```
CRIU_AUTH_HEADER="Authorization: Bearer <token>"
```

Generate `<token>` at <https://na.artifactory.swg-devops.com/ui/user_profile>. The
`--secret id=criu_secrets,src=...` flag passes this file to the Dockerfile as a build-time secret
without baking it into any image layer.

---

## Step 1 — Build the JVM container image

### 1a. Choose a base Dockerfile

Browse `semeru-containers` to find the Dockerfile matching your Java version, package type (JDK/JRE),
and target Linux distribution:

```
semeru-containers/<java_version>/<jdk_or_jre>/<linux_distro>/<distro_version>/Dockerfile.open.releases.full
```

Examples:
- Java 17 on UBI 9: `semeru-containers/17/jdk/ubi/ubi9/Dockerfile.open.releases.full`
- Java 21 on UBI 9: `semeru-containers/21/jdk/ubi/ubi9/Dockerfile.open.releases.full`
- Java 21 on UBI 10 minimal: `semeru-containers/21/jre/ubi-minimal/ubi10/Dockerfile.open.releases.full`

> **InstantOn check:** Ensure the chosen Dockerfile has InstantOn support by checking that it
> contains commands to fetch and configure CRIU (e.g. `grep -i 'criu' <Dockerfile>`). The UBI and
> UBI-minimal variants contain CRIU installation instructions.

### 1b. Set up the build context

Create a directory to serve as the build context. Copy the chosen Dockerfile and the JVM build into it:

```sh
mkdir -p ~/semeru-jvm-ctx
cp semeru-containers/<java_version>/<jdk_or_jre>/<linux_distro>/<distro_version>/Dockerfile.open.releases.full ~/semeru-jvm-ctx/Dockerfile
cp -r /path/to/jdk ~/semeru-jvm-ctx/jdk
```

The `jdk/` directory must land at the root of the build context so that it is reachable by a
`COPY jdk ...` instruction.

### 1c. Replace the JVM download step with a COPY

The Dockerfile contains a `RUN` block that downloads and unpacks a Semeru release binary into
`/opt/java/openjdk`. It looks like:

```dockerfile
RUN set -eux; \
    ARCH="$(uname -m)"; \
    case "${ARCH}" in \
       ...
       amd64|x86_64) \
         ESUM='...'; \
         BINARY_URL='https://github.com/ibmruntimes/semeru...-binaries/...'; \
    ...
    curl -LfsSo /tmp/openjdk.tar.gz ${BINARY_URL}; \
    ...
    tar -xf /tmp/openjdk.tar.gz --strip-components=1; \
    ...
    rm -rf /tmp/openjdk.tar.gz;
```

Replace **the entire block** with:

```dockerfile
COPY jdk /opt/java/openjdk
```

The `jdk/` directory is already an extracted JDK root, so no `--strip-components` is needed. The
`COPY` instruction creates the destination directory automatically, so any `mkdir -p /opt/java/openjdk`
inside the original `RUN` block does not need to be preserved.

> **Note:** The CRIU build number embedded in the Dockerfile (e.g. `criu_build/146/`) may refer to
> a build that no longer exists on Artifactory. If the CRIU download step fails with a 404, browse
> <https://na.artifactory.swg-devops.com/artifactory/hyc-rt-java-delivery-generic-local/IBM/java/criu_build/>
> to find the latest available build number and substitute it in the two `CRIU_URL` / `CRIU_SHA_URL`
> lines for your architecture before building.

### 1d. Build the JVM image

Using **Docker**:
```sh
docker build \
  --secret id=criu_secrets,src=~/criu_secrets \
  -t instanton-jvm:test \
  ~/semeru-jvm-ctx
```

Using **Podman**:
```sh
podman build \
  --secret id=criu_secrets,src=~/criu_secrets \
  -t localhost/instanton-jvm:test \
  ~/semeru-jvm-ctx
```

> **Tip for Podman users:** Prefix locally-built image tags with `localhost/` (e.g.
> `localhost/instanton-jvm:test`) so Podman does not prompt you to select a remote registry.

---

## Step 2 — Build the Open Liberty container image

### 2a. Choose a Liberty Dockerfile

InstantOn-capable Liberty Dockerfiles are located in:

```
ci.docker/releases/latest/full/
ci.docker/releases/latest/kernel-slim/
```

Choose the Dockerfile corresponding to the Linux distribution and Java version you selected in Step 1:
- `ci.docker/releases/latest/full/Dockerfile.<distro>.openjdk<java_version>`
- `ci.docker/releases/latest/kernel-slim/Dockerfile.<distro>.openjdk<java_version>`

Examples:
- Java 17 on UBI: `ci.docker/releases/latest/full/Dockerfile.ubi.openjdk17`
- Java 21 on UBI 10 minimal: `ci.docker/releases/latest/full/Dockerfile.ubi10-minimal.openjdk21`

`full` includes all Liberty features; `kernel-slim` provides just the kernel and downloads only the
features your application declares, resulting in a smaller image but a longer build.

> **Important:** The `guide-getting-started` application Dockerfile calls `RUN features.sh`, a
> helper that is only present in `kernel-slim` images. If you use the `full` variant (recommended),
> you must remove that line from the application Dockerfile in Step 3b — the `full` image already
> contains all features. If you use `kernel-slim`, leave the line as-is.

### 2b. Replace both FROM lines

The Dockerfile has **two stages**, both pulling the base Semeru runtime image
(e.g., `FROM icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro>`). Replace each with your
JVM tag from Step 1:

```dockerfile
# Before:
FROM icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro> AS getRuntime
# After:
FROM instanton-jvm:test AS getRuntime

# Before (second stage):
FROM icr.io/appcafe/ibm-semeru-runtimes:open-<version>-jdk-<distro>
# After:
FROM instanton-jvm:test
```

### 2c. Build the Liberty image

Build from the chosen variant's directory so that the `helpers/`, `fixes/`, `NOTICES`, `resources/`,
and `server.xml` context files are available:

Using **Docker**:
```sh
docker build \
  -t instanton-liberty:test \
  -f ci.docker/releases/latest/<variant>/Dockerfile.<distro>.openjdk<version> \
  ci.docker/releases/latest/<variant>
```

Using **Podman**:
```sh
podman build \
  -t localhost/instanton-liberty:test \
  -f ci.docker/releases/latest/<variant>/Dockerfile.<distro>.openjdk<version> \
  ci.docker/releases/latest/<variant>
```

---

## Step 3 — Build the Getting Started application

### 3a. Build the application with Maven

```sh
cd guide-getting-started/finish
JAVA_HOME=/path/to/jdk ./mvnw package
```

This produces the WAR file that the Liberty Dockerfile will pick up.

### 3b. Configure the application Dockerfile

Open `guide-getting-started/finish/Dockerfile` and replace the `FROM` line with your Liberty tag:

```dockerfile
FROM instanton-liberty:test
```

If you chose the `full` Liberty variant in Step 2, remove the `RUN features.sh` line — it only exists
in `kernel-slim` images and will fail with `command not found` in a `full`-based image.

The remaining configuration depends on whether you plan to build the final image using **Podman** or
**Docker**:

- **For Podman:** Append the checkpoint instruction at the end of the file:
  ```dockerfile
  RUN checkpoint.sh afterAppStart
  ```
  Or the diagnostic version that prints the CRIU log on failure:
  ```dockerfile
  RUN CRIU_LOG_LEVEL=4 checkpoint.sh afterAppStart \
      || { rc=$?; cat /logs/checkpoint/checkpoint.log; exit $rc; }
  ```

- **For Docker:** Do **not** add `RUN checkpoint.sh` to the Dockerfile. Because `docker build` cannot
  grant capabilities, attempting to run `checkpoint.sh` during `docker build` will fail. Instead, the
  Dockerfile will build a "pre-checkpoint" image, and the checkpoint will be captured in Step 4 via
  a container run + commit.

Valid checkpoint phases are:
- `beforeAppStart` — checkpoint before Liberty starts loading the application; fastest checkpoint,
  less startup work pre-saved
- `afterAppStart` — checkpoint after the application has finished starting; more startup work
  pre-saved, recommended for most use cases

---

## Step 4 — Build the checkpointed application image

Here the Docker and Podman workflows diverge.

### Option A: Using Podman (build-time checkpoint)

Podman supports `--cap-add` during `podman build`.

1. **If using rootless Podman**, copy the Liberty image to root's store (since capability-enabled builds
   require root privileges):
   ```sh
   podman save localhost/instanton-liberty:test | sudo podman load
   ```

2. **Build the checkpointed image:**
   ```sh
   sudo podman build \
     --cap-add=CHECKPOINT_RESTORE \
     --cap-add=SETPCAP \
     --cap-add=SYS_PTRACE \
     --security-opt seccomp=unconfined \
     -t localhost/instanton-app:test \
     guide-getting-started/finish
   ```

---

### Option B: Using Docker (run-and-commit checkpoint)

`docker build` does not support `--cap-add`. To generate the checkpointed image with Docker:

1. **Build the pre-checkpoint image:**
   ```sh
   docker build -t instanton-app:pre-checkpoint guide-getting-started/finish
   ```

2. **Run a temporary container with the necessary capabilities to take the checkpoint:**
   ```sh
   docker run --name instanton-checkpoint \
     --cap-add=CHECKPOINT_RESTORE \
     --cap-add=SETPCAP \
     --cap-add=SYS_PTRACE \
     --security-opt seccomp=unconfined \
     --security-opt apparmor=unconfined \
     -e WLP_CHECKPOINT=afterAppStart \
     instanton-app:pre-checkpoint
   ```

   > **Why these options?**
   > - `--privileged` is **not** required. Passing `--cap-add=CHECKPOINT_RESTORE`,
   >   `--cap-add=SETPCAP`, `--cap-add=SYS_PTRACE`, `--security-opt seccomp=unconfined`, and
   >   disabling AppArmor via `--security-opt apparmor=unconfined` provides all necessary permissions
   >   for CRIU.
   > - `-e WLP_CHECKPOINT=afterAppStart` instructs Liberty's entrypoint (`docker-server.sh`) to
   >   invoke `checkpoint.sh afterAppStart`.
   > - To increase CRIU logging verbosity during checkpoint, pass `-e CRIU_LOG_LEVEL=4`.
   > - Once the checkpoint is taken and saved to `/output/workarea/checkpoint/image/`, Liberty
   >   terminates and the container exits with code 0.

3. **Commit the container to create the final image:**
   ```sh
   docker commit \
     --change 'ENTRYPOINT ["/opt/ol/helpers/runtime/docker-server.sh"]' \
     --change 'CMD ["/opt/ol/wlp/bin/server", "run", "defaultServer"]' \
     --change 'ENV WLP_CHECKPOINT=' \
     instanton-checkpoint instanton-app:test
   ```

   > **Note on `--change` flags:**
   > - The `ENTRYPOINT` and `CMD` are explicitly reset to ensure the committed image boots via
   >   `docker-server.sh`.
   > - `ENV WLP_CHECKPOINT=` clears the environment variable so that subsequent container runs
   >   restore the server from the checkpoint image rather than attempting to take another checkpoint.

4. **Clean up the stopped container:**
   ```sh
   docker rm instanton-checkpoint
   ```

---

## Step 5 — Test the result

Run a container from the final checkpointed image. The container requires `CHECKPOINT_RESTORE`,
`SETPCAP`, and unconfined seccomp/AppArmor to restore the JVM process from CRIU images.

Using **Docker**:
```sh
docker run --rm \
  --cap-add=CHECKPOINT_RESTORE \
  --cap-add=SETPCAP \
  --security-opt seccomp=unconfined \
  --security-opt apparmor=unconfined \
  -p 9080:9080 \
  instanton-app:test
```

Using **Podman**:
```sh
sudo podman run --rm \
  --cap-add=CHECKPOINT_RESTORE \
  --cap-add=SETPCAP \
  --security-opt seccomp=unconfined \
  -p 9080:9080 \
  localhost/instanton-app:test
```

The Liberty server should restore from the checkpoint and report ready in milliseconds. The Getting
Started application is reachable at `http://localhost:9080/`.
