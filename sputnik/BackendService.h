#pragma once

#include "BackendServer.h"
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <spine/Reactor.h>
#include <spine/Thread.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace SmartMet
{
class BackendService
{
 private:
  // Private data members
  const std::shared_ptr<BackendServer> itsBackendServer;
  std::string itsURI;
  int itsLastUpdate;
  bool itsAllowCache;
  int itsSequenceNumber;
  bool definesPrefix;

 public:
  // Methods to read Service entry parameters
  std::shared_ptr<BackendServer> Backend() { return itsBackendServer; }
  std::string& URI() { return itsURI; }
  int LastUpdate() { return itsLastUpdate; }
  bool AllowCache() { return itsAllowCache; }
  int SequenceNumber() { return itsSequenceNumber; }
  bool DefinesPrefix() { return definesPrefix; }
  // Method to set some Service entry parameters
  void setLastUpdate(int theLastUpdate) { itsLastUpdate = theLastUpdate; }
  void setAllowCache(bool theAllowCache) { itsAllowCache = theAllowCache; }
  // Constructors
  BackendService(std::shared_ptr<BackendServer> theBackendServer,
                 std::string theURI,
                 int theLastUpdate,
                 bool theAllowCache,
                 int theSequenceNumber,
                 int definesPrefix)
      : itsBackendServer(theBackendServer),
        itsURI(theURI),
        itsLastUpdate(theLastUpdate),
        itsAllowCache(theAllowCache),
        itsSequenceNumber(theSequenceNumber),
        definesPrefix(definesPrefix)
  {
  }

  // Destructor
  ~BackendService() = default;
};

}  // namespace SmartMet
