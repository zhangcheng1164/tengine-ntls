Name:           tengine
Version:        2.3.3.20211208
Release:	1%{?dist}
Summary:        tengine with NTLS	

License:        2-clause BSD-like license
URL:            http://tengine.taobao.org/
SOURCE:         tengine-2.3.3.20211208.tar.gz
#BuildRequires:	
#Requires:	

%description
tengine with NTLS 


%prep
%setup -q

%post -p /bin/sh
# Register the tengine service
if [ $1 -eq 1 ]; then
    /usr/bin/systemctl preset tengine.service >/dev/null 2>&1
fi


%build
cd ./tengine
./configure --add-module=modules/ngx_openssl_ntls \
    --without-http_gzip_module \
    --with-openssl=../BabaSSL-8.3-stable \
    --with-openssl-opt="--strict-warnings enable-ntls" \
    --with-http_ssl_module --with-stream \
    --with-stream_ssl_module --with-stream_sni \
    --prefix=%{_datadir}/tengine 
make %{?_smp_mflags}


%install
mkdir -p %{buildroot}%{_unitdir}
cp ./rpm/tengine.service %{buildroot}%{_unitdir}
cd ./tengine
make DESTDIR=%{buildroot} install

%files
%defattr (-,root,root,0755)
%{_datadir}/tengine/*
%{_unitdir}/tengine.service


