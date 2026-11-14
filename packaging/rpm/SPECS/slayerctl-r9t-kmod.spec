# SPDX-License-Identifier: MIT
# Headers and compilation stay on COPR. Each binary module names its exact ABI.
%global debug_package %{nil}
%undefine _auto_set_build_flags
%global kver %(v=$(rpm -q --qf '%%{VERSION}-%%{RELEASE}.%%{ARCH}\\n' kernel-devel 2>/dev/null); if test $? -eq 0; then echo "$v" | sort -V | tail -n1; else echo 0; fi)
%global copr_build 0
Name: slayerctl-r9t-kmod
Version: 0.1.5
Release: 1.%{copr_build}%{?dist}
Summary: Exact-model Slayer R9T controls and hwmon bridge
License: GPL-2.0-only
URL: https://github.com/MCC45TR/gg-slayer
Packager: MCC45TR <mcc45tr@gmail.com>
Source0: slayerctl_r9t.tar.gz
ExclusiveArch: x86_64
BuildRequires: gcc, make, kernel-devel, elfutils-libelf-devel
Requires: slayerctl >= 0.1.3
Requires: kmod-slayerctl-r9t-%{kver} = %{version}-%{release}
Provides: slayerctl-r9t-kmod-common = %{version}-%{release}
Obsoletes: slayerctl-r9t-kmod-common < %{version}-%{release}
Obsoletes: akmod-slayerctl-r9t < %{version}-%{release}
Provides: kmod-slayerctl-r9t = %{version}-%{release}
Obsoletes: kmod-slayerctl-r9t < %{version}-%{release}
%description
Prebuilt Exact-model Slayer R9T controls and hwmon bridge. All compilation occurs on COPR; no akmods or local build
runs on installation. The kernel-specific payload remains available for its ABI.

%package -n kmod-slayerctl-r9t-%{kver}
Summary: Exact-model Slayer R9T controls and hwmon bridge for %{kver}
Requires: kernel-uname-r = %{kver}
Requires: slayerctl >= 0.1.3
Provides: kernel-modules-for-kernel = %{kver}
# The name already includes the kernel ABI: retain different-ABI packages,
# but upgrade this ABI's payload normally. installonly would co-install two
# versions owning the same .ko and prevent migration from the local packages.
%description -n kmod-slayerctl-r9t-%{kver}
Binary module built on COPR against the exact %{kver} headers.

%prep
%setup -q -c -n slayerctl_r9t
%build
%make_build -C /usr/src/kernels/%{kver} M="$PWD" modules
%install
install -D -m0644 slayerctl_r9t.ko %{buildroot}%{_prefix}/lib/modules/%{kver}/extra/slayerctl-r9t/slayerctl_r9t.ko
install -d %{buildroot}%{_sysconfdir}/modules-load.d
printf 'slayerctl_r9t\n' > %{buildroot}%{_sysconfdir}/modules-load.d/slayerctl-r9t.conf
%posttrans -n kmod-slayerctl-r9t-%{kver}
# Signing with an already enrolled local MOK is not compilation. No new key,
# enrollment, firmware write, or Secure Boot change is made here.
%{_libexecdir}/slayerctl-kmod-activate '%{kver}' 'slayerctl-r9t' || :
%postun -n kmod-slayerctl-r9t-%{kver}
/usr/sbin/depmod -a '%{kver}' >/dev/null 2>&1 || :
%files
%license COPYING
%config(noreplace) %{_sysconfdir}/modules-load.d/slayerctl-r9t.conf
%files -n kmod-slayerctl-r9t-%{kver}
%{_prefix}/lib/modules/%{kver}/extra/slayerctl-r9t/
%changelog
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.4-1
- Diagnose the BIOS read mailbox for the fixed project and stored limit fields.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.3-1
- Add root-only fixed-address charging diagnostics without new firmware writes.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.2-1
- Expose only root-readable activity timing from the physical internal keyboard.
- Passive observation and one-shot notifications without exporting key contents.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.1-3
- Repeat activation with standard Fedora kmodgenca link and permission support.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.1-2
- Allow normal DNF upgrades of the payload for the same kernel ABI.
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.1-1
- Build binary modules on COPR and sign with an existing enrolled MOK at install.
