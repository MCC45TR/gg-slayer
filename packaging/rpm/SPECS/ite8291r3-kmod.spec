%global buildforkernels akmod
%global debug_package %{nil}
%undefine _auto_set_build_flags
Name: ite8291r3-kmod
Version: 0.0.20261113.4
Release: 1%{?dist}
Summary: ITE 8291 multicolor keyboard LED driver
License: GPL-2.0-or-later
URL: https://github.com/tuxedocomputers/tuxedo-drivers
Source0: ite_8291.tar.gz
BuildRequires: gcc, kmodtool, elfutils-libelf-devel
%{expand:%(kmodtool --target %{_target_cpu} --repo local --kmodname %{name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null)}
%description
Exposes the ITE 8291 keyboard lighting through the Linux LED class,
allowing UPower and KDE Powerdevil to control keyboard brightness.
%package -n ite8291r3-kmod-common
Summary: Native keyboard LED integration for ITE 8291
BuildArch: noarch
%description -n ite8291r3-kmod-common
Native KDE brightness, coalesced per-key frames and R9T RGB passthrough.
%prep
%{?kmodtool_check}
%setup -q -c -n ite_8291
for kernel_version in %{?kernel_versions}; do
    mkdir _kmod_build_${kernel_version%%___*}
    cp Kbuild ite_8291.c color_contract.h color_model.h _kmod_build_${kernel_version%%___*}/
done
%build
for kernel_version in %{?kernel_versions}; do
    %make_build -C "${kernel_version##*___}" M="$PWD/_kmod_build_${kernel_version%%___*}" modules
done
%install
for kernel_version in %{?kernel_versions}; do
    install -D -m 0644 _kmod_build_${kernel_version%%___*}/ite_8291.ko \
        $RPM_BUILD_ROOT/%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/ite_8291.ko
done
install -d $RPM_BUILD_ROOT/etc/modules-load.d
printf 'ite_8291\n' > $RPM_BUILD_ROOT/etc/modules-load.d/ite8291r3.conf
%{?akmod_install}
%files -n ite8291r3-kmod-common
%license COPYING
%config(noreplace) /etc/modules-load.d/ite8291r3.conf
%changelog
* Thu Nov 12 2026 Local system administrator - 0.0.20260731-1.local
- Use TUXEDO ITE 8291 module revision 76b226bb for native multicolor LEDs.
