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
  const std::string& URI() const { return itsURI; }
  int LastUpdate() const { return itsLastUpdate; }
  bool AllowCache() const { return itsAllowCache; }
  int SequenceNumber() const { return itsSequenceNumber; }
  bool DefinesPrefix() const { return definesPrefix; }
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
      : itsBackendServer(std::move(theBackendServer)),
        itsURI(std::move(theURI)),
        itsLastUpdate(theLastUpdate),
        itsAllowCache(theAllowCache),
        itsSequenceNumber(theSequenceNumber),
        definesPrefix(definesPrefix)
  {
  }

  ~BackendService() = default;

  BackendService() = delete;
  BackendService(const BackendService& other) = delete;
  BackendService& operator=(const BackendService& other) = delete;
  BackendService(BackendService&& other) = delete;
  BackendService& operator=(BackendService&& other) = delete;
};

}  // namespace SmartMet
