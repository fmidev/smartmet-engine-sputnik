#pragma once

#include "BackendSentinel.h"
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
class BackendServer
{
 private:
  // Private data members
  std::string itsName;
  std::string itsIP;
  int itsPort;
  std::string itsComment;
  float itsLoad;
  unsigned int itsThrottle;

 public:
  // Methods to read Service entry parameters
  std::string& Name() { return itsName; }
  std::string& IP() { return itsIP; }
  std::string& Comment() { return itsComment; }
  int Port() const { return itsPort; }
  float Load() const { return itsLoad; }
  unsigned int Throttle() const { return itsThrottle; }

  ~BackendServer() = default;

  BackendServer(std::string theName,
                std::string theIP,
                int thePort,
                std::string theComment,
                float theLoad,
                unsigned int theThrottle)
      : itsName(std::move(theName)),
        itsIP(std::move(theIP)),
        itsPort(thePort),
        itsComment(std::move(theComment)),
        itsLoad(theLoad),
        itsThrottle(theThrottle)

  {
  }

  BackendServer() = delete;
  BackendServer(const BackendServer& other) = delete;
  BackendServer& operator=(const BackendServer& other) = delete;
  BackendServer(BackendServer&& other) = delete;
  BackendServer& operator=(BackendServer&& other) = delete;
};

}  // namespace SmartMet
