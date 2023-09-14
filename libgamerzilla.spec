Summary: Gamerzilla Integration Library
Name: libgamerzilla
Version: 0.1.3
Release: %autorelease
License: zlib
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

%package server
Summary: Simple Gamerzilla server to relay information to Hubzilla

%description server
The gamerzillaserver listens for trophies awarded by games. It logs into
the user's Hubzilla server and passes on the awards.

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

%files server
%{_bindir}/gamerzillaserver

%changelog
%autochangelog
