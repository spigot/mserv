Summary: music server
Name: mserv
Version: 0.31
Release: 1
Group: Networking/Daemons
Source: http://www.mserv.org/download/mserv-0.31.tar.gz
Requires: mpg123
Copyright: GPL
Buildroot: /tmp
%description
local centralised multiuser music server environment
%prep
%setup
%build
cd mserv
./configure --prefix /usr/ --program-prefix=$RPM_BUILD_ROOT
make
cd ../mservcli
./configure --prefix /usr --program-prefix=$RPM_BUILD_ROOT
make
cd ../mservutils
./configure --prefix /usr --program-prefix=$RPM_BUILD_ROOT
make
%install
cd mserv
make install
cd ../mservcli
make install
cd ../mservutils
make install
%files
%doc mserv/CHANGES
/usr/man/man1/mserv.1
/usr/bin/mserv
/usr/bin/mservedit
/usr/bin/mservplay
/usr/share/mserv/
/usr/lib/libmservcli.a
/usr/lib/libmservplus.a
/usr/include/mservcli.h
/usr/bin/mservcmd
/usr/man/man1/mservcmd.1
