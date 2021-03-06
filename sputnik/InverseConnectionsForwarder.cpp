#include "InverseConnectionsForwarder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
InverseConnectionsForwarder::~InverseConnectionsForwarder() = default;

InverseConnectionsForwarder::InverseConnectionsForwarder(float balancingCoefficient)
    : BackendForwarder(balancingCoefficient)
{
}

void InverseConnectionsForwarder::redistribute(Spine::Reactor& theReactor)
{
  try
  {
    auto connections = theReactor.getBackendRequestStatus();

    std::vector<float> probVec;
    probVec.reserve(itsBackendInfos.size());

    for (const auto& info : itsBackendInfos)
    {
      int count = connections[info.hostName][info.port];  // zero if not found

      probVec.push_back(1.0F / (1.0F + itsBalancingCoefficient * count));
#ifdef MYDEBUG
      std::cout << "Inverse prob: " << probVec.back() << " from conns " << count << std::endl;
#endif
    }

    float sum = std::accumulate(probVec.begin(), probVec.end(), 0.0F);

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

void InverseConnectionsForwarder::rebalance(Spine::Reactor& theReactor)
{
  redistribute(theReactor);
}

}  // namespace SmartMet
