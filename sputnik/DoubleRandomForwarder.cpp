#include "DoubleRandomForwarder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
DoubleRandomForwarder::~DoubleRandomForwarder() = default;

DoubleRandomForwarder::DoubleRandomForwarder() : BackendForwarder(0.0) {}

std::size_t DoubleRandomForwarder::getBackend(Spine::Reactor& theReactor)
{
  try
  {
    auto connections = theReactor.getBackendRequestStatus();
    SmartMet::Spine::WriteLock lock(itsMutex);
    if (itsBackendInfos.empty())
      throw Fmi::Exception(BCP, "No backends available!");

    // Pick two random candidates
    auto maxnum = static_cast<int>(itsBackendInfos.size() - 1);
    boost::random::uniform_int_distribution<> dist{0, maxnum};
    auto num1 = boost::numeric_cast<std::size_t>(dist(itsGenerator));
    auto num2 = boost::numeric_cast<std::size_t>(dist(itsGenerator));

    // Choose the one with less connections
    const auto& info1 = itsBackendInfos[num1];
    const auto& info2 = itsBackendInfos[num2];

    auto count1 = connections[info1.hostName][info1.port];
    auto count2 = connections[info2.hostName][info2.port];

    return (count1 <= count2 ? num1 : num2);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
