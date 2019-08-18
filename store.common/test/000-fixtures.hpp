#pragma once

/*
Good openssl coverage:
https://www.digitalocean.com/community/tutorials/openssl-essentials-working-with-ssl-certificates-private-keys-and-csrs

ssh-keygen -t rsa -b 4096 (id_rsa/id_rsa.pub)

# Generate self signed cert using private key

openssl req \
       -key id_rsa \
       -new \
       -x509 -days 365 -out test.pem

# Just get the public key
openssl x509 -pubkey -noout -in test.pem  > pubkey.pem

# Or from a csr
openssl req -in csr.txt -noout -pubkey -out pubkey.pem

# Can now use the same key to sign the message.  Recipient can either use id_rsa.pub or pubkey.pem
*/

/*
reference: https://gist.github.com/ygotthilf/baa58da5c3dd1f69fae9

ssh-keygen -t rsa -P "" -b 4096 -m PEM -f jwtRS256.key
ssh-keygen -e -m PEM -f jwtRS256.key > jwtRS256.key.pub

-- or --

openssl genrsa -out private.pem 2048
openssl rsa -in private.pem -pubout -out public.pem

To encode the private key in single line

cat jwtRS256.key | base64

*/

#include <iostream>

using namespace std;

namespace test::jwt {

// An arbitrary key created by ssh-keygen.
string privateKey = 
R"(
-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)";

// This public key was generated using a self signed cert with the above private key.
string publicKeyPem = 
R"(
-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA4A8K9oaZJf84Ur1bwg9F
APx5UtiYTv/zlmCgltyPBWrKTxfA4Eg396U3VpeA2toD6GGI4ddABxHswRPCIl93
/5xipl/WA0cU1ThRnmJJx5QQEOdIXNgSC1dPfw2uEgg2hsGLgOBpW7WBD9hPA2nE
VKK4Y2Gi1vWMcQO2tSCq3jguYef0ta2TLY0YvHLYx0KBKDIP2AXBDKnhGMSP1jCH
d6h+K1SDg+4CL3dtADxJ9AXCAd2sD6h80U5oe/9DW6VsHYv5sPMIx3/MvcJ+fYP2
tszqfnalthimO/rAFuOEUTgi6jiRtg7Rv5bBtNdI6czSZg4NfRpUU0N2uNH1KSV1
Rymi183cT9Mqsh1w9vukfVbBJRPHgKqqdkicpwrWDHei4WJQ2I4gJz14y7kQp1f1
SBv4PKf3AdEpUiIPtMjRCNdr+z0kEUuLTv2IuqdkIjUe2VRoMRnOXZ3O1XlGfSU+
F8lrrGbrXoAoWNDX1nGAkX+SyVzeF6ULzp4C2k2Dsb6IxoO65SKU6mt5vpsvqPT6
q3j2F2/Zx/X8K6RptAF1/w0tt4G51IBHMDJqy5avM7yACPeohbN4qdfsl4nAA22b
U5n0jdOZJJ3HtCX2rjrD5NvMIME+YKYKT5zPga5SQyRDLXs/gIxxo7wAppplWKDZ
HgWXzi8l6pHhDqXBPUyHACMCAwEAAQ==
-----END PUBLIC KEY-----
)";

}
