Summary: Simple DirectMedia Layer - MNG Loading Library
Name: SDL_mng
Version: 0.2.7
Release: 3%{?dist}
License: LGPLv2+
URL: https://github.com/dulsi/SDL_mng
Source0: http://www.identicalsoftware.com/btbuilder/%{name}-%{version}.tgz
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: SDL2-devel
BuildRequires: libpng-devel
BuildRequires: SDL2_image-devel
BuildRequires: cmake

%description
This is a simple library to load mng animations as SDL surfaces.

%package devel
Summary: Libraries and includes for SDL MNG development
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: SDL2-devel%{?_isa}
Requires: pkgconfig

%description devel
This package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q

%build
export CXXFLAGS="%{optflags} -Wl,--as-needed"
%cmake
make %{?_smp_mflags}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%doc README ChangeLog
%license LICENSE
%{_libdir}/lib*.so.*

%files devel
%{_includedir}/SDL2/*
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/%{name}.pc

%changelog
* Sun Feb 18 2018 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.7-3
- Add build requirement of gcc-c++.

* Sun Oct 09 2016 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.7-2
- Add requires cmake to build

* Sun Oct 09 2016 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.7-1
- Convert to cmake

* Fri Aug 26 2016 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.6-2
- Fix unused direct shlib dependency

* Sat Aug 13 2016 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.6-1
- Updated to new version

* Tue Jul 12 2016 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.5-1
- Updated to new version

* Wed Oct 28 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.4-2
- Fix typo in requires

* Wed Sep 23 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.4-1
- Added pckconfig file
- Removed Group tags
- Changed description of devel package
- Reordered post and postun section

* Sat Aug 15 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.3-1
- Do not package static libraries
- Made the requirement for the devel package specific to the version.
- Specify the license file properly.
- Post install ldconfig step added.

* Wed Jun 3 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.2-3
- Do not package .la files
- Use make_install instead of makeinstall

* Sun May 31 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.2-2
- Fixed source file to tgz not tar.gz

* Sun May 31 2015 Dennis Payne <dulsi@identicalsoftware.com> - 0.2.2-1
- Initial spec
