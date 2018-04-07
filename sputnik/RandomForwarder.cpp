#include "RandomForwarder.h"
#include <spine/Exception.h>

namespace SmartMet
{
RandomForwarder::~RandomForwarder() {}

RandomForwarder::RandomForwarder() : BackendForwarder(0.0) {}

void RandomForwarder::redistribute()
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

    for (auto it = probVec.begin(); it != probVec.end(); ++it)
    {
      *it /= sum;
    }

    // #ifndef NDEBUG
    // 	if (!itsBackendInfos.empty())
    // 	  {
    // 		std::cout << "Redistributing backend probabilities: ";
    // 		BOOST_FOREACH(auto & prob, probVec)
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace SmartMet
