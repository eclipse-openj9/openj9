# OpenJ9 Helm Chart

## Introduction

Eclipse OpenJ9 is an independent implementation of a Java Virtual Machine.

The OpenJ9 Chart allows you to deploy and manage java applications running on OpenJ9 into OKD or OpenShift clusters. You can also perform Day-2 operations such as rolling update your application, enabling JITServer technology for continued performance enhancements or gathering JVM dumps using the operator.

## Resources Required

### System

* CPU Requested : 1 CPU
* Memory Requested : 256 MB

### Storage

* Currently no storage or volume needed.

## Chart Details

* Install one `deployment` running AdoptOpenJDK OpenJ9 image
* Install one `service` that is JITServer technology enabled by default

## Prerequisites

### Red Hat OpenShift SecurityContextConstraints Requirements

This chart does not require SecurityContextConstraints to be bound to the target namespace prior to installation.

### PodSecurityPolicy Requirements

This chart does not require PodSecurityPolicy to be bound to the target namespace prior to installation.

### PodDisruptionBudgets Requirement

This chart does not require PodDisruptionBudgets to be bound to the target namespace prior to installation.

## Installing the Chart

``` bash
helm install example-openj9 PATH_TO_OPENJ9_CHART
```

This chart installs OpenJ9 instance for Java 11 on x86 platform by default. If you would like to use OpenJ9 chart on other platform and different Java version, please override keyword `arch` or `image` with `--set` option.

## Verifying the Chart

``` bash
helm test example-openj9
helm status example-openj9
```

## Uninstalling the Chart

``` bash
helm delete example-openj9
```

## Getting started with OpenJ9 JITServer Technology

JITServer technology can be used for any Java application running on OpenJ9. Please make sure that JITServer image runs the same build version of JDK as your application image.

``` bash
helm install example-openj9 PATH_TO_OPENJ9_CHART
```

Once JITServer is deployed, you can enable JITServer by adding the following JVM option to your Java application.

You may change `<servicename>` and `<serverport>` by editing `values.yaml`. Look for service name with `kubectl get service`, `<serverport>` is set to `38400` by default.

``` bash
OPENJ9_JAVA_OPTIONS = -XX:+UseJITServer -XX:JITServerAddress=<servicename> -XX:JITServerPort=<serverport>
```

See the [JITServer quick start guide](doc/enabling-jitserver.adoc) for more information on how to enable JITServer technology with Open Liberty WebServer as an example java application.

### Configuration

| Qualifier      | Parameter                               | Definition                                                                  | Allowed Value                                             |
|----------------|-----------------------------------------|-----------------------------------------------------------------------------|-----------------------------------------------------------|
|fullnameOverride|                                         |Name of service, deployment, replicaset, should be unique inside cluster     |                                                           |
|arch            |                                         |CPU architecture preference                                                  |`amd64` or `ppc64le`                                       |
|image           |repository                               |Image repository                                                             |Defaults to `docker.io/adoptopenjdk`                       |
|                |tag                                      |Image tag                                                                    |Defaults to `openj9`                                       |
|                |pullPolicy                               |Image Pull Policy                                                            |`Always`, `Never`, or `IfNotPresent`. Defaults to Always   |
|container       |command                                  |Container start command / entrypoint, override existing entrypoint if present|Defaults to `jitserver`                                    |
|                |OPENJ9_JAVA_OPTIONS                      |Options passed into OpenJ9 JVM                                               |                                                           |
|                |limits.memory                            |Maximum memory allocated for container                                       |Defaults to `8 Gi`                                         |
|                |limits.cpu                               |Maximum CPU allocated for container                                          |Defaults to `8`                                            |
|                |requests.memory                          |Minimum memory allocated for container                                       |Defaults to `256 Mi`                                       |
|                |requests.cpu                             |Minimum CPU allocated for container                                          |Defaults to `1`                                            |
|service         |port                                     |Port number that java acceleration listens to                                |Defaults to `38400`                                        |
|replicaCount    |                                         |Number of replica to be deployed                                             |Defaults to `1`                                            |

## Documentation

* OpenJ9 website: https://www.eclipse.org/openj9/index.html
* OpenJ9 JITServer technology: https://www.eclipse.org/openj9/docs/jitserver/
* OpenJ9 repository: https://github.com/eclipse/openj9
* OpenJ9 docs: https://github.com/eclipse/openj9-docs

## Limitations

* Deploys on x86 (amd64) and power (ppcle) architecture only, it is intended to support other architectures in future releases.
* Supports Java 8, Java 11 and the current Java release only, it is intended to support other java versions in future releases.
