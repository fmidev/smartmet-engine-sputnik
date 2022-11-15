#include "BackendForwarder.h"
#include <macgyver/Exception.h>
#include <algorithm>

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

    // Seems to me this code works incorrectly. It should remove the host from the previous
    // cycle only. If the host was not present then, the code will remove the host from
    // the current cycle. Hence the host will activate only after two cycles. - Mika

    for (auto it = itsBackendInfos.begin(); it != itsBackendInfos.end(); ++it)
    {
      if (it->hostName == hostName && it->port == port)
      {
        // Remove just one backend with this name. There may be duplicates
        itsBackendInfos.erase(it);
        break;
      }
    }

    this->redistribute(theReactor);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
