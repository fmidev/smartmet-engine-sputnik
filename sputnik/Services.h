#pragma once

#include "BackendForwarder.h"
#include "BackendSentinel.h"
#include "BackendServer.h"
#include "BackendService.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <spine/Reactor.h>
#include <spine/Thread.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;
}

using BackendServicePtr = boost::shared_ptr<BackendService>;
using BackendServerPtr = boost::shared_ptr<BackendServer>;

class Services
{
 private:
  mutable SmartMet::Spine::MutexType itsMutex;
  Spine::Reactor* itsReactor = nullptr;

 public:
  typedef std::vector<BackendServicePtr> BackendServiceList;
  typedef boost::shared_ptr<BackendServiceList> BackendServiceListPtr;
  typedef std::map<std::string, std::pair<BackendServiceListPtr, BackendForwarderPtr> >
      ServiceURIMap;

  typedef std::map<std::string, boost::shared_ptr<BackendSentinel> > SentinelMap;

  enum ForwardingMode
  {
    Random,
    InverseLoad,
    InverseConnections
  };

  typedef std::list<boost::tuple<std::string, std::string, int> > BackendList;

  // Map contain lists of BackendService lists associated with URI
  ServiceURIMap itsServicesByURI;

  mutable SmartMet::Spine::MutexType itsSentinelMutex;  // This guards the sentinel map

  SentinelMap itsSentinels;

  ForwardingMode itsFwdMode;

  float itsBalancingCoefficient;

  // Service accessing methods
  BackendServicePtr getService(const std::string& theURI);

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

  // Constructors
  Services() = default;

  // Destructor
  ~Services() {}
  void status(std::ostream& out) const;
  boost::shared_ptr<SmartMet::Spine::Table> backends(const std::string& service = "") const;
  BackendList getBackendList(const std::string& service = "") const;

  void setReactor(Spine::Reactor& theReactor);
};

}  // namespace SmartMet
