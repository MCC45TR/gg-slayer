# SPDX-License-Identifier: MIT
# Headers and compilation stay on COPR. Each binary module names its exact ABI.
%global debug_package %{nil}
%undefine _auto_set_build_flags
%global kver %(v=$(rpm -q --qf '%%{VERSION}-%%{RELEASE}.%%{ARCH}\\n' kernel-devel 2>/dev/null); if test $? -eq 0; then echo "$v" | sort -V | tail -n1; else echo 0; fi)
%global copr_build 0
Name: ite8291r3-kmod
Version: 0.0.20261114.2
Release: 1.%{copr_build}%{?dist}
Summary: Native ITE keyboard LED driver with quiet idle brightness
License: GPL-2.0-or-later
URL: https://github.com/MCC45TR/gg-slayer
Packager: MCC45TR <mcc45tr@gmail.com>
Source0: ite_8291.tar.gz
ExclusiveArch: x86_64
BuildRequires: gcc, make, kernel-devel, elfutils-libelf-devel
Requires: slayerctl >= 0.1.3
Requires: kmod-ite8291r3-%{kver} = %{version}-%{release}
Provides: ite8291r3-kmod-common = %{version}-%{release}
Obsoletes: ite8291r3-kmod-common < %{version}-%{release}
Obsoletes: akmod-ite8291r3 < %{version}-%{release}
Provides: kmod-ite8291r3 = %{version}-%{release}
Obsoletes: kmod-ite8291r3 < %{version}-%{release}
%description
Prebuilt Native ITE keyboard LED driver with quiet idle brightness. All compilation occurs on COPR; no akmods or local build
runs on installation. The kernel-specific payload remains available for its ABI.

%package -n kmod-ite8291r3-%{kver}
Summary: Native ITE keyboard LED driver with quiet idle brightness for %{kver}
Requires: kernel-uname-r = %{kver}
Requires: slayerctl >= 0.1.3
Provides: kernel-modules-for-kernel = %{kver}
# The name already includes the kernel ABI: retain different-ABI packages,
# but upgrade this ABI's payload normally. installonly would co-install two
# versions owning the same .ko and prevent migration from the local packages.
%description -n kmod-ite8291r3-%{kver}
Binary module built on COPR against the exact %{kver} headers.

%prep
%setup -q -c -n ite_8291
%build
%make_build -C /usr/src/kernels/%{kver} M="$PWD" modules
%install
install -D -m0644 ite_8291.ko %{buildroot}%{_prefix}/lib/modules/%{kver}/extra/ite8291r3/ite_8291.ko
install -d %{buildroot}%{_sysconfdir}/modules-load.d
printf 'ite_8291\n' > %{buildroot}%{_sysconfdir}/modules-load.d/ite8291r3.conf
%posttrans -n kmod-ite8291r3-%{kver}
# Signing with an already enrolled local MOK is not compilation. No new key,
# enrollment, firmware write, or Secure Boot change is made here.
%{_libexecdir}/slayerctl-kmod-activate '%{kver}' 'ite8291r3' || :
%postun -n kmod-ite8291r3-%{kver}
/usr/sbin/depmod -a '%{kver}' >/dev/null 2>&1 || :
%files
%license COPYING
%config(noreplace) %{_sysconfdir}/modules-load.d/ite8291r3.conf
%files -n kmod-ite8291r3-%{kver}
%{_prefix}/lib/modules/%{kver}/extra/ite8291r3/
%changelog
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.0.20261114.2-1
- Interpolate idle fades to the measured visible 2/50 hardware minimum.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.0.20261114.1-3
- Repeat activation with standard Fedora kmodgenca link and permission support.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.0.20261114.1-2
- Allow normal DNF upgrades of the payload for the same kernel ABI.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.0.20261114.1-1
- Build binary modules on COPR and sign with an existing enrolled MOK at install.
