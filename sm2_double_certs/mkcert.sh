# export PATH=/usr/local/babassl-share/bin/:$PATH

rm -rf {newcerts,db,private,crl}
mkdir {newcerts,db,private,crl}
touch db/{index,serial}
echo 00 > db/serial

# sm2 ca
openssl ecparam -genkey -name SM2 -outform pem -out ca.key

openssl req -config ca.cnf -new -key ca.key -out ca.csr -sm3 -nodes -sigopt "sm2_id:1234567812345678" -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=root ca"

openssl ca -selfsign -config ca.cnf -in ca.csr -keyfile ca.key -extensions v3_ca -days 3650 -notext -out ca.crt -md sm3 -batch

# sm2 middle ca
openssl ecparam -genkey -name SM2 -out subca.key

openssl req -config ca.cnf -new -key subca.key -out subca.csr -sm3 -sigopt "sm2_id:1234567812345678" -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=sub ca"

openssl ca -config ca.cnf -extensions v3_intermediate_ca -days 3650 -in subca.csr -notext -out subca.crt -md sm3 -batch

cat ca.crt subca.crt > chain-ca.crt

# server sm2 double certs
openssl ecparam -name SM2 -out server_sm2.param

openssl req -config subca.cnf -newkey ec:server_sm2.param -nodes -keyout server_sign.key -sm3 -sigopt "sm2_id:1234567812345678" -new -out server_sign.csr -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=server sign"

openssl ca -config subca.cnf -extensions server_sign_req -days 3650 -in server_sign.csr -notext -out server_sign.crt -md sm3 -batch

openssl req -config subca.cnf -newkey ec:server_sm2.param -nodes -keyout server_enc.key -sm3 -sigopt "sm2_id:1234567812345678" -new -out server_enc.csr -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=server enc"

openssl ca -config subca.cnf -extensions server_enc_req -days 3650 -in server_enc.csr -notext -out server_enc.crt -md sm3 -batch

# client sm2 double certs
openssl ecparam -name SM2 -out client_sm2.param

openssl req -config subca.cnf -newkey ec:client_sm2.param -nodes -keyout client_sign.key -sm3 -sigopt "sm2_id:1234567812345678" -new -out client_sign.csr -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=client sign"

openssl ca -config subca.cnf -extensions client_sign_req -days 3650 -in client_sign.csr -notext -out client_sign.crt -md sm3 -batch

openssl req -config subca.cnf -newkey ec:client_sm2.param -nodes -keyout client_enc.key -sm3 -sigopt "sm2_id:1234567812345678" -new -out client_enc.csr -subj "/C=AA/ST=BB/O=CC/OU=DD/CN=client enc"

openssl ca -config subca.cnf -extensions client_enc_req -days 3650 -in client_enc.csr -notext -out client_enc.crt -md sm3 -batch
