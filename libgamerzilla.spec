Summary: Gamerzilla Integration Library
Name: libgamerzilla
Version: 0.0.1
Release: 2%{?dist}
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
%make_build

%install
%make_install

%files
%doc README
%license LICENSE
%{_libdir}/libgamerzilla.so.0
%{_libdir}/libgamerzilla.so.0.1.0

%files devel
%{_includedir}/gamerzilla/
%{_libdir}/libgamerzilla.so
%{_libdir}/pkgconfig/gamerzilla.pc

%changelog
* Tue Jun 30 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.1-2
- Specified files more percisely
- Update to latest macro usage

* Sun May 24 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.1-1
- Initial spec
