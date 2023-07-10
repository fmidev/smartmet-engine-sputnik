
#pragma once

#include <spine/Thread.h>

namespace SmartMet
{
class BackendSentinel
{
 public:
  ~BackendSentinel();
  explicit BackendSentinel(unsigned int theThrottle);

  BackendSentinel() = delete;
  BackendSentinel(const BackendSentinel& other) = delete;
  BackendSentinel& operator=(const BackendSentinel& other) = delete;
  BackendSentinel(BackendSentinel&& other) = delete;
  BackendSentinel& operator=(BackendSentinel&& other) = delete;

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
