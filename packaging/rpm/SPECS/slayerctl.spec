Name:           slayerctl
Version:        0.1.1
Release:        6%{?dist}
Summary:        Game Garaj Slayer hardware control backend
License:        MIT AND LGPL-2.1-or-later
URL:            https://github.com/MCC45TR/gg-slayer
Source0:        slayerctl-%{version}.tar.xz
Packager:       MCC45TR <mcc45tr@gmail.com>
BuildRequires:  gcc-c++, cmake, make, ninja-build, python3-devel
BuildRequires:  openssl-devel
BuildRequires:  qt6-qtbase-devel, qt6-qtdeclarative-devel, extra-cmake-modules
BuildRequires:  kf6-kcmutils-devel, pkgconfig(xkbcommon), pkgconfig(wayland-client)
BuildRequires:  pkgconfig(libsystemd), pkgconfig(x11), pkgconfig(xext), wayland-devel
BuildRequires:  systemd-rpm-macros, desktop-file-utils, breeze-icon-theme
BuildRequires:  kf6-kirigami, kf6-qqc2-desktop-style, qt6-qtimageformats
BuildRequires:  desktop-backgrounds-kde, xkeyboard-config
Requires:       polkit, systemd, systemd-udev, /usr/bin/systemd-creds
Recommends:     slayerctl-r9t-kmod, ite8291r3-kmod
%{?systemd_requires}
%global debug_package %{nil}
%description
Validated device control, D-Bus broker and desktop bridges for Slayer R9T.
Settings and color profiles are TPM encrypted. Frontends communicate with
this backend; the Qt application is optional. Developer: MCC45TR.

%package -n slayer-control-qt
Summary:        Native KDE Qt frontend for slayerctl
Requires:       slayerctl%{?_isa} = %{version}-%{release}
Requires:       qt6-qtwayland, qt6-qtimageformats, kf6-kirigami, kf6-qqc2-desktop-style
Requires:       kf6-kcmutils, qt6-qtdeclarative
Suggests:       python3-pyside6
Provides:       slayerctl-gui = %{version}-%{release}
Provides:       slayerctl-kde = %{version}-%{release}
Obsoletes:      slayerctl-gui < 0.1.0
Obsoletes:      slayerctl-kde < 0.1.0
%description -n slayer-control-qt
Kirigami control interface and KDE System Settings module. Hardware logic
and desktop integration belong to slayerctl. Developer: MCC45TR.

%package -n slayer-face-auth
Summary:        Validated IR enrollment and test frontend
BuildArch:      noarch
Requires:       slayerctl = %{version}-%{release}, python3
Suggests:       howdy
Provides:       slayerctl-face = %{version}-%{release}
Obsoletes:      slayerctl-face < 0.1.0
%description -n slayer-face-auth
Explicit Howdy IR enrollment and test commands. Installation does not enroll
biometric data or modify PAM. Developer: MCC45TR <mcc45tr@gmail.com>.

%package -n slayer-control-gtk
Summary:        GNOME frontend placeholder for slayerctl
BuildArch:      noarch
Requires:       slayerctl = %{version}-%{release}, python3-gobject, gtk4
Provides:       slayerctl-gnome = %{version}-%{release}
Obsoletes:      slayerctl-gnome < 0.1.0
%description -n slayer-control-gtk
Placeholder window for the future GNOME frontend. Device control is provided
by slayerctl. Developer: MCC45TR <mcc45tr@gmail.com>.

%prep
%autosetup

%build
%cmake -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DSLAYER_BUILD_GUI=ON -DSLAYER_BUILD_KCM=ON
%cmake_build

%install
%cmake_install
install -D -m0755 packaging/slayer-face-auth %{buildroot}%{_bindir}/slayer-face-auth
install -D -m0755 packaging/slayer-face-compat %{buildroot}%{_bindir}/slayer-face
install -D -m0755 gtk/slayer-control-gtk %{buildroot}%{_bindir}/slayer-control-gtk
install -D -m0644 packaging/slayer-control-gtk.desktop %{buildroot}%{_datadir}/applications/slayer-control-gtk.desktop
install -D -m0644 packaging/slayerctl.preset %{buildroot}%{_presetdir}/90-slayerctl.preset
install -d %{buildroot}%{_sysconfdir}/slayerctl/fan-curves %{buildroot}%{_sysconfdir}/yum.repos.d
%if 0%{?fedora} == 45
sed 's/@CHROOT@/fedora-45/g' packaging/gg-slayer.repo.in > %{buildroot}%{_sysconfdir}/yum.repos.d/_copr:copr.fedorainfracloud.org:mcc45tr:gg-slayer.repo
%else
sed 's/@CHROOT@/fedora-rawhide/g' packaging/gg-slayer.repo.in > %{buildroot}%{_sysconfdir}/yum.repos.d/_copr:copr.fedorainfracloud.org:mcc45tr:gg-slayer.repo
%endif

install -D -m0644 packaging/slayer-control-qt.metainfo.xml %{buildroot}%{_datadir}/metainfo/io.github.MCC45TR.slayer_control_qt.metainfo.xml
install -D -m0644 packaging/slayer-control-gtk.metainfo.xml %{buildroot}%{_datadir}/metainfo/io.github.MCC45TR.slayer_control_gtk.metainfo.xml

%check
QT_QPA_PLATFORM=offscreen %ctest --timeout 120
desktop-file-validate %{buildroot}%{_datadir}/applications/slayer-control.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/slayer-control-gtk.desktop
%{__python3} packaging/check-payload.py %{buildroot} %{__cmake_builddir}
%{__python3} -m py_compile packaging/slayer-face-auth gtk/slayer-control-gtk tools/*.py

%post
%systemd_post slayerctl.service slayerctl-resume.service
%udev_rules_update
%preun
%systemd_preun slayerctl.service slayerctl-resume.service slayerctl-restore.service slayerctl-power-event.service
%postun
%systemd_postun slayerctl.service slayerctl-resume.service
%udev_rules_update
%posttrans
/usr/bin/systemctl try-restart slayerctl-control.service >/dev/null 2>&1 || :
%{_libexecdir}/slayerctl-desktop --restart-session >/dev/null 2>&1 || :

%files
%license LICENSE tools/COPYING.LGPL-2.1
%doc README.md AUTHORS docs/KIRIGAMI-IMPLEMENTATION.md docs/COPR.md docs/IDLE-DIMMING.md docs/KDE-FACE.md
%{_bindir}/slayerctl
%{_libexecdir}/slayerctl-helper
%{_libexecdir}/slayerctl-module-sign
%{_libexecdir}/slayerctl-kmod-activate
%{_libexecdir}/slayerctl-desktop
%{_libexecdir}/slayerctl-service
%{_libexecdir}/slayerctl-session
%{_userunitdir}/slayerctl-session.service
%{_datadir}/dbus-1/services/org.gamegaraj.Slayer.Session1.service
%{_sysconfdir}/xdg/autostart/slayerctl-session.desktop
%{_datadir}/dbus-1/system.d/org.gamegaraj.Slayer1.conf
%{_datadir}/dbus-1/system-services/org.gamegaraj.Slayer1.service
%{_datadir}/polkit-1/rules.d/49-slayerctl-admin.rules
%{_datadir}/polkit-1/actions/org.gamegaraj.slayerctl.policy
%{_unitdir}/slayerctl*.service
%{_udevrulesdir}/85-slayerctl-power.rules
%{_presetdir}/90-slayerctl.preset
%dir %{_sysconfdir}/slayerctl
%dir %{_sysconfdir}/slayerctl/rgb-profiles
%dir %{_sysconfdir}/slayerctl/fan-curves
%attr(0600,root,root) %config(noreplace) %{_sysconfdir}/slayerctl/slayerctl.conf
%config(noreplace) %{_sysconfdir}/slayerctl/rgb-profiles/*.conf
%config(noreplace) %{_sysconfdir}/yum.repos.d/_copr:copr.fedorainfracloud.org:mcc45tr:gg-slayer.repo

%files -n slayer-control-qt
%{_bindir}/slayer-control-qt
%{_datadir}/metainfo/io.github.MCC45TR.slayer_control_qt.metainfo.xml
%{_bindir}/slayer-control
%{_datadir}/applications/slayer-control.desktop
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_slayerctl.so
%{_datadir}/applications/kcm_slayerctl.desktop
%dir %{_datadir}/slayerctl
%dir %{_datadir}/slayerctl/tools
%{_datadir}/slayerctl/tools/*.py

%files -n slayer-face-auth
%{_bindir}/slayer-face-auth
%{_bindir}/slayer-face

%files -n slayer-control-gtk
%{_bindir}/slayer-control-gtk
%{_datadir}/metainfo/io.github.MCC45TR.slayer_control_gtk.metainfo.xml
%{_datadir}/applications/slayer-control-gtk.desktop

%changelog
* Sat Nov 14 2026 MCC45TR <mcc45tr@gmail.com> - 0.1.1-1
- Independent ctl desktop integration and quiet five-second keyboard idle fades.
- COPR distribution, native Qt frontend, face utility and GNOME placeholder.
