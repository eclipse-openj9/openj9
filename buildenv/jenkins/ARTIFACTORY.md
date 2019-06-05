<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->
# About
Artifactory is a binary repository manager. 
Our server is being used to store the Jenkins artifacts between builds to save space on the Jenkins master. The artifacts will be kept for a period after the job in case someone needs to debug the build.
It is being hosted on the proxy worker machine and the current address is https://140-211-168-230-openstack.osuosl.org/artifactory/webapp/

# Install Artifactory: 

Download Artifactory from JFrog (https://jfrog.com/open-source/)

`https://api.bintray.com/content/jfrog/artifactory/jfrog-artifactory-oss-$latest.zip;bt_package=jfrog-artifactory-oss-zip`

Once Artifactory is downloaded, unzip the file to a directory.  

`unzip jfrog-artifactory-oss-zip -d /home/jenkins/artifactory` 

Edit the file in /home/jenkins/artifactory/etc/binarystore.xml and create the following configuration

```
<config version="1">
    <chain template="file-system"/>
    <provider id="file-system" type="file-system">
        <baseDataDir>/home/jenkins/artifact_storage/data</baseDataDir>
        <fileStoreDir>/home/jenkins/artifact_storage/filestore</fileStoreDir>
        <tempDir>/home/jenkins/artifact_storage/temp</tempDir>
    </provider>
</config>
```

# Reverse Proxy 
Next is to install a reverse proxy. In this case, NGINX was installed 
A Reverse Proxy was used so that https can be configured. 

To install NGINX use this command
` sudo apt install nginx`

Then the reverse configuration needs to be set
The template for the reverse proxy was taken from https://www.jfrog.com/confluence/display/RTF/Configuring+NGINX and there were some changes to it. 
This is the copy that is currently being used 
```
## server configuration
server {

    server_name 140-211-168-230-openstack.osuosl.org;
    if ($http_x_forwarded_proto = '') {
        set $http_x_forwarded_proto  $scheme;
    }

    ## Application specific logs
    ## access_log /var/log/nginx/artifactory.log timing;
    ## error_log /var/log/nginx/artifactory-error.log;
    rewrite ^/$ /artifactory/webapp/ redirect;
    rewrite ^/artifactory/?(/webapp)?$ /artifactory/webapp/ redirect;
    chunked_transfer_encoding on;
    client_max_body_size 0;
    location /artifactory/ {
    proxy_read_timeout  900;
    proxy_pass_header   Server;
    proxy_cookie_path   ~*^/.* /;
    if ( $request_uri ~ ^/artifactory/(.*)$ ) {
        proxy_pass          http://localhost:8081/artifactory/$1;
    }
    proxy_pass         http://localhost:8081/artifactory/;
    proxy_set_header    X-Artifactory-Override-Base-Url $http_x_forwarded_proto://$host:$server_port/artifactory;
    proxy_set_header    X-Forwarded-Port  $server_port;
    proxy_set_header    X-Forwarded-Proto $http_x_forwarded_proto;
    proxy_set_header    Host              $http_host;
    proxy_set_header    X-Forwarded-For   $proxy_add_x_forwarded_for;
    }

    listen [::]:443 ssl ipv6only=on; # managed by Certbot
    listen 443 ssl; # managed by Certbot
    ssl_certificate /etc/letsencrypt/live/140-211-168-230-openstack.osuosl.org/fullchain.pem; # managed by Certbot
    ssl_certificate_key /etc/letsencrypt/live/140-211-168-230-openstack.osuosl.org/privkey.pem; # managed by Certbot
    include /etc/letsencrypt/options-ssl-nginx.conf; # managed by Certbot
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem; # managed by Certbot

}

server {
    if ($host = 140-211-168-230-openstack.osuosl.org) {
        return 301 https://$host$request_uri;
    } # managed by Certbot


    listen 80 ;
    listen [::]:80 ;

    server_name 140-211-168-230-openstack.osuosl.org;
    return 404; # managed by Certbot
}
```
All of the lines with the `# managed by Certbot` were added when adding in the ssl certificate. 

Those lines do not need to be added as they were automatically added by certbot

I am getting my certificate from Let’s Encrypt and their install tool certbot. Firstly, I installed certbot:
```
sudo apt-get update
sudo apt-get install software-properties-common
sudo add-apt-repository universe
sudo add-apt-repository ppa:certbot/certbot
sudo apt-get update
sudo apt-get install certbot python-certbot-nginx
```
Then I used the certbot automated script to add the certificates and to automatically renew the certificates: 
```
sudo certbot –nginx
```
Follow the onscreen instructions and everything should work out. 

# Open the Ports
Log into OpenStack (https://openpower-controller.osuosl.org/auth/login/?next=/project/) and go to network security groups
Click on `http` (if it is not created, create the rule)
Create a new rule with these settings
```
Custome TCP Rule
Ingres
Port
443
CIDR
0.0.0.0/0
```
Create another rule for port 80 if you enabled port forward in the certbot step

Then go to compute -> instances and click on ` eclipse-openj9-proxy`

Edit instance -> security group
Add http to the instance security groups



# Configure Artifactory 
To start the Artifactory server, run the command:
```
/home/jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactoryctl start
```

On your local browser, go to `http://140-211-168-230-openstack.osuosl.org/` and follow the onscreen instructions.
The admin password is in the secrets repo and the repository that is used is ‘generic’ with the name being “ci-eclipse-openj9”. 

Next, click on the `Welcome, admin` tab and select `Add User`
Insert the username `Jenkins` and the email address `j9build@ca.ibm.com`. Insert the password as indicated in the secrets file.

Now click on the `Welcome, admin` tab and select `Add Permission` 
Give the permission a name like `Jenkins-perm` and include the `ci-eclipse-openj9` repository. Click on Users and then add Jenkins. Give Jenkins all of the permissions except manage and save. 

Under the Admin tab on the left side, select Services -> Backups and make sure that they are disabled. To see if they are disabled, click on the backup and see if the enabled button is checked. If it is, uncheck it and save. The backups take up too much memory for what ends up being needed. 

Now logoff of the admin account and log into the Jenkins account. Edit the profile of Jenkins by clicking on Welcome, Jenkins -> Edit Profile. Insert the password to unlock the account and generate the API key. Copy that key to the secrets repo and update Jenkins so that it can upload artifacts. 


I also added in a separate shell command to restart Artifactory. 
The command is artifactory_restart.sh and contains 
```
#!/bin/bash

/home/jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactoryctl stop
/home/jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactoryctl start
```
A cron job was later added to call this command @midnight
The command used is `crontab -e`
This line was added 
```
@reboot bash /home/jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactory_restart.sh
```


# Useful Links

https://www.jfrog.com/confluence/display/RTF/Installing+on+Linux+Solaris+or+Mac+OS#InstallingonLinuxSolarisorMacOS-ManualInstallation

https://www.jfrog.com/confluence/display/RTF/Configuring+the+Filestore

https://www.jfrog.com/confluence/display/RTF/Configuring+NGINX 

https://certbot.eff.org/lets-encrypt/ubuntuxenial-nginx


# Useful Commands

Start: `/home/jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactoryctl start`

Stop: `/home/Jenkins/artifactory/artifactory-oss-6.5.2/bin/artifactoryctl stop`

