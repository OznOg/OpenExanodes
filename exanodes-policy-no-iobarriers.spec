#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


# Disable auto-generation of the debug package

%define debug_package %{nil}
%define __os_install_post /bin/true


# Define some extra locations

%define _pkgdatadir %{_datadir}/exanodes


%define	name		exanodes-policy-no-io-barriers
%define version         3.1
# Must be empty for development versions, and set to rcX for -rc versions
%define extraversion
%{?extraversion: %define release 0.%extraversion}
%{?!extraversion: %define release 1}

Summary: 		Exanodes Policy that disables IO barriers
Name: 			%name
Version: 		%version
Vendor:			Seanodes
Packager:		Seanodes

Release: 		%{release}
License: 		PROPRIETARY
Group: 			System
URL: 			http://www.seanodes.com
BuildArch:              noarch
BuildRoot: 		%{_tmppath}/%{name}-%{version}-%{release}-buildroot

Provides:               exanodes-policy = %{version}-%{release}

%description
Exanodes Policy that disables IO barriers.

%install
mkdir -p $RPM_BUILD_ROOT/%{_pkgdatadir}
cat > $RPM_BUILD_ROOT/%{_pkgdatadir}/default_tunables.conf << EOF
<Exanodes release="3.0">
  <tunables>
    <tunable name="io_barriers" default_value="FALSE"/>
  </tunables>
</Exanodes>
EOF

%files
%defattr(644,root,root,-)
%{_pkgdatadir}/default_tunables.conf

