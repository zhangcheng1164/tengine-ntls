tengine-ntls
===

为了支持龙芯架构下VMP的国密https，构建 tengine + babassl支持国密双证书

#### 重新编译tengine

tengine-2.3.3.20211208 目录中   
Tengine： 来自于 [Tengine](https://github.com/alibaba/tengine) 的2.3.3版本后的2021/12/08日的最后一个commit（0125093dc81b28724e1fd84fa5f1f248ee27272b）。   
BabaSSL： 来自于 [Babassl](https://github.com/BabaSSL/BabaSSL) 的8.3-stable branch。    
Tengine + Babassl支持国密的思路来自 [Tengine + BabaSSL ，让国密更易用！](https://www.sofastack.tech/tengine-babassl-making-state-secrets-easier-to-use/)

编译构建具体命令参考rpm目录下面的tengine.spec。

rpm构建(不要安装rpmdevtools，在lns8.3有版本依赖冲突)：   
```shell
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}  
yum install gcc gcc-c++ zlib-devel pcre-devel automake autoconf libtool make rpm-build   
tar -xcf tengine-2.3.3.20211208.tar.gz tengine-2.3.3.20211208  
cp tengine-2.3.3.20211208.tar.gz ~/rpmbuild/SOURCES/  
cp rpm/tengine.spec ~/rpmbuild/SPEC/   
rpmbuild -ba ~/rpmbuild/SPEC/tengine.spec    
rpm -ihv tengine-2.3.3.20211208-1.lns8.loongarch64.rpm
```   

#### 国密双证书
需要在一个安装了支持NTLS的BabaSSL的环境中，使用 sm2_double_certs/mkcert.sh 生成相关国密双证书。  
sm2_double_certs/dist 目录中是制作好的给vmp使用的证书，不过注意**证书的有效期是最近一年的**。具体可以参考Babassl。  

dist/    
├── ca.crt &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;#自签CA根证书  
├── ca.key    
├── chain-ca.crt &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;#根CA和二级CA的证书合集   
├── server_enc.crt &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;#NTLS 加密证书   
├── server_enc.key   
├── server_sign.crt &nbsp;&nbsp;&nbsp;&nbsp;#NTLS 签名证书   
├── server_sign.key    
├── subca.crt &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;#二级CA证书   
└── subca.key    





