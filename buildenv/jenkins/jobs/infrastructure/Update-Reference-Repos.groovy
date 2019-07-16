/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
    LABEL = 'ci.role.build'
}

CLEAN_CACHE_DIR = params.CLEAN_CACHE_DIR
if ((CLEAN_CACHE_DIR == null) || (CLEAN_CACHE_DIR == '')) {
    CLEAN_CACHE_DIR = false
}

properties([buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '', numToKeepStr: '10')),
            pipelineTriggers([cron('''# Daily at 11:00pm
                                        0 23 * * *''')])])

def jobs = [:]

timeout(time: 6, unit: 'HOURS') {
    timestamps {
        node(SETUP_LABEL) {
            try{
                checkout scm
                def variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
                variableFile.parse_variables_file()
                variableFile.set_user_credentials()

                def osLabels = ['sw.os.aix', 'sw.os.linux', 'sw.os.osx', 'sw.os.windows']
                for (aNode in jenkins.model.Jenkins.instance.getLabel(LABEL).getNodes()) {
                    def nodeName = aNode.getDisplayName()

                    if (aNode.toComputer().isOffline()) {
                        // skip offline slave
                        continue
                    }

                    def foundLabel = false
                    def nodeLabels = aNode.getLabelString().tokenize(' ')
                    for (osLabel in osLabels) {
                        if (nodeLabels.contains(osLabel)) {
                            foundLabel = true
                            break
                        }
                    }

                    // get Eclipse OpenJ9 extensions repositories from variables file
                    def repos = get_openjdk_repos(VARIABLES.openjdk, foundLabel)

                    jobs["${nodeName}"] = {
                        node("${nodeName}"){
                            stage("${nodeName} - Update Reference Repo") {
                                refresh(nodeName, "${HOME}/openjdk_cache", repos, foundLabel)
                            }
                        }
                    }
                }
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
    def releases = ['8', '11', '12', '13']

    // iterate over VARIABLES.openjdk map and fetch the repository URL
    openJdkMap.entrySet().each { mapEntry ->
        if (releases.contains(mapEntry.key.toString())) {
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
    }

    echo "Eclipse OpenJ9 extensions repositories: ${repos.toString()}"
    return repos
}
