#include "Services.h"
#include "InverseLoadForwarder.h"
#include "RandomForwarder.h"

#include <smartmet/spine/Table.h>
#include <spine/Exception.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>

namespace SmartMet
{
BackendServicePtr Services::getService(const std::string& theURI)
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    // Check that URI map for server list
    auto pos = itsServicesByURI.find(theURI);
    if (pos == itsServicesByURI.end())
    {
      // Nothing for this URI found on the list. Return with error.
      std::cout << boost::posix_time::second_clock::local_time() << " Nothing known about URI "
                << theURI << std::endl;

      return BackendServicePtr();
    }

    // Verify that the list of Services is not empty
    // (could happen if all backends fail to respond.)

    auto theBackendList = pos->second.first;
    if (theBackendList->empty())
    {
      // Nothing for this URI found. Return with error.
      std::cout << boost::posix_time::second_clock::local_time()
                << " Backend server list empty for URI " << theURI << std::endl;

      return BackendServicePtr();
    }

    // Select the BackendServer for forwarding
    //
    // NOTE: Uses backend load information
    auto backendRandPtr = pos->second.second;
    std::size_t rndServerSlot = backendRandPtr->getBackend();
    auto theService = theBackendList->at(rndServerSlot);

#ifdef MYDEBUG
    std::cout << "Broadcast forwarding to backend: " << theService->Backend()->Name() << std::endl;
#endif

    return theService;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Services::removeBackend(const std::string& theHostname, int thePort, const std::string& theURI)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    BOOST_FOREACH (auto& theURIs, itsServicesByURI)
      for (auto it = (*theURIs.second.first).begin(); it != (*theURIs.second.first).end();)
      {
        if (((*it)->Backend()->Name() == theHostname && (*it)->Backend()->Port() == thePort) &&
            (theURI.empty() || theURI == (*it)->URI()))
        {
// Temporarily retire the server from the service.
#ifdef MYDEBUG
          std::cout << boost::posix_time::second_clock::local_time() << " Removing backend "
                    << (*it)->Backend()->Name() << " seq " << (*it)->SequenceNumber() << " URI "
                    << (*it)->URI() << std::endl;
#endif
          theURIs.second.second->removeBackend((*it)->Backend()->Name(), (*it)->Backend()->Port());
          it = (*theURIs.second.first).erase(it);
        }
        else
        {
#ifdef MYDEBUG
          std::cout << boost::posix_time::second_clock::local_time() << " Keeping backend "
                    << (*it)->Backend()->Name() << " seq " << (*it)->SequenceNumber() << " URI "
                    << (*it)->URI() << std::endl;
#endif
          ++it;
        }
      }

    // If there are no services left, something has gone wrong.
    // Better exit and restart.

    BOOST_FOREACH (auto& theURIs, itsServicesByURI)
    {
      if (!theURIs.second.first->empty())
        return true;
    }

    std::cout << boost::posix_time::second_clock::local_time()
              << " No services left, performing a restart" << std::endl;
    exit(123);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    {
      return iter->second->getAlive();
    }
    else
    {
      // Unknown backend, this should not happen
      return false;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
      std::cout << boost::posix_time::second_clock::local_time() << " Setting backend " << sname
                << " alive" << std::endl;
#endif
      iter->second->setAlive();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
      std::cout << boost::posix_time::second_clock::local_time() << " Incremented backend " << sname
                << " connections to " << count << std::endl;
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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

    BOOST_FOREACH (auto& theURIs, itsServicesByURI)
      for (auto it = (*theURIs.second.first).begin(); it != (*theURIs.second.first).end();)
      {
        if ((*it)->SequenceNumber() != itsSequenceNumber)
        {
// Temporarily retire the server from the service.
#ifdef MYDEBUG
          std::cout << boost::posix_time::second_clock::local_time() << "Removing sequence "
                    << (*it)->SequenceNumber() << " backend " << (*it)->Backend()->Name() << " URI "
                    << (*it)->URI() << std::endl;
#endif

          // Remove this entry as it has unmatching sequence number
          theURIs.second.second->removeBackend((*it)->Backend()->Name(), (*it)->Backend()->Port());
          it = (*theURIs.second.first).erase(it);
        }
        else
        {
#ifdef MYDEBUG
          std::cout << boost::posix_time::second_clock::local_time() << "  sequence "
                    << (*it)->SequenceNumber() << " backend " << (*it)->Backend()->Name() << " URI "
                    << (*it)->URI() << std::endl;
#endif
          ++it;
        }
      }

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Services::addService(BackendServicePtr theBackendService,
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
    std::cout << boost::posix_time::second_clock::local_time() << " Adding service "
              << theBackendService->Backend()->Name() << " seq "
              << theBackendService->SequenceNumber() << " URI " << theFrontendURI << std::endl;
#endif

    SmartMet::Spine::WriteLock lock(itsMutex);

    const auto pos = itsServicesByURI.find(theFrontendURI);
    if (pos != itsServicesByURI.end())
    {
      // URI already known, take the URI's existing std::list
      pos->second.first->push_back(theBackendService);
      pos->second.second->addBackend(
          theBackendService->Backend()->Name(), theBackendService->Backend()->Port(), theLoad);
    }
    else
    {
      // URI not known, make new list
      BackendServiceListPtr newlist(new BackendServiceList());
      newlist->push_back(theBackendService);

      BackendForwarderPtr theForwarder;
      if (itsFwdMode == ForwardingMode::InverseLoad)
        theForwarder = BackendForwarderPtr(new InverseLoadForwarder(itsBalancingCoefficient));
      else if (itsFwdMode == ForwardingMode::Random)
        theForwarder = BackendForwarderPtr(new RandomForwarder);

      theForwarder->addBackend(
          theBackendService->Backend()->Name(), theBackendService->Backend()->Port(), theLoad);

      itsServicesByURI[theFrontendURI] = std::make_pair(newlist, theForwarder);
    }

    // Update sentinel information for this backend
    SmartMet::Spine::WriteLock sentinelLock(itsSentinelMutex);

    std::string sname = theBackendService->Backend()->Name() + ":" +
                        std::to_string(theBackendService->Backend()->Port());

    auto it = itsSentinels.find(sname);
    if (it == itsSentinels.end())
    {
      itsSentinels.insert(std::make_pair(sname, boost::make_shared<BackendSentinel>(theThrottle)));
    }
    else
    {
      // Make a new sentinel, updating the throttle value
      itsSentinels.erase(it);
      itsSentinels.insert(std::make_pair(sname, boost::make_shared<BackendSentinel>(theThrottle)));
    }

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief List backends with given service or all services
 */
// ----------------------------------------------------------------------

boost::shared_ptr<SmartMet::Spine::Table> Services::backends(const std::string& service) const
{
  try
  {
    SmartMet::Spine::ReadLock lock(itsMutex);

    boost::shared_ptr<SmartMet::Spine::Table> ret(new SmartMet::Spine::Table());

    std::string serviceuri = "/" + service;

    // List all backends with matching URI

    std::size_t row = 0;
    BOOST_FOREACH (const ServiceURIMap::value_type& uri, itsServicesByURI)
    {
      if (uri.first == serviceuri)
      {
        for (auto it = (*uri.second.first).begin(); it != (*uri.second.first).end(); ++it)
        {
          ret->set(0, row, (*it)->Backend()->Name());
          ret->set(1, row, (*it)->Backend()->IP());
          ret->set(2, row, boost::lexical_cast<std::string>((*it)->Backend()->Port()));
          ++row;
        }
      }
    }

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string serviceuri = "/" + service;

    // List all backends with matching URI

    BackendList theList;
    BOOST_FOREACH (const ServiceURIMap::value_type& uri, itsServicesByURI)
    {
      if (uri.first == serviceuri)
      {
        for (auto it = (*uri.second.first).begin(); it != (*uri.second.first).end(); ++it)
        {
          theList.push_back(boost::make_tuple(
              (*it)->Backend()->Name(), (*it)->Backend()->IP(), (*it)->Backend()->Port()));
        }
      }
    }

    return theList;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    BOOST_FOREACH (const ServiceURIMap::value_type& uri, itsServicesByURI)
    {
      out << "<li>URI " << uri.first << "</li>" << std::endl;

      out << "<ol>" << std::endl;
      for (auto it = (*uri.second.first).begin(); it != (*uri.second.first).end(); ++it)
      {
        out << "<li>" << (*it)->Backend()->Name() << (*it)->URI() << " [" << (*it)->Backend()->IP()
            << ":" << (*it)->Backend()->Port() << "]"
            << " [Sequence " << (*it)->SequenceNumber() << "]"
            << "</li>" << std::endl;
      }
      out << "</ol>" << std::endl;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    else
      throw SmartMet::Spine::Exception(BCP, "Unknown backend forwarding mode: '" + theMode + "'");

    itsBalancingCoefficient = balancingCoefficient;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
