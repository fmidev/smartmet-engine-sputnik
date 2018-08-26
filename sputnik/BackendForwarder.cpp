#include "BackendForwarder.h"
#include <spine/Exception.h>

namespace SmartMet
{
BackendForwarder::BackendForwarder(float balancingCoefficient)
    : itsGenerator(boost::numeric_cast<unsigned int>(time(nullptr))),
      itsBalancingCoefficient(balancingCoefficient)
{
}

BackendForwarder::~BackendForwarder() {}

void BackendForwarder::setBackends(const std::vector<BackendInfo>& backends)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    itsBackendInfos = backends;

    this->redistribute();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t BackendForwarder::getBackend()
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    return boost::numeric_cast<std::size_t>(itsDistribution(itsGenerator));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void BackendForwarder::addBackend(const std::string& hostName, int port, float load)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    itsBackendInfos.emplace_back(hostName, port, load);

    this->redistribute();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void BackendForwarder::removeBackend(const std::string& hostName, int port)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    for (auto it = itsBackendInfos.begin(); it != itsBackendInfos.end();)
    {
      if (it->hostName == hostName && it->port == port)
      {
        // Remove just one backend with this name. There may be duplicates
        itsBackendInfos.erase(it);
        break;
      }
      else
      {
        ++it;
      }
    }

    this->redistribute();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
