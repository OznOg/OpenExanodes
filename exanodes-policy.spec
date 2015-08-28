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


%define	name		exanodes-policy
%define version         3.1
# Must be empty for development versions, and set to rcX for -rc versions
%define extraversion
%{?extraversion: %define release 0.%extraversion}
%{?!extraversion: %define release 1}

Summary: 		Exanodes Policy
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
Exanodes Policy.

%install
mkdir -p $RPM_BUILD_ROOT/%{_pkgdatadir}
cat > $RPM_BUILD_ROOT/%{_pkgdatadir}/default_tunables.conf << EOF
<Exanodes release="3.0">
  <tunables>
    <tunable name="gulm_masters" default_value=""/>
    <tunable name="sfs_lock_protocol" default_value="lock_gulm"/>
    <tunable name="max_requests" default_value="256"/>
    <tunable name="server_buffers" default_value="60"/>
    <tunable name="max_request_size" default_value="131072"/>
    <tunable name="max_client_requests" default_value="75"/>
    <tunable name="max_requests_per_disk" default_value="32"/>
    <tunable name="request_to_deactivate" default_value="2"/>
    <tunable name="tcp_max_active_disks" default_value="0"/>
    <tunable name="scheduler" default_value="fifo"/>
    <tunable name="tcp_client_buffer_size" default_value="131072"/>
    <tunable name="tcp_server_buffer_size" default_value="131072"/>
    <tunable name="sched_streams_number" default_value="128"/>
    <tunable name="sched_max_contiguous_reqs" default_value="1024"/>
    <tunable name="sched_anticipatory_trigger" default_value="50"/>
    <tunable name="sched_anticipatory_timeout" default_value="30"/>
    <tunable name="tcp_data_net_timeout" default_value="0"/>
    <tunable name="ib_data_net_timeout" default_value="20"/>
    <tunable name="io_barriers" default_value="TRUE"/>
    <tunable name="heartbeat_period" default_value="1"/>
    <tunable name="alive_timeout" default_value="5"/>
    <tunable name="default_slot_height" default_value="16"/>
    <tunable name="default_su_size" default_value="1024"/>
    <tunable name="default_dirty_zone_size" default_value="32768"/>
    <tunable name="default_readahead" default_value="8192"/>
    <tunable name="multicast_address" default_value="229.230.231.232"/>
    <tunable name="multicast_port" default_value="30798"/>
    <tunable name="rebuilding_slowdown" default_value="1"/>
    <tunable name="degraded_rebuilding_slowdown" default_value="0"/>
  </tunables>
</Exanodes>
EOF

%files
%defattr(644,root,root,-)
%{_pkgdatadir}/default_tunables.conf

