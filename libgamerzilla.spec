Summary: Gamerzilla Integration Library
Name: libgamerzilla
Version: 0.0.1
Release: 1%{?dist}
License: LGPLv2+
URL: https://github.com/dulsi/libgamerzilla
Source0: http://www.identicalsoftware.com/gamerzilla/%{name}-%{version}.tgz
BuildRequires: gcc
BuildRequires: jansson-devel
BuildRequires: libcurl-devel
BuildRequires: cmake

%description
Gamerzilla is trophy/achievement system for games. It intergrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

%package devel
Summary: Libraries and includes for Gamerzilla development
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: SDL2-devel%{?_isa}
Requires: pkgconfig

%description devel
This package contains libraries and header files for
developing applications that use gamerzilla.

%prep
%setup -q

%build
%cmake
make %{?_smp_mflags}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%doc README
%license LICENSE
%{_libdir}/lib*.so.*

%files devel
%{_includedir}/gamerzilla/*
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/gamerzilla.pc

%changelog
* Sun May 24 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.1-1
- Initial spec
