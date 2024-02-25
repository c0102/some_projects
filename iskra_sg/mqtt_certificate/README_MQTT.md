#MQTT Certificates

To test the TLS connection we use the "openssl" program, both to generate the
certificates and to act as a basic server and the Mosquitto suite (the server
and the mosquitto_sub and mosquitto_pub command-line applications).

##Generate certification authority (CA):

```bash
openssl genrsa -out CA.key
openssl req -config openssl.conf -x509 -new -key CA.key -sha256 -days 365 -out CA.crt
```
And to setup some files needed later:

```bash
touch index.txt index.txt.attr
echo 00000001 > serial
```

##Generate server certificate

Then you can generate a certificate for the server (lets suppose you're using
an IP in a private network, like 10.1.99.10) and sign it with the CA. When
asked for the certificate distinguished name remember to use the IP as the CN
part.

```bash
openssl genrsa -out 10.1.99.10.key   

openssl req -config openssl.conf -new -key 10.1.99.10.key -out 10.1.99.10.csr

openssl ca -config openssl.conf -extensions server_cert -in 10.1.99.10.csr -out 10.1.99.10.crt
```

##Generate client key
```bash
openssl genrsa -out client.key
```

##Generate client certificate

The certificate for the client is almost identical as for the server, but you don't need to provide an IP, just a name.

```bash
openssl req -config openssl.conf -new -key client.key -out client.csr
openssl ca -config openssl.conf -extensions gateway_cert -in client.csr -out client.crt
```

The name passed in the "-extensions" option identifies a set of flags that will
be used as X.509 extensions in the certificate. We have provided a our
openssl.conf as an example.

##Test MQTT TLS connection

Now that you have both certificates and the CA you can start a TLS server and
configure it to *require* a certificate from the client:

```bash
openssl s_server -port 8883 -Verify 1 -cert 10.1.99.10.crt -key 10.1.99.10.key -CAfile CA.crt
```

And test the connection using openssl:

```bash
openssl s_client -connect 10.1.99.10:8883 -cert client.crt -key client.key -CAfile CA.crt
```

After you have checked that the certificates work you can use the server certificate to secure the broker.
To debug the server we usually use mosquitto_sub to subscribe to the wildcard topic over a TLS connection, using the generated certificates:

```bash
mosquitto_sub -h 10.1.99.10 -p 8883 -i TEST -t '#' -cert client.crt -key client.key -cafile CA.crt
```
#Adding certificates to Mosquitto mqtt broker
Copy previously generated certificates to mosquitto configuration folder.

```bash
sudo -s
mkdir -p /etc/mosquitto/certs
cp CA.crt /etc/mosquitto/certs
cp 10.1.99.10* /etc/mosquitto/certs
sudo chmod 777 /etc/mosquitto/certs/*
```

##Configure mosquitto to use certificates
Open mosquitto configuration file:

```bash
vi /etc/mosquitto/mosquitto.conf
```

Enter following paths to certificates

```bash
listener 8883
cafile /etc/mosquitto/certs/CA.crt
certfile /etc/mosquitto/certs/10.1.99.10.crt
keyfile /etc/mosquitto/certs/10.1.99.10.key
require_certificate true
use_identity_as_username true
```
