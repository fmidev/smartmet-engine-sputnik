#include "RandomForwarder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
RandomForwarder::~RandomForwarder() = default;

RandomForwarder::RandomForwarder() : BackendForwarder(0.0) {}

void RandomForwarder::redistribute(Spine::Reactor& /* theReactor */)
{
  try
  {
    std::vector<float> probVec;
    probVec.reserve(itsBackendInfos.size());

    for (unsigned i = 0; i < itsBackendInfos.size(); ++i)
    {
      probVec.push_back(1);
    }

    float sum = std::accumulate(probVec.begin(), probVec.end(), 0.0f);

    for (auto& prob : probVec)
      prob /= sum;

    // #ifndef NDEBUG
    // 	if (!itsBackendInfos.empty())
    // 	  {
    // 		std::cout << "Redistributing backend probabilities: ";
    // 		for(const auto & prob : probVec)
    // 		  {
    // 			std::cout << prob << " ";
    // 		  }
    // 		std::cout << std::endl << std::endl;
    // 	  }
    // #endif

    boost::random::discrete_distribution<> theDistribution(probVec);
    itsDistribution = theDistribution;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
