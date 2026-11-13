%global buildforkernels akmod
%global debug_package %{nil}
%undefine _auto_set_build_flags
Name: slayerctl-r9t-kmod
Version: 0.1.0
Release: 1%{?dist}
Summary: Exact-model Slayer R9T controls and hwmon bridge
License: GPL-2.0-only
URL: https://github.com/MCC45TR/gg-slayer
Source0: slayerctl_r9t.tar.gz
BuildRequires: gcc, kmodtool, elfutils-libelf-devel
%{expand:%(kmodtool --target %{_target_cpu} --repo local --kmodname %{name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null)}
%description
Strict model/BIOS/board/project-gated charge, Fn, factory fan/thermal controls,
fan RPM and fixed-field next-boot MUX. No arbitrary EC/UEFI, TDP or OC writes.
%package -n slayerctl-r9t-kmod-common
Summary: Slayer R9T module loading
BuildArch: noarch
%description -n slayerctl-r9t-kmod-common
Load the exact-model bridge. Loading does not change the charge profile.
%prep
%{?kmodtool_check}
%setup -q -c -n slayerctl_r9t
for kernel_version in %{?kernel_versions}; do
    mkdir _kmod_build_${kernel_version%%___*}
    cp Kbuild slayerctl_r9t.c profile_bits.h model_contract.h firmware_contract.h mux_transaction.h thermal_profile.h _kmod_build_${kernel_version%%___*}/
done
%build
for kernel_version in %{?kernel_versions}; do
    %make_build -j2 -C "${kernel_version##*___}" M="$PWD/_kmod_build_${kernel_version%%___*}" modules
done
%install
for kernel_version in %{?kernel_versions}; do
    install -D -m 0644 _kmod_build_${kernel_version%%___*}/slayerctl_r9t.ko \
        $RPM_BUILD_ROOT/%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/slayerctl_r9t.ko
done
install -d $RPM_BUILD_ROOT/etc/modules-load.d
printf 'slayerctl_r9t\n' > $RPM_BUILD_ROOT/etc/modules-load.d/slayerctl-r9t.conf
%{?akmod_install}
%files -n slayerctl-r9t-kmod-common
%license COPYING
%config(noreplace) /etc/modules-load.d/slayerctl-r9t.conf
%changelog
* Fri Nov 13 2026 Local system administrator - 0.0.6-1.local
- Lock complete EFI transactions and verify rollback after failed stores.
* Fri Nov 13 2026 Local system administrator - 0.0.5-1.local
- Add Fn, factory thermal/fan controls, RPM and native fixed-field MUX.
* Fri Nov 13 2026 Local system administrator - 0.0.4-1.local
- Add model-gated battery profile and read-only temperature interfaces.
