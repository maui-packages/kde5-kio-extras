# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.27
# 

Name:       kio-extras

# >> macros
# << macros

Summary:    Additional components to increase the functionality of KIO Framework
Version:    5.0.0
Release:    1
Group:      System/Base
License:    GPLv2+
URL:        http://www.kde.org
Source0:    %{name}-%{version}.tar.xz
Source100:  kio-extras.yaml
Source101:  kio-extras-rpmlintrc
Requires:   kf5-filesystem
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(Qt5PrintSupport)
BuildRequires:  pkgconfig(Qt5Svg)
BuildRequires:  kf5-rpm-macros
BuildRequires:  extra-cmake-modules
BuildRequires:  karchive-devel
BuildRequires:  kconfig-devel
BuildRequires:  kconfigwidgets-devel
BuildRequires:  kcoreaddons-devel
BuildRequires:  kdbusaddons-devel
BuildRequires:  kdoctools-devel
BuildRequires:  kdnssd-devel
BuildRequires:  kiconthemes-devel
BuildRequires:  ki18n-devel
BuildRequires:  kio-devel
BuildRequires:  khtml-devel
BuildRequires:  kdelibs4support-devel
BuildRequires:  solid-devel
BuildRequires:  phonon-qt5-devel
BuildRequires:  bzip2-devel
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  lzma-devel
BuildRequires:  shared-mime-info

%description
Additional components to increase the functionality of KIO Framework.


%prep
%setup -q -n %{name}-%{version}/upstream

# >> setup
# << setup

%build
# >> build pre
%kf5_make
# << build pre



# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
%kf5_make_install
# << install pre

# >> install post
# Remove libmolletnetwork.so - we don't have headers for it anyway and having
# a -devel package just because of this does not make sense
rm %{buildroot}%{_kf5_libdir}/libmolletnetwork.so
# << install post

%files
%defattr(-,root,root,-)
%{_kf5_bindir}/ktrash5
%{_kf5_libdir}/libmolletnetwork.so.*
%{_kf5_plugindir}/*.so
%{_kf5_sharedir}/kio_*
%{_kf5_sharedir}/konqsidebartng/*
%{_kf5_sharedir}/remoteview/*
%{_kf5_servicesdir}/*
%{_kf5_servicetypesdir}/*
%{_kf5_dbusinterfacesdir}/kf5_org.kde.network.kioslavenotifier.xml
%{_datadir}/mime/packages/kf5_network.xml
%{_kf5_htmldir}/en/kioslave5
%{_kf5_htmldir}/en/kcontrol
%{_datadir}/config.kcfg/jpegcreatorsettings.kcfg
# >> files
# << files
