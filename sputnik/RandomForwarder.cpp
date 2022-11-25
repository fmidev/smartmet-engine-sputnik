#include "RandomForwarder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
RandomForwarder::~RandomForwarder() = default;

RandomForwarder::RandomForwarder() : BackendForwarder(0.0) {}

std::size_t RandomForwarder::getBackend(Spine::Reactor& theReactor)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    if (itsBackendInfos.empty())
      throw Fmi::Exception(BCP, "No backends available!");
    auto maxnum = static_cast<int>(itsBackendInfos.size() - 1);
    boost::random::uniform_int_distribution<> dist{0, maxnum};
    return boost::numeric_cast<std::size_t>(dist(itsGenerator));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
