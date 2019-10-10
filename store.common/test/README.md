__Good openssl coverage:__
https://www.digitalocean.com/community/tutorials/openssl-essentials-working-with-ssl-certificates-private-keys-and-csrs

___Generate self signed cert using private key___
```bash
ssh-keygen -t rsa -b 4096 (id_rsa/id_rsa.pub)

openssl req \
       -key id_rsa \
       -new \
       -x509 -days 365 -out test.pem
```

___Just get the public key___
```bash
openssl x509 -pubkey -noout -in test.pem  > pubkey.pem
```

___Or from a csr___
```bash
openssl req -in csr.txt -noout -pubkey -out pubkey.pem
```

___Can now use the same key to sign the message.___  
Recipient can either use id_rsa.pub or pubkey.pem

__reference:__
https://gist.github.com/ygotthilf/baa58da5c3dd1f69fae9

```bash
openssl genrsa -out private.pem 4096
openssl rsa -in private.pem -pubout -out public.pem
```

___To encode the private key in single line___
```bash
cat jwtRS256.key | base64
```