#include "Services.h"
#include "DoubleRandomForwarder.h"
#include "ExponentialConnectionsForwarder.h"
#include "InverseConnectionsForwarder.h"
#include "InverseLoadForwarder.h"
#include "LeastConnectionsForwarder.h"
#include "RandomForwarder.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <smartmet/macgyver/StringConversion.h>
#include <smartmet/spine/Table.h>
#include <csignal>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>

namespace SmartMet
{
BackendServicePtr Services::getService(const Spine::HTTP::Request& theRequest)
{
  try
  {
    const auto uri = theRequest.getResource();

    SmartMet::Spine::ReadLock lock(itsMutex);

    // Check that URI map for server list
    const std::string uri_prefix = itsPrefixMap(uri);
    auto pos = itsServicesByURI.find(uri_prefix);
    if (pos == itsServicesByURI.end())
    {
      // Nothing for this URI found on the list. Return with error.
      std::cout << Fmi::SecondClock::local_time() << " Nothing known about URI requested by "
                << theRequest.getClientIP() << " : " << uri_prefix << std::endl;

      return {};
    }

    // Verify that the list of Services is not empty
    // (could happen if all backends fail to respond.)

    auto theBackendList = pos->second.first;
    if (theBackendList->empty())
    {
      // Nothing for this URI found. Return with error.
      std::cout << Fmi::SecondClock::local_time() << " Backend server list empty for URI " << uri
                << std::endl;

      return {};
    }

    // Select the BackendServer for forwarding
    //
    // NOTE: Uses backend load information
    auto backendRandPtr = pos->second.second;
    std::size_t rndServerSlot = backendRandPtr->getBackend(*itsReactor);
    auto theService = theBackendList->at(rndServerSlot);

#ifdef MYDEBUG
    std::cout << "Broadcast forwarding to backend: " << theService->Backend()->Name() << std::endl;
#endif

    return theService;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Services::removeBackend(const std::string& theHostname, int thePort, const std::string& theURI)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    for (auto& theURIs : itsServicesByURI)
      for (auto it = (*theURIs.second.first).begin(); it != (*theURIs.second.first).end();)
      {
        itsPrefixMap.removeBackend(theURI, *it);
        if (((*it)->Backend()->Name() == theHostname && (*it)->Backend()->Port() == thePort) &&
            (theURI.empty() || theURI == (*it)->URI()))
        {
// Temporarily retire the server from the service.
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << " Removing backend "
                    << (*it)->Backend()->Name() << " seq " << (*it)->SequenceNumber() << " URI "
                    << (*it)->URI() << std::endl;
#endif
          theURIs.second.second->removeBackend(
              (*it)->Backend()->Name(), (*it)->Backend()->Port(), *itsReactor);
          it = (*theURIs.second.first).erase(it);
        }
        else
        {
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << " Keeping backend "
                    << (*it)->Backend()->Name() << " seq " << (*it)->SequenceNumber() << " URI "
                    << (*it)->URI() << std::endl;
#endif
          ++it;
        }
      }

    // If there are no services left, something has gone wrong.
    // Better exit and restart.

    for (const auto& theURIs : itsServicesByURI)
    {
      if (!theURIs.second.first->empty())
        return true;
    }

    std::cout << Fmi::SecondClock::local_time() << " No services left, performing a restart"
              << std::endl;
    // Using exit might generate a coredump, and we want a fast restart
    kill(getpid(), SIGKILL);

    // a dummy return to avoid compiler warnings
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Services::queryBackendAlive(const std::string& theHostName, int thePort)
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsSentinelMutex);

    std::string sname = theHostName + ":" + std::to_string(thePort);
    auto iter = itsSentinels.find(sname);
    if (iter != itsSentinels.end())
      return iter->second->getAlive();

    // Unknown backend, this should not happen
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Services::setBackendAlive(const std::string& theHostName, int thePort)
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsSentinelMutex);

    std::string sname = theHostName + ":" + std::to_string(thePort);
    auto iter = itsSentinels.find(sname);
    if (iter != itsSentinels.end())
    {
#ifdef MYDEBUG
      std::cout << Fmi::SecondClock::local_time() << " Setting backend " << sname << " alive"
                << std::endl;
#endif
      iter->second->setAlive();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Services::signalBackendConnection(const std::string& theHostName, int thePort)
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsSentinelMutex);

    std::string sname = theHostName + ":" + std::to_string(thePort);
    auto iter = itsSentinels.find(sname);
    if (iter != itsSentinels.end())
    {
      iter->second->signalSentConnection();
#ifdef MYDEBUG
      unsigned int count = iter->second->getCurrentThrottle();
      std::cout << Fmi::SecondClock::local_time() << " Incremented backend " << sname
                << " connections to " << count << std::endl;
#endif
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Delete all service information from the list whose
// sequence number doesn't match the latest one.
//
// (This is used for timeouting unresponsive Broadcast requests.)
bool Services::latestSequence(int itsSequenceNumber)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

#ifdef MYDEBUG
    std::cout << "UPDATING TO SEQ " << itsSequenceNumber << std::endl;
#endif

    for (const auto& theURIs : itsServicesByURI)
      for (auto it = (*theURIs.second.first).begin(); it != (*theURIs.second.first).end();)
      {
        if ((*it)->SequenceNumber() != itsSequenceNumber)
        {
// Temporarily retire the server from the service.
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << "Removing sequence "
                    << (*it)->SequenceNumber() << " backend " << (*it)->Backend()->Name() << " URI "
                    << (*it)->URI() << std::endl;
#endif

          // Remove this entry as it has unmatching sequence number
          theURIs.second.second->removeBackend(
              (*it)->Backend()->Name(), (*it)->Backend()->Port(), *itsReactor);
          it = (*theURIs.second.first).erase(it);
        }
        else
        {
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << "  sequence " << (*it)->SequenceNumber()
                    << " backend " << (*it)->Backend()->Name() << " URI " << (*it)->URI()
                    << std::endl;
#endif
          ++it;
        }
      }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Services::addService(const BackendServicePtr& theBackendService,
                          const std::string& theFrontendURI,
                          float theLoad,
                          unsigned int theThrottle)
{
  try
  {
    // Sanity check
    if (!theBackendService)
      return false;

      // Check if URI is already known

#ifdef MYDEBUG
    std::cout << Fmi::SecondClock::local_time() << " Adding service "
              << theBackendService->Backend()->Name() << " seq "
              << theBackendService->SequenceNumber() << " URI " << theFrontendURI << std::endl;
#endif

    SmartMet::Spine::WriteLock lock(itsMutex);

    if (theBackendService->DefinesPrefix())
    {
      itsPrefixMap.addPrefix(theFrontendURI, theBackendService);
    }

    const auto pos = itsServicesByURI.find(theFrontendURI);
    if (pos != itsServicesByURI.end())
    {
      // URI already known, take the URI's existing std::list
      pos->second.first->push_back(theBackendService);
      pos->second.second->addBackend(theBackendService->Backend()->Name(),
                                     theBackendService->Backend()->Port(),
                                     theLoad,
                                     *itsReactor);
    }
    else
    {
      // URI not known, make new list
      BackendServiceListPtr newlist(new BackendServiceList());
      newlist->push_back(theBackendService);

      BackendForwarderPtr theForwarder;
      switch (itsFwdMode)
      {
        case ForwardingMode::InverseLoad:
          theForwarder = BackendForwarderPtr(new InverseLoadForwarder(itsBalancingCoefficient));
          break;
        case ForwardingMode::Random:
          theForwarder = BackendForwarderPtr(new RandomForwarder);
          break;
        case ForwardingMode::DoubleRandom:
          theForwarder = BackendForwarderPtr(new DoubleRandomForwarder);
          break;
        case ForwardingMode::LeastConnections:
          theForwarder = BackendForwarderPtr(new LeastConnectionsForwarder);
          break;
        case ForwardingMode::InverseConnections:
          theForwarder =
              BackendForwarderPtr(new InverseConnectionsForwarder(itsBalancingCoefficient));
          break;
        case ForwardingMode::ExponentialConnections:
          theForwarder =
              BackendForwarderPtr(new ExponentialConnectionsForwarder(itsBalancingCoefficient));
          break;
      }

      theForwarder->addBackend(theBackendService->Backend()->Name(),
                               theBackendService->Backend()->Port(),
                               theLoad,
                               *itsReactor);

      itsServicesByURI[theFrontendURI] = std::make_pair(newlist, theForwarder);
    }

    // Update sentinel information for this backend
    SmartMet::Spine::WriteLock sentinelLock(itsSentinelMutex);

    std::string sname = theBackendService->Backend()->Name() + ":" +
                        std::to_string(theBackendService->Backend()->Port());

    auto it = itsSentinels.find(sname);
    if (it == itsSentinels.end())
    {
      itsSentinels.insert(std::make_pair(sname, std::make_shared<BackendSentinel>(theThrottle)));
    }
    else
    {
      // Make a new sentinel, updating the throttle value
      itsSentinels.erase(it);
      itsSentinels.insert(std::make_pair(sname, std::make_shared<BackendSentinel>(theThrottle)));
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief List backends with given service or all services
 */
// ----------------------------------------------------------------------

std::shared_ptr<SmartMet::Spine::Table> Services::backends(const std::string& service) const
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    std::shared_ptr<SmartMet::Spine::Table> ret = std::make_shared<SmartMet::Spine::Table>();

    std::string serviceuri = "/" + itsPrefixMap(service);

    // List all backends with matching URI

    std::size_t row = 0;
    for (const auto& uri : itsServicesByURI)
    {
      if (uri.first == serviceuri)
      {
        for (const auto& backend : *uri.second.first)
        {
          ret->set(0, row, backend->Backend()->Name());
          ret->set(1, row, backend->Backend()->IP());
          ret->set(2, row, Fmi::to_string(backend->Backend()->Port()));
          ++row;
        }
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return list of backends with given service or all service
 */
// ----------------------------------------------------------------------

Services::BackendList Services::getBackendList(const std::string& service) const
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    std::string serviceuri = "/" + itsPrefixMap(service);

    // List all backends with matching URI

    BackendList theList;
    for (const auto& uri : itsServicesByURI)
    {
      if (uri.first == serviceuri)
      {
        for (const auto& backend : *uri.second.first)
        {
          theList.push_back(boost::make_tuple(
              backend->Backend()->Name(), backend->Backend()->IP(), backend->Backend()->Port()));
        }
      }
    }

    return theList;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate status report
 */
// ----------------------------------------------------------------------

void Services::status(std::ostream& out) const
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    // Read the Backend information list

    out << "<ul>" << std::endl;
    for (const auto& uri : itsServicesByURI)
    {
      out << "<li>URI " << uri.first << "</li>" << std::endl;

      out << "<ol>" << std::endl;
      for (const auto& backend : *uri.second.first)
      {
        out << "<li>" << backend->Backend()->Name() << backend->URI() << " ["
            << backend->Backend()->IP() << ":" << backend->Backend()->Port() << "]"
            << " [Sequence " << backend->SequenceNumber() << "]"
            << "</li>" << std::endl;
      }
      out << "</ol>" << std::endl;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Services::setForwarding(const std::string& theMode, float balancingCoefficient)
{
  try
  {
    if (theMode == "inverseload")
      itsFwdMode = ForwardingMode::InverseLoad;
    else if (theMode == "random")
      itsFwdMode = ForwardingMode::Random;
    else if (theMode == "doublerandom")
      itsFwdMode = ForwardingMode::DoubleRandom;
    else if (theMode == "inverseconnections")
      itsFwdMode = ForwardingMode::InverseConnections;
    else if (theMode == "exponentialconnections")
      itsFwdMode = ForwardingMode::ExponentialConnections;
    else if (theMode == "leastconnections")
      itsFwdMode = ForwardingMode::LeastConnections;
    else
      throw Fmi::Exception(BCP, "Unknown backend forwarding mode: '" + theMode + "'");

    itsBalancingCoefficient = balancingCoefficient;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Services::setReactor(Spine::Reactor& theReactor)
{
  itsReactor = &theReactor;
}

}  // namespace SmartMet
