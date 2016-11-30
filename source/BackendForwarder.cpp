#include "BackendForwarder.h"
#include <spine/Exception.h>

namespace SmartMet
{
BackendForwarder::BackendForwarder(float balancingCoefficient)
    : itsGenerator(boost::numeric_cast<unsigned int>(time(NULL))),
      itsBalancingCoefficient(balancingCoefficient)
{
}

BackendForwarder::~BackendForwarder()
{
}

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::size_t BackendForwarder::getBackend()
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);
    std::size_t idx = boost::numeric_cast<std::size_t>(itsDistribution(itsGenerator));
    return idx;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace SmartMet
