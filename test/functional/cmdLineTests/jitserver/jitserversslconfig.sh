#!/bin/bash
# Set certificate details
COMMON_NAME="localhost"
VALID_DAYS=365

# Generate private key
openssl genrsa -out key.pem 2048

# Generate self-signed certificate
openssl req -new -x509 -sha256 -key key.pem -out cert.pem -days $VALID_DAYS -subj "/CN=$COMMON_NAME"

# Generate another private key and self-signed certificate
openssl req -nodes -newkey rsa:2048 -keyout wrongKey.pem -x509 -days 365 -out wrongCert.pem  -subj "/CN=localhost"

# Generate another self-signed certificate
openssl req -new -x509 -sha256 -key key.pem -out nosslserverCert.pem -days $VALID_DAYS -subj "/CN=$COMMON_NAME"
echo "Certificates generated"
