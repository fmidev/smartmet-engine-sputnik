#include "BackendForwarder.h"
#include <algorithm>
#include <macgyver/Exception.h>

namespace SmartMet
{
BackendForwarder::BackendForwarder(float balancingCoefficient)
    : itsGenerator(boost::numeric_cast<unsigned int>(time(nullptr))),
      itsBalancingCoefficient(balancingCoefficient)
{
}

BackendForwarder::~BackendForwarder() = default;

void BackendForwarder::setBackends(const std::vector<BackendInfo>& backends,
                                   Spine::Reactor& theReactor)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    itsBackendInfos = backends;

    this->redistribute(theReactor);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t BackendForwarder::getBackend(Spine::Reactor& theReactor)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    rebalance(theReactor);
    return boost::numeric_cast<std::size_t>(itsDistribution(itsGenerator));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void BackendForwarder::addBackend(const std::string& hostName,
                                  int port,
                                  float load,
                                  Spine::Reactor& theReactor)
{
  try
  {
#ifdef MYDEBUG
    std::cout << "BackendForwarder adding backend " << hostName << ":" << port << " with load "
              << load << std::endl;
#endif

    SmartMet::Spine::WriteLock lock(itsMutex);

    itsBackendInfos.emplace_back(hostName, port, load);

    this->redistribute(theReactor);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void BackendForwarder::removeBackend(const std::string& hostName,
                                     int port,
                                     Spine::Reactor& theReactor)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    itsBackendInfos.erase(
      std::remove_copy_if(
        itsBackendInfos.begin(),
        itsBackendInfos.end(),
        itsBackendInfos.begin(),
        [&hostName, port](const BackendInfo& info) -> bool
	{
	  return info.hostName == hostName && info.port == port;
	}),
      itsBackendInfos.end());

    this->redistribute(theReactor);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
