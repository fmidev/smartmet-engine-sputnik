#pragma once

#include "BackendServer.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

#include <spine/Reactor.h>
#include <spine/Thread.h>

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
  inline boost::shared_ptr<BackendServer> Backend() { return itsBackendServer; }
  inline std::string& URI() { return itsURI; }
  inline int LastUpdate() { return itsLastUpdate; }
  inline bool AllowCache() { return itsAllowCache; }
  inline int SequenceNumber() { return itsSequenceNumber; }
  // Method to set some Service entry parameters
  inline void setLastUpdate(int theLastUpdate) { itsLastUpdate = theLastUpdate; }
  inline void setAllowCache(bool theAllowCache) { itsAllowCache = theAllowCache; }
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
  ~BackendService() {}
};

}  // namespace SmartMet
