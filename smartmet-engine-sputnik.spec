%define DIRNAME sputnik
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-engine-%{DIRNAME}
Summary: SmartMet Sputnik cluster management engine
Name: %{SPECNAME}
Version: 25.9.29
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Engines
URL: https://github.com/fmidev/smartmet-engine-sputnik
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# https://fedoraproject.org/wiki/Changes/Broken_RPATH_will_fail_rpmbuild
%global __brp_check_rpaths %{nil}

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: smartmet-library-spine-devel >= 25.9.16
BuildRequires: protobuf-compiler
BuildRequires: protobuf-devel
BuildRequires: smartmet-library-macgyver-devel >= 25.9.19
Requires: protobuf
Requires: smartmet-server >= 25.9.9
Requires: smartmet-library-spine >= 25.9.16
Requires: smartmet-library-macgyver >= 25.9.19
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
Obsoletes: smartmet-brainstorm-sputnik < 16.11.1
Obsoletes: smartmet-brainstorm-sputnik-debuginfo < 16.11.1

%description
SmartMet Sputnik cluster management engine


%package -n %{SPECNAME}-devel
Summary: SmartMet %{SPECNAME} development headers
Group: SmartMet/Development
Provides: %{SPECNAME}-devel
Obsoletes: smartmet-brainstorm-sputnik-devel < 16.11.1
%description -n %{SPECNAME}-devel
SmartMet %{SPECNAME} development headers.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}

%build -q -n %{SPECNAME}
make protoc
make %{_smp_mflags}

%install
%makeinstall

%postun

%clean
rm -rf $RPM_BUILD_ROOT

%files -n %{SPECNAME}
%defattr(0775,root,root,0775)
%{_datadir}/smartmet/engines/%{DIRNAME}.so

%files -n %{SPECNAME}-devel
%defattr(0664,root,root,0775)
%{_includedir}/smartmet/engines/%{DIRNAME}

%changelog
* Mon Sep 29 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.9.29-1.fmi
- Fixed compiler warnings

* Tue Feb 18 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.2.18-1.fmi
- Update to gdal-3.10, geos-3.13 and proj-9.5

* Fri Jan 17 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.1.17-1.fmi
- Additional parameter for optional removing part of information from responses of Engine::backends and Engine::status

* Thu Jan 16 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.1.16-1.fmi
- Fix title of /admin?what=backends response

* Wed Nov 13 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.13-1.fmi
- Move handling admin requests to plugins (frontend and backend)

* Fri Nov  8 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.8-1.fmi
- Register admin requests to SmartMet::Spine::Reactor

* Wed Sep 18 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.9.18-1.fmi
- Print to journal if Sputnik is paused during startup

* Tue Sep 17 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.9.17-1.fmi
- Added optional pause configuration setting with default value false.

* Wed Aug  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.8.7-1.fmi
- Update to gdal-3.8, geos-3.12, proj-94 and fmt-11

* Wed Jul 24 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.24-1.fmi
- Use std::array instead of boost::array

* Fri Jul 19 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.19-1.fmi
- Leave logging in URIPrefixMap for debugging only (MYDEBUG defined)

* Fri Jul 12 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.12-1.fmi
- Replace many boost library types with C++ standard library ones

* Thu Jun  6 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.6.6-1.fmi
- Added ExponentialConnectionsForwarder

* Thu May 16 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.16-1.fmi
- Clean up boost date-time uses

* Tue May  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.7-1.fmi
- Use Date library (https://github.com/HowardHinnant/date) instead of boost date_time

* Fri Feb 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.23-1.fmi
- Full repackaging

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Mon Jul 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.10-1.fmi
- Silenced compiler warnings

* Mon Mar  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.6-1.fmi
- Silenced CodeChecker warnings

* Fri Nov 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.25-1.fmi
- Simplified RandomForwarder code
- Added DoubleRandomForwarder

* Tue Nov 15 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.15-1.fmi
- Fixed removeBackend not to remove all hosts which lead to a load inbalance

* Tue Nov  8 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.11.8-1.fmi
- Update support of URI prefixes. Use std::shared_ptr

* Wed Oct  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.5-2.fmi
- Do not use boost::noncopyable

* Wed Oct  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.5-1.fmi
- Add client IP to error reports on strange URI requests

* Tue Sep 20 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.9.20-1.fmi
- Fix URI prefix support

* Thu Sep 15 2022 Andris Pavenis <andris.pavenis@fmi.fi> 22.9.15-1.fmi
- Fix crash in Services::removeBackend

* Mon Sep 12 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.9.12-1.fmi
- Remove unnecessary debugging output

* Wed Sep  7 2022 Andris Pavenis <andris.pavenis@fmi.fi> 22.9.7-1.fmi
- Support URI prefixes like (/edr/...) for both backend and frontend side

* Fri Jun 17 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.17-1.fmi
- Add support for RHEL9. Update libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Tue May 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.24-1.fmi
- Repackaged due to NFmiArea ABI changes

* Mon Sep 27 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.27-1.fmi
- Repackage due to dependency change (libgonfig++)

* Tue Aug 31 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.31-1.fmi
- Repackaged due to Spine ABI changes

* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.fmi
- Use the new shutdown API

* Wed Jul 28 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.7.28-1.fmi
- Silenced compiler warnings

* Mon Apr 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.19-1.fmi
- Added "leastconnections" load balancing

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Mon Dec  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.7-1.fmi
- Minor fixes to silence CodeChecker warnings

* Tue Oct 20 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.20-1.fmi
- Rebuild due to libconfig upgrade to version 1.7.2

* Mon Oct 12 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.12-1.fmi
- Prefer lambdas over boost::bind

* Wed Sep 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.23-1.fmi
- Use Fmi::Exception instead of Spine::Exception

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Mon Aug 10 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.10-1.fmi
- Sputnik will no longer respond to frontends if the load is high

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgraded to Boost 1.69

* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.fmi
- Added support for ASAN & TSAN builds

* Tue Dec  4 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.4-1.fmi
- Repackaged since Spine::Table size changed

* Fri Nov  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.9-1.fmi
- Use SIGKILL instead of exit for restart to prevent expensive core dumps

* Thu Nov  8 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.8-1.fmi
- Added possibility to pause/continue sputnik with optional deadlines

* Mon Nov  5 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.5-1.fmi
- Added load balancing based on the number of active connections

* Fri Oct 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.26-2.fmi
- Check the shutdown flag more often

* Fri Oct 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.26-1.fmi
- heartbeat.interval default value is 5 seconds, was previously hardcoded
- heartbeat.timeout default value is 2 seconds, was previously hardcoded
- heartbeat.max_skipped_cycles value is 2, was previously hardcoded

* Thu Oct 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.25-1.fmi
- Fixed forwarding, throttle_limit and balance_factor settings
- Limit load in balancing to >= 1.0 to avoid peaks from very low loads (0.01)

* Sun Aug 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.26-1.fmi
- Use Fmi::to_string instead of boost::lexical_cast
- Silenced CodeChecker warnings

* Wed Jul 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.25-1.fmi
- Prefer nullptr over NULL

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Upgrade to boost 1.66

* Tue Mar 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.3.20-1.fmi
- Full repackaging of the server

* Fri Feb  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.9-1.fmi
- Repackaged since base class SmartMetEngine size changed

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-1.fmi
- Recompiled since Spine::Exception changed

* Wed Jan  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.4-1.fmi
- Changed to use renamed SmartMet base libraries

* Wed Nov 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.30-1.fmi
- No installation for configuration

* Tue Nov  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.1-1.fmi
- Namespace changed. Services.h and BackendForwarer.h refactored

* Tue Sep 13 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- Fixed UDP listener to work after merge failure

* Tue Sep  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- New exception handler

* Tue Aug 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.23-1.fmi
- Less verbose output

* Mon Aug 15 2016 Markku Koskela <markku.koskela@fmi.fi> - 16.8.15-1.fmi
- The Sputnic engine was modified so that in addition to the broadcast
- address it allows to define a list of IP-addresses and ports where
- the service discovery messages are sent. This makes possible to run
- several frontend and backend installations in the same computer, which
- makes the testing much more easier. Notice that also some of 
- the configuration parameters were renamed.

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Full recompile

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-1.fmi
- Full recompile

* Wed Jun  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.1-1.fmi
- Added graceful shutdown

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-1.fmi
- newbase API changed, full recompile

* Thu Nov  5 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.5-1.fmi
- Backend throttle limit is now 0 by default (was 50)

* Mon Oct 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.26-1.fmi
- Added proper debuginfo packaging

* Tue Aug 18 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.18-1.fmi
- Recompile forced by HTTP API changes

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-1.fmi
- Use -fno-omit-frame-pointer to improve perf use

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Wed Apr  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.8-1.fmi
- Dynamic linking of smartmet libraries into use

* Wed Dec 17 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.17-1.fmi
- Recompiled due to spine API changes

* Thu Nov 13 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.13-1.fmi
- Recompiled due to newbase API changes

* Mon Jun 30 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.30-1.fmi
- Recompiled due to spine API changes

* Fri May 16 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.16-1.fmi
- Better error reporting when parsing configuration

* Wed May 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.14-1.fmi
- Use shared macgyver and locus libraries

* Mon Apr 28 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.4.28-1.fmi
- Full recompile due to large changes in spine etc APIs

* Fri Apr 25 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.4.25-2.fmi
- Added mock configuration file

* Tue Nov  5 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.5-1.fmi
- Major release

* Tue Oct 15 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.10.15-1.fmi
- Frontend mode now persists old service information if no backend responses are received

* Wed Oct  9 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.10.9-1.fmi
- Now conforming with the new Reactor initialization API

* Wed Sep 18 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.9.18-1.fmi
- Recompiled against the new Spine

* Fri Sep 6  2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.9.6-1.fmi
- Recompiled due Spine changes

* Mon Aug 19 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.8.19-1.fmi
- Improvements to throttling: 
- Information when backend is resurrected)
- Throttle value updated at every cycle

* Thu Aug 15 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.8.15-1.fmi
- Added sentinel for a posteriori backend health checking

* Mon Aug 12 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.8.12-1.fmi
- Added sending of the connection throttle limit

* Tue Jul 23 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.7.23-1.fmi
- Recompiled due to thread safety fixes in newbase & macgyver

* Thu Jul 11 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.7.11-1.fmi
- Fixed backend load calculation
- Added balancing factor as configurable parameter

* Tue Jul  9 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.7.9-1.fmi
- Sputnik can now load balance backend connections in frontend mode

* Wed Jul  3 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54

* Mon Jun 17 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.6.17-1.fmi
- Sputnik now sets reuse_address option to to the socket, it may help when restarting the server

* Mon May 27 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.5.27-1.fmi
- Socket errors are now fatal (Sputnik considered a core service)
- Frontend now binds to an arbitrary local endpoint

* Wed May 22 2013 tervo    <roope.tervo@fmi.fi>    - 13.5.22-1.fmi
- Better error reporting
- Rewrote networking portion with Boost ASIO, Sputnik is now fully asynchronous

* Thu May 16 2013 lauri    <tuomo.lauri@fmi.fi>   - 14.5.16-1.fmi
- Built against new spine

* Mon Apr 22 2013 mheiskan <mika.heiskanen@fi.fi> - 13.4.22-1.fmi
- Brainstorm API changed

* Fri Apr 12 2013 lauri <tuomo.lauri@fmi.fi>    - 13.4.12-2.fmi
- Removed spurious error report

* Fri Apr 12 2013 lauri <tuomo.lauri@fmi.fi>    - 13.4.12-1.fmi
- Built against the new Spine

* Tue Apr  9 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.9-1.fmi
- Added strerror messages to all possible locations in Sputnik

* Wed Feb  6 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.2.6-1.fmi
- Built against new Spine and Server

* Tue Dec 18 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.12.18-1.fmi
- Reverted guard, did not work properly

* Wed Nov 21 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.11.21-1.fmi
- Added guard against unknown broadcast messages

* Wed Nov  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.7-1.el6.fmi
- Upgrade to boost 1.52
- Refactored spine library into use

* Wed Aug 29 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.8.29-1.el6.fmi
- Added function to return an actual list of backends

* Wed Aug 15 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.8.15-1.el6.fmi
- SmartMet ContentEngine API  change

* Thu Aug  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.8.9-3.el6.fmi
- Disabled automatic sputnik.conf generation since ethernet card configurations are not consistent

* Thu Aug  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.8.9-2.el6.fmi
- Fixed backend listing to produce multiple row of output

* Thu Aug  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.8.9-1.el6.fmi
- Added listing of backends

* Tue Aug  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.8.7-1.el6.fmi
- Added wall clock times to cout messages

* Thu Jul 26 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.26-1.el6.fmi
- Added URI() method

* Wed Jul 25 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.25-2.el6.fmi
- Fixed cluster info HTML for frontends

* Wed Jul 25 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.25-1.el6.fmi
- Output cluster info inside HTML tags

* Mon Jul  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.9-2.el6.fmi
- Fixed clusterinfo requests

* Mon Jul  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.9-1.el6.fmi
- Improved error handling to prevent segmentation faults from null pointer dereferences

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-2.el6.fmi
- Improved API for determining the mode

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi
- Upgrade to boost 1.50

* Wed Apr  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.4-1.el6.fmi
- common lib changed

* Mon Apr  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.2-1.el6.fmi
- macgyver change forced recompile

* Sat Mar 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.31-1.el5.fmi
- Upgrade to boost 1.49

* Fri Jan 20 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.1.20-1.el5.fmi
- Sputnik will now throw if there are no services left once a backend is removed

* Wed Dec 21 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.21-1.el6.fmi
- RHEL6 release

* Tue Aug 16 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.8.16-1.el5.fmi
- Upgraded to boost 1.47

* Mon Mar 28 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.28-1.el5.fmi
- Fixed protobuf dependencies

* Fri Mar 25 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.25-1.el5.fmi
- Fixed to use boost::this_thread::sleep

* Thu Mar 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.24-1.el5.fmi
- Upgrade to boost 1.46

* Thu Oct 28 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.28-1.el5.fmi
- Recompiled due to external API changes

* Tue Sep 14 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.9.14-1.el5.fmi
- Upgrade to boost 1.44

* Mon May 10 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.5.11-1.el5.fmi
- Upgrade to RHEL 5.5

* Tue Mar 23 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.23-1.el5.fmi
- Fixed config file generation

* Sat Mar 20 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.20-1.el5.fmi
- Added config file generation

* Fri Jan 15 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.15-1.el5.fmi
- Upgrade to boost 1.41

* Mon Sep  7 2009 westerba <antti.westerberg@fmi.fi> - 9.9.7-1.el5.fmi
- Fixed the mess relating to compilations against to different protobuf versions

* Thu Jul 16 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.16-1.el5.fmi
- Bugfixes

* Tue Jul 14 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.14-2.el5.fmi
- Upgrade to boost 1.39

* Tue Jul 14 2009 westerba <antti.westerberg@fmi.fi> - 9.7.14-1.el5.fmi
- Completely revamped the cluster synchronization mechanism using SeqNo's.

* Thu Jul  2 2009 westerba <antti.westerberg@fmi.fi> - 9.7.2-1.el5.fmi
- Method to delete specific servers from the list. Ping round now 15 secs.

* Mon May 25 2009 westerba <antti.westerberg@fmi.fi> - 9.5.25-1.el5.fmi
- Possibility to access particular host (URI /host/service)

* Wed Nov 19 2008 westerba <antti.westerberg@fmi.fi> - 8.11.19-1.el5.fmi
- Compiled against new SmartMet API

* Thu Oct  9 2008 westerba <antti.westerberg@fmi.fi> - 8.10.9-1.el5.fmi
- Packaged operational and development files into separate packages

* Mon Oct 6 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.10.6-1.el5.fmi
- Initial release

* Mon Sep 1 2008 westerba <antti.westerberg@fmi.fi> - 8.9.1-1.el5.fmi
- Initial build
