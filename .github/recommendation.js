const axios = require('axios');
const core = require('@actions/core');
const github = require('@actions/github');

async function run() {
    const sandboxIssueNumber = 19673;
    const sandboxOwner = 'eclipse-openj9';
    const sandboxRepo = 'openj9';

    const input = {
        issue_title: process.env.ISSUE_TITLE,
        issue_description: process.env.ISSUE_DESCRIPTION,
    };

    const apiUrl = "http://140.211.168.122/recommendation";
    const octokit = new github.getOctokit(process.env.GITHUB_TOKEN);

    try {
        const response = await axios.post(apiUrl, JSON.stringify(input), {
            headers: {
                'Accept': 'application/json',
                'Content-Type': 'application/json'
            }
        });

        const issueComment = response.data;
        let predictedAssignees = issueComment.recommended_developers;
        let predictedLabels = issueComment.recommended_components;

        let resultString = `Issue Number: ${process.env.ISSUE_NUMBER}\n`;
        resultString += 'Status: Open\n';
        resultString += `Recommended Components: ${predictedLabels.join(', ')}\n`;
        // Temporarily disable assignees predicition due to https://github.com/eclipse-openj9/openj9/issues/20489
        // resultString += `Recommended Assignees: ${predictedAssignees.join(', ')}\n`;

        await octokit.rest.issues.createComment({
            issue_number: sandboxIssueNumber,
            owner: sandboxOwner,
            repo: sandboxRepo,
            body: resultString
        });
    } catch (error) {
        core.setFailed(`Action failed with error: ${error}`);
        await octokit.rest.issues.createComment({
            issue_number: sandboxIssueNumber,
            owner: sandboxOwner,
            repo: sandboxRepo,
            body: `The TriagerX model is currently not responding to the issue ${process.env.ISSUE_NUMBER}. Please try again later.`
        });
    }
}
run();
