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

/**
* This script deletes old artifacts off the Artifactory server 
* 
* Parameters: 
*   JOB_TYPE: Choice - Keep artifacts based on TIME or COUNT
*   JOB_TO_CHECK: String – The folder to keep x amount of builds in 
*   ARTIFACTORY_SERVER: String - The artifactory server to clean up
*   ARTIFACTORY_REPO: String - The artifactory repo to clean up
*   ARTIFACTORY_NUM_ARTIFACTS: String - How many artifacts to keep in artifactory
*   ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS: String - Artifacts older than this will be deleted
**/

timestamps {
    LABEL = params.LABEL
    if (!LABEL) {
        LABEL = 'worker'
    }
    node(LABEL) {
        checkout scm
        def variableFile = load 'buildenv/jenkins/common/variables-functions'
        variableFile.parse_variables_file()
        variableFile.set_artifactory_config()

        def artifactory_server = params.ARTIFACTORY_SERVER ? params.ARTIFACTORY_SERVER : env.ARTIFACTORY_SERVER
        def server = Artifactory.server artifactory_server
        def ARTIFACTORY_SERVER_URL = server.getUrl()
        def ARTIFACTORY_REPO = params.ARTIFACTORY_REPO ? params.ARTIFACTORY_REPO : env.ARTIFACTORY_REPO
        def artifactoryCreds = env.ARTIFACTORY_CREDS
        def ARTIFACTORY_NUM_ARTIFACTS = params.ARTIFACTORY_NUM_ARTIFACTS ? params.ARTIFACTORY_NUM_ARTIFACTS : env.ARTIFACTORY_NUM_ARTIFACTS
        def ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS = params.ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS ? params.ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS : env.ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS

        switch(params.JOB_TYPE) {
            case 'TIME':
                cleanupTime(ARTIFACTORY_SERVER_URL, ARTIFACTORY_REPO, ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS, artifactoryCreds)
            break
            case 'COUNT':
                if (params.JOB_TO_CHECK){
                    cleanupBuilds(ARTIFACTORY_SERVER_URL, ARTIFACTORY_REPO, params.JOB_TO_CHECK, ARTIFACTORY_NUM_ARTIFACTS, artifactoryCreds)
                } else {
                    error 'Please input a job to cleanup'
                }
            break
            default:
                error 'the JOB_TO_CHECK parameter is not properly defined. Please Enter TIME or COUNT'
            break
        }
    }
}

def cleanupBuilds(artifactory_server, artifactory_repo, jobToCheck, artifactory_num_artifacts, artifactoryCreds){
    // This parameter is originally a string and needs to be casted as an Integer
    def artifactory_max_num_artifacts = artifactory_num_artifacts as Integer 

    stage('Discover Stored Artifacts'){
        echo "Cleaning up ${jobToCheck}"
        echo "Keeping the latest ${artifactory_max_num_artifacts} builds"
        currentBuild.description = "Keeping ${artifactory_max_num_artifacts} builds of ${jobToCheck}"

        def request = httpRequest authentication: artifactoryCreds, consoleLogResponseBody: true, validResponseCodes: '200,404', url: "${artifactory_server}/api/storage/${artifactory_repo}/${jobToCheck}" 

        requestStatus = request.getStatus()
        if (requestStatus == 200){
            data = readJSON text: request.getContent()
            numberOfArtifacts = data.children.size()
            echo "There are ${numberOfArtifacts} builds"
        } else {
            currentBuild.description = "Warning: The URL does not exist<br>" + currentBuild.description
            echo 'Warning: This URL does not exist... EXITING'
            return
        }
    }
    if (requestStatus == 404){
        return
    }
    stage('Delete Old Artifacts'){
        def amount_deleted = numberOfArtifacts - artifactory_max_num_artifacts
        if (amount_deleted > 0){
            def folderNames = getFolderNumbers(data.children.uri)
            for(i=0; i  < amount_deleted; i++){
                echo "Deleting Build #${folderNames[i]}"
                httpRequest authentication: artifactoryCreds, httpMode: 'DELETE', consoleLogResponseBody: true, url: "${artifactory_server}/${env.ARTIFACTORY_REPO}/${jobToCheck}/${folderNames[i]}"
            }
        } else {
            echo 'There are no artifacts to delete'
            amount_deleted = 0
        } 
        currentBuild.description += "<br>Deleted ${amount_deleted} artifacts"
    }
}

def getFolderNumbers(folderURI){
    def folderNumbers = []
    folderURI.each{
        folderNumbers.add(it.minus('/') as int )
    }
    return folderNumbers.sort()
}

def cleanupTime(artifactory_server, artifactory_repo , artifactory_days_to_keep_artifacts, artifactoryCreds){
    stage('Discover Old Artifacts') {
        // This parameter is originally a string and needs to be casted as an Integer
        artifactory_days_to_keep_artifacts = artifactory_days_to_keep_artifacts as Integer
        def date = new Date()
        def current_time = date.getTime()
        def created_before_time = date.minus(artifactory_days_to_keep_artifacts).getTime()
        echo "Getting all artifacts over ${artifactory_days_to_keep_artifacts} days old"
        currentBuild.description = "Deleting build over ${artifactory_days_to_keep_artifacts} days"

        def request = httpRequest authentication: artifactoryCreds, consoleLogResponseBody: true, validResponseCodes: '200,404', url: "${artifactory_server}/api/search/usage?notUsedSince=${current_time}&createdBefore=${created_before_time}&repos=${artifactory_repo}"
        data = readJSON text: request.getContent()
        requestStatus = request.getStatus()
    }
    stage ('Delete Old Artifacts'){
        if (requestStatus == 200){
            echo "There are ${data.results.size()} artifacts over ${artifactory_days_to_keep_artifacts} days old.\nCleaning them up"

            def artifacts_to_be_deleted = data.results.uri
            artifacts_to_be_deleted.each() { uri -> 
                httpRequest authentication: artifactoryCreds, httpMode: 'DELETE', consoleLogResponseBody: true, url: uri.minus('/api/storage')
            }
            echo 'Deleted all the old artifacts'
            currentBuild.description += "<br>Deleted ${artifacts_to_be_deleted.size()} artifacts"
        } else if (requestStatus == 404){
            currentBuild.description += "<br>Deleted 0 artifacts"
            if (data.errors.message.contains("No results found.")){
                echo 'There are no artifacts to delete'
            }else {
                error 'HTTP 404 Not Found. Please check the logs'
            }
        } else {
            error 'Something went terribly wrong. Please check the logs'
        }
    }
}
