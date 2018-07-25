#include "InverseLoadForwarder.h"
#include <boost/foreach.hpp>
#include <spine/Exception.h>

namespace SmartMet
{
InverseLoadForwarder::~InverseLoadForwarder() {}
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

    BOOST_FOREACH (auto& info, itsBackendInfos)
    {
      probVec.push_back(1.0f / (1.0f + itsBalancingCoefficient * info.load));
    }

    float sum = std::accumulate(probVec.begin(), probVec.end(), 0.0f);

    for (auto it = probVec.begin(); it != probVec.end(); ++it)
    {
      *it /= sum;
    }

    boost::random::discrete_distribution<> theDistribution(probVec);
    itsDistribution = theDistribution;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
