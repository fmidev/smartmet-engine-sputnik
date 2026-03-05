#pragma once

#include "BackendForwarder.h"
#include "BackendInfoRequest.h"
#include "BackendSentinel.h"
#include "BackendServer.h"
#include "BackendService.h"
#include "URIPrefixMap.h"
#include <macgyver/AtomicSharedPtr.h>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <spine/Reactor.h>
#include <spine/Thread.h>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;
}

using BackendServicePtr = std::shared_ptr<BackendService>;
using BackendServerPtr = std::shared_ptr<BackendServer>;
using BackendInfoRequestPtr = std::shared_ptr<BackendInfoRequest>;

class Services
{
 private:
  struct Snapshot
  {
    std::map<std::string,
             std::pair<std::shared_ptr<std::vector<BackendServicePtr>>, BackendForwarderPtr>>
        servicesByURI;
    std::map<std::string, std::shared_ptr<std::vector<BackendInfoRequestPtr>>> backendInfoRequests;
  };

  mutable SmartMet::Spine::MutexType itsMutex;
  Spine::Reactor* itsReactor = nullptr;

  Fmi::AtomicSharedPtr<Snapshot> itsSnapshot;
  std::atomic<std::uint64_t> itsSnapshotVersion{0};
  std::atomic<std::uint64_t> itsGetServiceCalls{0};
  std::atomic<std::uint64_t> itsGetServiceMisses{0};
  mutable std::atomic<std::uint64_t> itsLastMissWarningCallCount{0};

  void maybeLogHighMissRate() const;

  BackendForwarderPtr createForwarder(const std::vector<BackendServicePtr>& services,
                                      float defaultLoad) const;

 public:
  using BackendServiceList = std::vector<BackendServicePtr>;
  using BackendInfoRequestList = std::vector<BackendInfoRequestPtr>;
  using BackendServiceListPtr = std::shared_ptr<BackendServiceList>;
  using BackendInfoRequestListPtr = std::shared_ptr<BackendInfoRequestList>;
  using SentinelMap = std::map<std::string, std::shared_ptr<BackendSentinel>>;
  using ServiceURIMap =
      std::map<std::string, std::pair<BackendServiceListPtr, BackendForwarderPtr>>;
  using BackendInfoRequestMap =
      std::map<std::string, BackendInfoRequestListPtr>;

  enum ForwardingMode : std::uint8_t
  {
    Random,
    InverseLoad,
    InverseConnections,
    LeastConnections,
    DoubleRandom,
    ExponentialConnections
  };

  using BackendList = std::list<boost::tuple<std::string, std::string, int>>;

  // Map contain mapping of URI prefixes to backends
  URIPrefixMap itsPrefixMap;

  mutable SmartMet::Spine::MutexType itsSentinelMutex;  // This guards the sentinel map

  SentinelMap itsSentinels;

  ForwardingMode itsFwdMode = ForwardingMode::InverseConnections;

  float itsBalancingCoefficient = 2.0F;

  // Service accessing methods
  BackendServicePtr getService(const Spine::HTTP::Request& theRequest);

  // Service management methods
  bool addService(const BackendServicePtr& theBackendService,
                  const std::string& theFrontEndURI,
                  float theLoad,
                  unsigned int theThrottle);

  bool addBackendInfoRequest(const BackendInfoRequestPtr& theRequest);

  std::set<std::string> getInfoRequestNames() const;

  bool removeBackend(const std::string& theHostname, int thePort, const std::string& theURI = "");

  bool latestSequence(int itsSequenceNumber);

  void setForwarding(const std::string& theMode, float balancingCoefficient);

  bool queryBackendAlive(const std::string& theHostName, int thePort);

  void signalBackendConnection(const std::string& theHostName, int thePort);

  void setBackendAlive(const std::string& theHostName, int thePort);

  ~Services() = default;
  Services();

  Services(const Services& other) = delete;
  Services& operator=(const Services& other) = delete;
  Services(Services&& other) = delete;
  Services& operator=(Services&& other) = delete;

  /**
   * @brief Generate status report
   *
   * @param out Output stream to write the status report to
   * @param full If true, full information is listed. If false, only the service name is listed.
   */
  void status(std::ostream& out, bool full) const;

  /**
   * @brief List backends with given service or all services
   *
   * @param service Service name to list backends for. If empty, all services are listed.
   * @param full If true, full information is listed. If false, only the service name is listed.
   * @return Table containing the backend information
   */
  std::unique_ptr<SmartMet::Spine::Table> backends(const std::string& service = "",
                                                   bool full = true) const;

  BackendList getBackendList(const std::string& service = "") const;

  BackendList getInfoRequestBackendList(const std::string& infoRequestName) const;

  void setReactor(Spine::Reactor& theReactor);
};

}  // namespace SmartMet
