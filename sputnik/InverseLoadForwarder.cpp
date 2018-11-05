#include "InverseLoadForwarder.h"
#include <spine/Exception.h>

namespace SmartMet
{
InverseLoadForwarder::~InverseLoadForwarder() = default;

InverseLoadForwarder::InverseLoadForwarder(float balancingCoefficient)
    : BackendForwarder(balancingCoefficient)
{
}

void InverseLoadForwarder::redistribute(Spine::Reactor& /* theReactor */)
{
  try
  {
    std::vector<float> probVec;
    probVec.reserve(itsBackendInfos.size());

    for (const auto& info : itsBackendInfos)
    {
      // Limit load to range 1...inf to avoid problems due to loads close to zero
      auto load = std::max(1.0f, info.load);

      probVec.push_back(1.0f / (1.0f + itsBalancingCoefficient * load));
#ifdef MYDEBUG
      std::cout << "Inverse prob: " << probVec.back() << " from load " << info.load << std::endl;
#endif
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
