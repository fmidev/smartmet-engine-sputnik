
#pragma once

#include <boost/noncopyable.hpp>
#include <spine/Thread.h>

namespace SmartMet
{
class BackendSentinel : virtual boost::noncopyable
{
 public:
  BackendSentinel(unsigned int theThrottle);

  ~BackendSentinel();

  /*! \brief Acknowledge the backend is responding an throttling can be reset
   *
   * This should be called whenever the backend shows signs of life:
   *
   *  - when it announces its services using a UDP broadcast
   *  - when it sends a HTTP response to a request to the frontend
   */

  void setAlive();

  bool getAlive();

  unsigned int getCurrentThrottle();

  void signalSentConnection();

 private:
  mutable SmartMet::Spine::MutexType itsMutex;

  unsigned int itsThrottle;

  unsigned int itsCurrentThrottle;
};

}  // namespace SmartMet
