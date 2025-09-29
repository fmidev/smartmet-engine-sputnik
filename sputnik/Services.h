#pragma once

#include "BackendForwarder.h"
#include "BackendSentinel.h"
#include "BackendServer.h"
#include "BackendService.h"
#include "URIPrefixMap.h"
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <spine/Reactor.h>
#include <spine/Thread.h>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;
}

using BackendServicePtr = std::shared_ptr<BackendService>;
using BackendServerPtr = std::shared_ptr<BackendServer>;

class Services
{
 private:
  mutable SmartMet::Spine::MutexType itsMutex;
  Spine::Reactor* itsReactor = nullptr;

 public:
  using BackendServiceList = std::vector<BackendServicePtr>;
  using BackendServiceListPtr = std::shared_ptr<BackendServiceList>;
  using SentinelMap = std::map<std::string, std::shared_ptr<BackendSentinel>>;
  using ServiceURIMap =
      std::map<std::string, std::pair<BackendServiceListPtr, BackendForwarderPtr>>;

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

  // Map contain lists of BackendService lists associated with URI
  ServiceURIMap itsServicesByURI;

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

  bool removeBackend(const std::string& theHostname, int thePort, const std::string& theURI = "");

  bool latestSequence(int itsSequenceNumber);

  void setForwarding(const std::string& theMode, float balancingCoefficient);

  bool queryBackendAlive(const std::string& theHostName, int thePort);

  void signalBackendConnection(const std::string& theHostName, int thePort);

  void setBackendAlive(const std::string& theHostName, int thePort);

  ~Services() = default;
  Services() = default;

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

  void setReactor(Spine::Reactor& theReactor);
};

}  // namespace SmartMet
