#include "BackendSentinel.h"
#include <spine/Exception.h>

namespace SmartMet
{
BackendSentinel::BackendSentinel(unsigned int theThrottle)
    : itsThrottle(theThrottle), itsCurrentThrottle(0)
{
}

BackendSentinel::~BackendSentinel()
{
}

void BackendSentinel::setAlive()
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    itsCurrentThrottle = 0;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool BackendSentinel::getAlive()
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    if (itsThrottle == 0)
    {
      // 0 value throttle means no throttling, always available
      return true;
    }

    if (itsCurrentThrottle > itsThrottle)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace SmartMet
