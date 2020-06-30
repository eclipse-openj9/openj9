/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
SETUP_LABEL = params.SETUP_LABEL
if (!SETUP_LABEL) {
    SETUP_LABEL = 'worker'
}

LABEL = params.LABEL
if (!LABEL) {
    LABEL = 'ci.role.build || ci.role.test'
}

CLEAN_CACHE_DIR = params.CLEAN_CACHE_DIR
if ((CLEAN_CACHE_DIR == null) || (CLEAN_CACHE_DIR == '')) {
    CLEAN_CACHE_DIR = false
}

UPDATE_SETUP_NODES = params.UPDATE_SETUP_NODES
UPDATE_BUILD_NODES = params.UPDATE_BUILD_NODES

def jobs = [:]

timeout(time: 6, unit: 'HOURS') {
    timestamps {
        node(SETUP_LABEL) {
            try {
                def gitConfig = scm.getUserRemoteConfigs().get(0)
                def remoteConfigParameters = [url: "${gitConfig.getUrl()}"]

                if (gitConfig.getCredentialsId()) {
                    remoteConfigParameters.put("credentialsId", "${gitConfig.getCredentialsId()}")
                }

                checkout changelog: false,
                        poll: false,
                        scm: [$class: 'GitSCM',
                        branches: [[name: scm.branches[0].name]],
                        doGenerateSubmoduleConfigurations: false,
                        extensions: [[$class: 'CloneOption',
                                      reference: "${HOME}/openjdk_cache"]],
                        submoduleCfg: [],
                        userRemoteConfigs: [remoteConfigParameters]]

                def variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
                variableFile.parse_variables_file()
                variableFile.set_user_credentials()

                def buildNodes = jenkins.model.Jenkins.instance.getLabel(LABEL).getNodes()
                def slaveNodes = []
                def setupNodesNames = []
                def buildNodesNames = []

                if (UPDATE_SETUP_NODES) {
                    // update openj9 repo cache on slaves that have SETUP_LABEL
                    if (UPDATE_BUILD_NODES) {
                        // remove build nodes from the setup nodes collection
                        slaveNodes.addAll(jenkins.model.Jenkins.instance.getLabel(SETUP_LABEL).getNodes().minus(buildNodes))
                    } else {
                        slaveNodes.addAll(jenkins.model.Jenkins.instance.getLabel(SETUP_LABEL).getNodes())
                    }

                    //add master if slaveNodes does not contain it already
                    if (slaveNodes.intersect(jenkins.model.Jenkins.instance.getLabel('master').getNodes()).isEmpty()) {
                        slaveNodes.addAll(jenkins.model.Jenkins.instance.getLabel('master').getNodes())
                    }

                    for (sNode in slaveNodes) {
                        if (sNode.toComputer().isOffline()) {
                            // skip offline slave
                            continue
                        }

                        def sNodeName = sNode.getDisplayName()
                        if (sNodeName == 'Jenkins') {
                            sNodeName = 'master'
                        }
                        setupNodesNames.add(sNodeName)

                        jobs["${sNodeName}"] = {
                            node("${sNodeName}") {
                                stage("${sNodeName} - Update Reference Repo") {
                                    refresh(sNodeName, "${HOME}/openjdk_cache", [[name: "openj9", url: VARIABLES.openj9.get('default').get('repoUrl')]], true)
                                }
                            }
                        }
                    }
                }

                if (UPDATE_BUILD_NODES) {
                    // update openjdk and openj9 repos cache on slaves
                    for (aNode in buildNodes) {
                        if (aNode.toComputer().isOffline()) {
                            // skip offline slave
                            continue
                        }

                        def nodeName = aNode.getDisplayName()
                        buildNodesNames.add(nodeName)

                        def osLabels = ['sw.os.aix', 'sw.os.linux', 'sw.os.osx', 'sw.os.windows']
                        def foundLabel = false
                        def nodeLabels = aNode.getLabelString().tokenize(' ')
                        for (osLabel in osLabels) {
                            if (nodeLabels.contains(osLabel)) {
                                foundLabel = true
                                break
                            }
                        }

                        // get Eclipse OpenJ9 extensions repositories from variables file
                        def repos = []
                        if (nodeLabels.contains('ci.role.build')) {
                            repos.addAll(get_openjdk_repos(VARIABLES.openjdk, foundLabel))
                            repos.add([name: "openj9", url: VARIABLES.openj9.get('default').get('repoUrl')])
                            repos.add([name: "omr", url: VARIABLES.omr.get('default').get('repoUrl')])
                        }

                        if (nodeLabels.contains('ci.role.test')) {
                            // add AdoptOpenJDK/openjdk-tests repository
                            repos.add([name: "adoptopenjdk", url: VARIABLES.adoptopenjdk.default.get('repoUrl')])
                        }

                        if (jenkins.model.Jenkins.instance.getLabel(SETUP_LABEL).getNodes().contains(aNode)) {
                            // add OpenJ9 repo
                            repos.add([name: "openj9", url: VARIABLES.openj9.get('default').get('repoUrl')])
                            setupNodesNames.add(aNode)
                        }

                        // Remove any dups
                        repos.unique()

                        jobs["${nodeName}"] = {
                            node("${nodeName}") {
                                stage("${nodeName} - Update Reference Repo") {
                                    refresh(nodeName, "${HOME}/openjdk_cache", repos, foundLabel)
                                }
                            }
                        }
                    }
                }

                echo "Setup nodes: ${setupNodesNames.toString()}"
                echo "Build nodes: ${buildNodesNames.toString()}"

            } finally {
                cleanWs()
            }
        }

        parallel jobs
    }
}

/*
 * Creates and updates the git reference repository cache on the node.
 */
def refresh(node, cacheDir, repos, isKnownOs) {
    if (CLEAN_CACHE_DIR) {
        sh "rm -fr ${cacheDir}"
    }

    dir("${cacheDir}") {
        stage("${node} - Config") {
            sh "git init --bare"
            repos.each { repo ->
                config(repo.name, repo.url)
            }
        }
        stage("${node} - Fetch") {
            if (USER_CREDENTIALS_ID && isKnownOs) {
                 sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
                    sh 'git fetch --all'
                }
            } else {
                sh "git fetch --all"
            }
        }
        stage("${node} - GC Repo") {
            sh "git gc --aggressive --prune=all"
        }
    }
}

/*
 * Add a git remote.
 */
def config(remoteName, remoteUrl) {
    sh "git config remote.${remoteName}.url ${remoteUrl}"
    sh "git config remote.${remoteName}.fetch +refs/heads/*:refs/remotes/${remoteName}/*"
}

/*
 * Parses openjdk map from variables file and fetch the URL of the extensions
 * repositories for the supported releases. Returns an array.
 */
def get_openjdk_repos(openJdkMap, useDefault) {
    def repos = []

    // iterate over VARIABLES.openjdk map and fetch the repository URL
    openJdkMap.entrySet().each { mapEntry ->
        if (useDefault) {
            repos.add([name: "jdk${mapEntry.key}", url: mapEntry.value.get('default').get('repoUrl')])
        } else {
           mapEntry.value.entrySet().each { entry ->
                if (entry.key != 'default') {
                    repos.add([name: "jdk${mapEntry.key}", url: entry.value.get('repoUrl')])
                }
            }
        }
    }
    return repos
}
