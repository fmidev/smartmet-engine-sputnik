#include "BackendSentinel.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
BackendSentinel::BackendSentinel(unsigned int theThrottle)
    : itsThrottle(theThrottle), itsCurrentThrottle(0)
{
}

BackendSentinel::~BackendSentinel() = default;

void BackendSentinel::setAlive()
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    itsCurrentThrottle = 0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool BackendSentinel::getAlive()
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    if (itsThrottle == 0)
      return true;  // 0 value throttle means no throttling, always available

    return (itsCurrentThrottle <= itsThrottle);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned int BackendSentinel::getCurrentThrottle()
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);
    return itsCurrentThrottle;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void BackendSentinel::signalSentConnection()
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    ++itsCurrentThrottle;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
