#pragma once

#include "BackendServer.h"
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
class BackendService
{
 private:
  // Private data members
  const boost::shared_ptr<BackendServer> itsBackendServer;
  std::string itsURI;
  int itsLastUpdate;
  bool itsAllowCache;
  int itsSequenceNumber;

 public:
  // Methods to read Service entry parameters
  boost::shared_ptr<BackendServer> Backend() { return itsBackendServer; }
  std::string& URI() { return itsURI; }
  int LastUpdate() { return itsLastUpdate; }
  bool AllowCache() { return itsAllowCache; }
  int SequenceNumber() { return itsSequenceNumber; }
  // Method to set some Service entry parameters
  void setLastUpdate(int theLastUpdate) { itsLastUpdate = theLastUpdate; }
  void setAllowCache(bool theAllowCache) { itsAllowCache = theAllowCache; }
  // Constructors
  BackendService(boost::shared_ptr<BackendServer> theBackendServer,
                 std::string theURI,
                 int theLastUpdate,
                 bool theAllowCache,
                 int theSequenceNumber)
      : itsBackendServer(theBackendServer),
        itsURI(theURI),
        itsLastUpdate(theLastUpdate),
        itsAllowCache(theAllowCache),
        itsSequenceNumber(theSequenceNumber)
  {
  }

  // Destructor
  ~BackendService() = default;
};

}  // namespace SmartMet
