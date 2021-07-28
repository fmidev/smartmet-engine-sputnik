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
  int Port() { return itsPort; }
  float Load() { return itsLoad; }
  unsigned int Throttle() { return itsThrottle; }
  // Constructor
  BackendServer(std::string theName,
                std::string theIP,
                int thePort,
                std::string theComment,
                float theLoad,
                unsigned int theThrottle)
      : itsName(theName),
        itsIP(theIP),
        itsPort(thePort),
        itsComment(theComment),
        itsLoad(theLoad),
        itsThrottle(theThrottle)

  {
  }

  // Destructor
  ~BackendServer() = default;
};

}  // namespace SmartMet
