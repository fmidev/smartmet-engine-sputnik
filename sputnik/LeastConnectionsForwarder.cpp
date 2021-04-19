#include "LeastConnectionsForwarder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
LeastConnectionsForwarder::~LeastConnectionsForwarder() = default;

LeastConnectionsForwarder::LeastConnectionsForwarder() : BackendForwarder(0.0) {}

void LeastConnectionsForwarder::redistribute(Spine::Reactor& theReactor)
{
  try
  {
    auto connections = theReactor.getBackendRequestStatus();

    std::vector<float> probVec;
    probVec.reserve(itsBackendInfos.size());

    // Find minimum number of connections
    int min_count = -1;
    for (const auto& info : itsBackendInfos)
    {
      int count = connections[info.hostName][info.port];  // zero if not found
      if (min_count < 0)
        min_count = count;
      else
        min_count = std::min(min_count, count);
    }

    // Choose a server with min_count connections
    for (const auto& info : itsBackendInfos)
    {
      int count = connections[info.hostName][info.port];  // zero if not found

      if (count == min_count)
        probVec.push_back(1.0F);
      else
        probVec.push_back(0.0F);
    }

    float sum = std::accumulate(probVec.begin(), probVec.end(), 0.0F);

    if (sum > 0)
      for (auto& prob : probVec)
        prob /= sum;

    boost::random::discrete_distribution<> theDistribution(probVec);
    itsDistribution = theDistribution;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void LeastConnectionsForwarder::rebalance(Spine::Reactor& theReactor)
{
  redistribute(theReactor);
}

}  // namespace SmartMet
