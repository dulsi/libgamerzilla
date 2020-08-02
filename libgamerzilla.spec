Summary: Gamerzilla Integration Library
Name: libgamerzilla
Version: 0.0.5
Release: 1%{?dist}
License: LGPLv2+
URL: https://github.com/dulsi/libgamerzilla
Source0: http://www.identicalsoftware.com/gamerzilla/%{name}-%{version}.tgz
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: jansson-devel
BuildRequires: libcurl-devel
BuildRequires: cmake

%description
Gamerzilla is trophy/achievement system for games. It integrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

%package devel
Summary: Libraries and includes for Gamerzilla development
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: pkgconfig

%description devel
This package contains libraries and header files for
developing applications that use gamerzilla.

%prep
%setup -q

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%doc README
%license LICENSE
%{_libdir}/libgamerzilla.so.0
%{_libdir}/libgamerzilla.so.0.1.0

%files devel
%{_includedir}/gamerzilla/
%{_libdir}/libgamerzilla.so
%{_libdir}/pkgconfig/gamerzilla.pc
%{_datadir}/vala/vapi/gamerzilla.vapi
%{_datadir}/vala/vapi/gamerzilla.deps

%changelog
* Sun Aug 02 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.5-1
- Update to newest version

* Tue Jul 21 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.4-1
- Update to newest version

* Thu Jul 09 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.3-1
- Update to newest version

* Thu Jul 09 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.2-1
- Update to newest version

* Tue Jun 30 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.1-2
- Specified files more precisely
- Update to latest macro usage

* Sun May 24 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.1-1
- Initial spec
