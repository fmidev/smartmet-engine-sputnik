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

using namespace std::string_literals;

namespace SmartMet
{
Services::Services()
{
  itsSnapshot.store(std::make_shared<Snapshot>());
  itsSnapshotVersion.store(1);
}

BackendForwarderPtr Services::createForwarder(const std::vector<BackendServicePtr>& services,
                                              float defaultLoad) const
{
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
      theForwarder = BackendForwarderPtr(new InverseConnectionsForwarder(itsBalancingCoefficient));
      break;
    case ForwardingMode::ExponentialConnections:
      theForwarder =
          BackendForwarderPtr(new ExponentialConnectionsForwarder(itsBalancingCoefficient));
      break;
  }

  for (const auto& backendService : services)
  {
    if (!backendService)
      continue;

    const auto backend = backendService->Backend();
    if (!backend)
      continue;

    const float load = backend->Load() > 0 ? backend->Load() : defaultLoad;
    theForwarder->addBackend(backend->Name(), backend->Port(), load, *itsReactor);
  }

  return theForwarder;
}

BackendServicePtr Services::getService(const Spine::HTTP::Request& theRequest)
{
  try
  {
    ++itsGetServiceCalls;

    const auto uri = theRequest.getResource();
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
    {
      ++itsGetServiceMisses;
      return {};
    }

    // Check that URI map for server list
    const std::string uri_prefix = itsPrefixMap(uri);
    auto pos = snapshot->servicesByURI.find(uri_prefix);
    if (pos == snapshot->servicesByURI.end())
    {
      ++itsGetServiceMisses;

      // Nothing for this URI found on the list. Return with error.
      std::cout << Fmi::SecondClock::local_time() << " Nothing known about URI requested by "
                << theRequest.getClientIP() << " : " << uri_prefix << '\n';

      return {};
    }

    // Verify that the list of Services is not empty
    // (could happen if all backends fail to respond.)

    auto theBackendList = pos->second.first;
    if (theBackendList->empty())
    {
      ++itsGetServiceMisses;

      // Nothing for this URI found. Return with error.
      std::cout << Fmi::SecondClock::local_time() << " Backend server list empty for URI " << uri
                << '\n';

      return {};
    }

    // Select the BackendServer for forwarding
    //
    // NOTE: Uses backend load information
    auto backendRandPtr = pos->second.second;
    std::size_t rndServerSlot = backendRandPtr->getBackend(*itsReactor);
    if (rndServerSlot >= theBackendList->size())
      rndServerSlot %= theBackendList->size();
    auto theService = theBackendList->at(rndServerSlot);

#ifdef MYDEBUG
    std::cout << "Broadcast forwarding to backend: " << theService->Backend()->Name() << '\n';
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
    auto oldSnapshot = itsSnapshot.load();
    if (!oldSnapshot)
      oldSnapshot = std::make_shared<Snapshot>();

    auto newSnapshot = std::make_shared<Snapshot>(*oldSnapshot);

    bool anyServicesLeft = false;
    for (auto& theURIs : newSnapshot->servicesByURI)
    {
      BackendServiceList newList;
      const auto& oldList = *theURIs.second.first;
      newList.reserve(oldList.size());

      for (const auto& backendService : oldList)
      {
        itsPrefixMap.removeBackend(theURI, backendService);

        const bool matchingBackend =
            backendService && backendService->Backend() &&
            backendService->Backend()->Name() == theHostname &&
            backendService->Backend()->Port() == thePort;

        if (matchingBackend && (theURI.empty() || theURI == backendService->URI()))
        {
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << " Removing backend "
                    << backendService->Backend()->Name() << " seq "
                    << backendService->SequenceNumber() << " URI " << backendService->URI() << '\n';
#endif
          continue;
        }

#ifdef MYDEBUG
        std::cout << Fmi::SecondClock::local_time() << " Keeping backend "
                  << backendService->Backend()->Name() << " seq "
                  << backendService->SequenceNumber() << " URI " << backendService->URI() << '\n';
#endif
        newList.push_back(backendService);
      }

      theURIs.second.first = std::make_shared<BackendServiceList>(std::move(newList));
      theURIs.second.second = createForwarder(*theURIs.second.first, 1.0F);
      if (!theURIs.second.first->empty())
        anyServicesLeft = true;
    }

    itsSnapshot.store(newSnapshot);
    ++itsSnapshotVersion;

    // If there are no services left, something has gone wrong.
    // Better exit and restart.
    if (anyServicesLeft)
      return true;

    std::cout << Fmi::SecondClock::local_time() << " No services left, performing a restart\n";
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
    std::string sname = theHostName + ":" + std::to_string(thePort);
    std::shared_ptr<BackendSentinel> sentinel;
    {
      SmartMet::Spine::ReadLock lock(itsSentinelMutex);
      auto iter = itsSentinels.find(sname);
      if (iter != itsSentinels.end())
        sentinel = iter->second;
    }

    if (sentinel)
      return sentinel->getAlive();

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
    std::string sname = theHostName + ":" + std::to_string(thePort);
    std::shared_ptr<BackendSentinel> sentinel;
    {
      SmartMet::Spine::ReadLock lock(itsSentinelMutex);
      auto iter = itsSentinels.find(sname);
      if (iter != itsSentinels.end())
        sentinel = iter->second;
    }

    if (sentinel)
    {
#ifdef MYDEBUG
      std::cout << Fmi::SecondClock::local_time() << " Setting backend " << sname << " alive\n";
#endif
      sentinel->setAlive();
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
    std::string sname = theHostName + ":" + std::to_string(thePort);
    std::shared_ptr<BackendSentinel> sentinel;
    {
      SmartMet::Spine::ReadLock lock(itsSentinelMutex);
      auto iter = itsSentinels.find(sname);
      if (iter != itsSentinels.end())
        sentinel = iter->second;
    }

    if (sentinel)
    {
      sentinel->signalSentConnection();
#ifdef MYDEBUG
      unsigned int count = sentinel->getCurrentThrottle();
      std::cout << Fmi::SecondClock::local_time() << " Incremented backend " << sname
                << " connections to " << count << '\n';
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
    auto oldSnapshot = itsSnapshot.load();
    if (!oldSnapshot)
      oldSnapshot = std::make_shared<Snapshot>();

    auto newSnapshot = std::make_shared<Snapshot>(*oldSnapshot);

#ifdef MYDEBUG
    std::cout << "UPDATING TO SEQ " << itsSequenceNumber << '\n';
#endif

    // Clean up services
    for (auto& theURIs : newSnapshot->servicesByURI)
    {
      BackendServiceList newList;
      const auto& oldList = *theURIs.second.first;
      newList.reserve(oldList.size());

      for (const auto& backendService : oldList)
      {
        if (backendService->SequenceNumber() != itsSequenceNumber)
        {
// Temporarily retire the server from the service.
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << "Removing sequence "
                    << backendService->SequenceNumber() << " backend "
                    << backendService->Backend()->Name() << " URI " << backendService->URI() << '\n';
#endif
        }
        else
        {
#ifdef MYDEBUG
          std::cout << Fmi::SecondClock::local_time() << "  sequence "
                    << backendService->SequenceNumber() << " backend "
                    << backendService->Backend()->Name() << " URI " << backendService->URI() << '\n';
#endif
          newList.push_back(backendService);
        }
      }
      theURIs.second.first = std::make_shared<BackendServiceList>(std::move(newList));
      theURIs.second.second = createForwarder(*theURIs.second.first, 1.0F);
    }

    // Clean up info requests - remove entries from backends that didn't respond
    for (auto& infoReqPair : newSnapshot->backendInfoRequests)
    {
      BackendInfoRequestList newList;
      const auto& requestList = infoReqPair.second;
      if (requestList)
      {
        newList.reserve(requestList->size());
        for (const auto& infoRequest : *requestList)
        {
          if (infoRequest->SequenceNumber() != itsSequenceNumber)
          {
#ifdef MYDEBUG
            std::cout << Fmi::SecondClock::local_time() << "Removing info request sequence "
                      << infoRequest->SequenceNumber() << " backend "
                      << infoRequest->Backend()->Name() << " name " << infoRequest->Name() << '\n';
#endif
          }
          else
          {
#ifdef MYDEBUG
            std::cout << Fmi::SecondClock::local_time() << "  info request sequence "
                      << infoRequest->SequenceNumber() << " backend "
                      << infoRequest->Backend()->Name() << " name " << infoRequest->Name() << '\n';
#endif
            newList.push_back(infoRequest);
          }
        }
      }
      infoReqPair.second = std::make_shared<BackendInfoRequestList>(std::move(newList));
    }

    itsSnapshot.store(newSnapshot);
    ++itsSnapshotVersion;

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
              << theBackendService->SequenceNumber() << " URI " << theFrontendURI << '\n';
#endif

    SmartMet::Spine::WriteLock lock(itsMutex);
    auto oldSnapshot = itsSnapshot.load();
    if (!oldSnapshot)
      oldSnapshot = std::make_shared<Snapshot>();

    auto newSnapshot = std::make_shared<Snapshot>(*oldSnapshot);

    if (theBackendService->DefinesPrefix())
    {
      itsPrefixMap.addPrefix(theFrontendURI, theBackendService);
    }

    BackendServiceList newList;
    const auto pos = newSnapshot->servicesByURI.find(theFrontendURI);
    if (pos != newSnapshot->servicesByURI.end() && pos->second.first)
      newList = *pos->second.first;

    newList.push_back(theBackendService);
    auto newListPtr = std::make_shared<BackendServiceList>(std::move(newList));
    newSnapshot->servicesByURI[theFrontendURI] =
        std::make_pair(newListPtr, createForwarder(*newListPtr, theLoad));

    itsSnapshot.store(newSnapshot);
    ++itsSnapshotVersion;

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

std::unique_ptr<SmartMet::Spine::Table> Services::backends(const std::string& service,
                                                           bool full) const
{
  try
  {
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
      snapshot = std::make_shared<Snapshot>();

    std::unique_ptr<SmartMet::Spine::Table> ret = std::make_unique<SmartMet::Spine::Table>();

    std::string serviceuri = "/" + itsPrefixMap(service);

    ret->setTitle("Backends"s + (service.empty() ? "" : " for service " + service));
    if (full)
      ret->setNames({"Backend", "IP", "Port"});
    else
      ret->setNames({"Backend"});

    // List all backends with matching URI
    std::set<std::string> listedIds;  // To avoid listing the same backend multiple times if it appears in multiple URIs

    std::size_t row = 0;
    for (const auto& uri : snapshot->servicesByURI)
    {
      if (uri.first == serviceuri)
      {
        for (const auto& backend : *uri.second.first)
        {
          const std::string backendName = backend->Backend()->Name();
          const std::string backendIP = backend->Backend()->IP();
          const int backendPort = backend->Backend()->Port();
          const std::string id = backendName + "|" + backendIP + "|" + std::to_string(backendPort);
          if (!listedIds.insert(id).second)
            continue;  // This backend has already been listed, skip it

          ret->set(0, row, backendName);
          if (full)
          {
            ret->set(1, row, backendIP);
            ret->set(2, row, Fmi::to_string(backendPort));
          }
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
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
      snapshot = std::make_shared<Snapshot>();

    std::string serviceuri = "/" + itsPrefixMap(service);

    // List all backends with matching URI

    BackendList theList;
    for (const auto& uri : snapshot->servicesByURI)
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
 * \brief Return list of backends providing the given info request
 */
// ----------------------------------------------------------------------

Services::BackendList Services::getInfoRequestBackendList(const std::string& infoRequestName) const
{
  try
  {
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
      snapshot = std::make_shared<Snapshot>();

    BackendList theList;

    // Find the info request by name
    auto pos = snapshot->backendInfoRequests.find(infoRequestName);
    if (pos != snapshot->backendInfoRequests.end() && pos->second)
    {
      // List all backends providing this info request
      for (const auto& infoRequest : *pos->second)
      {
        theList.push_back(boost::make_tuple(
            infoRequest->Backend()->Name(),
            infoRequest->Backend()->IP(),
            infoRequest->Backend()->Port()));
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

void Services::status(std::ostream& out, bool full) const
{
  try
  {
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
      snapshot = std::make_shared<Snapshot>();

    // Read the Backend information list

    out << "<ul>\n";
    out << "<li>Snapshot version " << itsSnapshotVersion.load() << " getService calls "
      << itsGetServiceCalls.load() << " misses " << itsGetServiceMisses.load() << "</li>\n";
    for (const auto& uri : snapshot->servicesByURI)
    {
      out << "<li>URI " << uri.first << "</li>\n";

      out << "<ol>\n";
      for (const auto& backend : *uri.second.first)
      {
        out << "<li>" << backend->Backend()->Name() << backend->URI();
        if (full)
        {
          out << " [" << backend->Backend()->IP() << ":" << backend->Backend()->Port() << "]"
              << " [Sequence " << backend->SequenceNumber() << "]";
        }
        out << "</li>\n";
      }
      out << "</ol>\n";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add backend info request
 */
// ----------------------------------------------------------------------

bool Services::addBackendInfoRequest(const BackendInfoRequestPtr& theRequest)
{
  try
  {
    // Sanity check
    if (!theRequest)
      return false;

#ifdef MYDEBUG
    std::cout << Fmi::SecondClock::local_time() << " Adding info request "
              << theRequest->Backend()->Name() << " seq "
              << theRequest->SequenceNumber() << " name " << theRequest->Name() << '\n';
#endif

    SmartMet::Spine::WriteLock lock(itsMutex);
    auto oldSnapshot = itsSnapshot.load();
    if (!oldSnapshot)
      oldSnapshot = std::make_shared<Snapshot>();

    auto newSnapshot = std::make_shared<Snapshot>(*oldSnapshot);

    BackendInfoRequestList newList;
    const auto pos = newSnapshot->backendInfoRequests.find(theRequest->Name());
    if (pos != newSnapshot->backendInfoRequests.end() && pos->second)
      newList = *pos->second;

    newList.push_back(theRequest);
    newSnapshot->backendInfoRequests[theRequest->Name()] =
        std::make_shared<BackendInfoRequestList>(std::move(newList));

    itsSnapshot.store(newSnapshot);
    ++itsSnapshotVersion;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get all unique info request names from all backends
 */
// ----------------------------------------------------------------------

std::set<std::string> Services::getInfoRequestNames() const
{
  try
  {
    auto snapshot = itsSnapshot.load();
    if (!snapshot)
      snapshot = std::make_shared<Snapshot>();

    std::set<std::string> names;
    for (const auto& item : snapshot->backendInfoRequests)
    {
      // Only include if there's at least one backend providing this info request
      if (item.second && !item.second->empty())
      {
        names.insert(item.first);
      }
    }

    return names;
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
    SmartMet::Spine::WriteLock lock(itsMutex);

    if (theMode == "inverseload")
      itsFwdMode = ForwardingMode::InverseLoad;
    else if (theMode == "random")
      itsFwdMode = ForwardingMode::Random;
    else if (theMode == "doublerandom")
      itsFwdMode = ForwardingMode::DoubleRandom;
    else if (theMode == "inverseconnections")
      itsFwdMode = ForwardingMode::InverseConnections;
    else if (theMode == "leastconnections")
      itsFwdMode = ForwardingMode::LeastConnections;
    else if (theMode == "exponentialconnections")
      itsFwdMode = ForwardingMode::ExponentialConnections;
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
