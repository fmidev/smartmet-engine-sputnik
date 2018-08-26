#include "InverseLoadForwarder.h"
#include <spine/Exception.h>

namespace SmartMet
{
InverseLoadForwarder::~InverseLoadForwarder() = default;

InverseLoadForwarder::InverseLoadForwarder(float balancingCoefficient)
    : BackendForwarder(balancingCoefficient)
{
}

void InverseLoadForwarder::redistribute()
{
  try
  {
    std::vector<float> probVec;
    probVec.reserve(itsBackendInfos.size());

    for (const auto& info : itsBackendInfos)
    {
      probVec.push_back(1.0f / (1.0f + itsBalancingCoefficient * info.load));
    }

    float sum = std::accumulate(probVec.begin(), probVec.end(), 0.0f);

    for (auto& prob : probVec)
      prob /= sum;

    boost::random::discrete_distribution<> theDistribution(probVec);
    itsDistribution = theDistribution;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
