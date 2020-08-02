%?mingw_package_header

Summary: MinGW Windows port of Gamerzilla Integration Library
Name: mingw-libgamerzilla
Version: 0.0.4
Release: 1%{?dist}
License: LGPLv2+
URL: https://github.com/dulsi/libgamerzilla
Source0: http://www.identicalsoftware.com/gamerzilla/libgamerzilla-%{version}.tgz
BuildArch:      noarch
BuildRequires: mingw32-filesystem >= 95
BuildRequires: mingw32-gcc
BuildRequires: mingw32-binutils
BuildRequires: mingw32-jansson
BuildRequires: mingw32-curl

BuildRequires: mingw64-filesystem >= 95
BuildRequires: mingw64-gcc
BuildRequires: mingw64-binutils
BuildRequires: mingw64-jansson
BuildRequires: mingw64-curl

%description
Gamerzilla is trophy/achievement system for games. It integrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

# Win32
%package -n mingw32-libgamerzilla
Summary:        MinGW Windows port of Gamerzilla Integration Library
Requires:       pkgconfig

%description -n mingw32-libgamerzilla
Gamerzilla is trophy/achievement system for games. It integrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

# Win64
%package -n mingw64-libgamerzilla
Summary:        MinGW Windows port of Gamerzilla Integration Library
Requires:       pkgconfig

%description -n mingw64-libgamerzilla
Gamerzilla is trophy/achievement system for games. It integrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

%?mingw_debug_package

%prep
%setup -q -n libgamerzilla-%{version}

%build
%mingw_cmake
%mingw_make %{?smp_mflags}

%install
%mingw_make DESTDIR=%{buildroot} install

# Win32
%files -n mingw32-libgamerzilla
%doc README
%license LICENSE
%{mingw32_libdir}/libgamerzilla.dll
%{mingw32_libdir}/libgamerzilla.dll.a
%{mingw32_libdir}/pkgconfig/gamerzilla.pc
%{mingw32_includedir}/gamerzilla/

# Win64
%files -n mingw64-libgamerzilla
%doc README
%license LICENSE
%{mingw64_libdir}/libgamerzilla.dll
%{mingw64_libdir}/libgamerzilla.dll.a
%{mingw64_libdir}/pkgconfig/gamerzilla.pc
%{mingw64_includedir}/gamerzilla/

%changelog
* Tue Jul 21 2020 Dennis Payne <dulsi@identicalsoftware.com> - 0.0.4-1
- Update to newest version
