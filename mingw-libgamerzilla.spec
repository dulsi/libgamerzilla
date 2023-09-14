%?mingw_package_header

Summary: MinGW Windows port of Gamerzilla Integration Library
Name: mingw-libgamerzilla
Version: 0.1.3
Release: %autorelease
License: zlib
URL: https://github.com/dulsi/libgamerzilla
Source0: http://www.identicalsoftware.com/gamerzilla/libgamerzilla-%{version}.tgz
BuildArch:      noarch
BuildRequires: cmake
BuildRequires: mingw32-filesystem >= 95
BuildRequires: mingw32-gcc
BuildRequires: mingw32-gcc-c++
BuildRequires: mingw32-binutils
BuildRequires: mingw32-jansson
BuildRequires: mingw32-curl

BuildRequires: mingw64-filesystem >= 95
BuildRequires: mingw64-gcc
BuildRequires: mingw64-gcc-c++
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

%description -n mingw32-libgamerzilla
Gamerzilla is trophy/achievement system for games. It integrates with a
hubzilla plugin to display your results online. Games can either connect
directly to hubzilla or connect to a game launcher with using this
library.

# Win64
%package -n mingw64-libgamerzilla
Summary:        MinGW Windows port of Gamerzilla Integration Library

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
mkdir %{buildroot}/%{mingw32_bindir}
mkdir %{buildroot}/%{mingw64_bindir}
mv %{buildroot}/%{mingw32_libdir}/libgamerzilla.dll %{buildroot}/%{mingw32_bindir}/libgamerzilla.dll
mv %{buildroot}/%{mingw64_libdir}/libgamerzilla.dll %{buildroot}/%{mingw64_bindir}/libgamerzilla.dll

# Win32
%files -n mingw32-libgamerzilla
%doc README
%license LICENSE
%{mingw32_bindir}/libgamerzilla.dll
%{mingw32_libdir}/libgamerzilla.dll.a
%{mingw32_libdir}/pkgconfig/gamerzilla.pc
%{mingw32_includedir}/gamerzilla/

# Win64
%files -n mingw64-libgamerzilla
%doc README
%license LICENSE
%{mingw64_bindir}/libgamerzilla.dll
%{mingw64_libdir}/libgamerzilla.dll.a
%{mingw64_libdir}/pkgconfig/gamerzilla.pc
%{mingw64_includedir}/gamerzilla/

%changelog
%autochangelog
