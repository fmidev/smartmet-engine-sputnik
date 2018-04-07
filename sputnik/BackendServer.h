#pragma once

#include "BackendSentinel.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

#include <spine/Reactor.h>
#include <spine/Thread.h>

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
  // Methos to read Service entry parameters
  inline std::string& Name() { return itsName; }
  inline std::string& IP() { return itsIP; }
  inline std::string& Comment() { return itsComment; }
  inline int Port() { return itsPort; }
  inline float Load() { return itsLoad; }
  inline unsigned int Throttle() { return itsThrottle; }
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
  ~BackendServer() {}
};

}  // namespace SmartMet
