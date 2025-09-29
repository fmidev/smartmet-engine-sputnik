#include "BroadcastMessage.pb.h"
#include "Engine.h"
#include "Services.h"
#include <macgyver/Exception.h>
#include <spine/Reactor.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Sputnik
{
// Frontend
void Engine::sendDiscoveryRequest(std::string& theMessageBuffer, int theSequenceNumber)
{
  if (Spine::Reactor::isShuttingDown())
    return;

  try
  {
    // Create a Service message
    SmartMet::BroadcastMessage theMessage;

    // Message header
    theMessage.set_name(itsHostname);
    theMessage.set_messagetype(SmartMet::BroadcastMessage::SERVICE_DISCOVERY_REQUEST);
    theMessage.set_seqnum(theSequenceNumber);

    // Serialize BroadcastMessage
    theMessage.SerializeToString(&theMessageBuffer);

#ifdef MYDEBUG
    std::cout << "Sending request " << theSequenceNumber << '\n';
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Backend
void Engine::sendDiscoveryReply(std::string& theMessageBuffer, int theSequenceNumber)
{
  if (Spine::Reactor::isShuttingDown())
    return;

  try
  {
    static unsigned corenum = boost::thread::hardware_concurrency();

    // Create a Service message
    SmartMet::BroadcastMessage message;

    // Message header
    message.set_name(itsHostname);
    message.set_messagetype(SmartMet::BroadcastMessage::SERVICE_DISCOVERY_REPLY);
    message.set_seqnum(theSequenceNumber);

    // The Server
    auto* host = message.mutable_host();

    if (host == nullptr)
    {
      std::cerr << "Broadcast failed to acquire host information\n";
      return;
    }

    // Get the system load average
    double currentLoad = 0;
    getloadavg(&currentLoad, 1);

    // Report the per-core load average
    currentLoad /= corenum;

    host->set_ip(itsHttpAddress);
    host->set_port(boost::numeric_cast<int32_t>(itsHttpPort));
    host->set_comment(itsComment);
    host->set_load(boost::numeric_cast<float>(currentLoad));
    host->set_throttle(boost::numeric_cast<int32_t>(itsThrottleLimit));

    // The Services
    SmartMet::BroadcastMessage::Service* theService = nullptr;

    // Better not call the reactor if shutdown is in progress
    if (Spine::Reactor::isShuttingDown())
      return;

    auto theHandlers = itsReactor->getURIMap();

    for (const auto& handler : theHandlers)
    {
      theService = message.add_services();
      if (theService == nullptr)
      {
        std::cerr << "Broadcast found a null service\n";
      }
      else
      {
        theService->set_uri(handler.first);
        theService->set_lastupdate(0);
        theService->set_allowcache(false);
        theService->set_is_prefix(itsReactor->isURIPrefix(handler.first));
      }
    }

    message.SerializeToString(&theMessageBuffer);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Backend
void Engine::processRequest(SmartMet::BroadcastMessage& theMessage, std::string& theResponseBuffer)
{
  if (Spine::Reactor::isShuttingDown())
    return;

  try
  {
    switch (theMessage.messagetype())
    {
      case BroadcastMessage::SERVICE_DISCOVERY_REQUEST:
        sendDiscoveryReply(theResponseBuffer, theMessage.seqnum());
        break;
      case BroadcastMessage::SERVICE_DISCOVERY_REPLY:
      case BroadcastMessage::SERVICE_DISCOVERY_BEACON:
        break;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Frontend
void Engine::processReply(SmartMet::BroadcastMessage& theMessage)
{
  if (Spine::Reactor::isShuttingDown())
    return;

  try
  {
    // Check that message type is correct
    if (theMessage.messagetype() != SmartMet::BroadcastMessage::SERVICE_DISCOVERY_REPLY)
    {
      // Packet was not a correct one for this Frontend
      return;
    }

    // Check that the required messages are present
    if (!theMessage.has_host() || theMessage.services_size() == 0)
    {
      // Packet didn't contain the required SERVICE_DISCOVERY_REPLY fields
      return;
    }

    // Create a new backend
    const auto& host = theMessage.host();
    BackendServerPtr theServer(
        new BackendServer(theMessage.name(),
                          host.ip(),
                          host.port(),
                          host.comment(),
                          host.load(),
                          boost::numeric_cast<unsigned int>(host.throttle())));
#ifdef MYDEBUG
    std::cout << "Processing reply " << theMessage.seqnum() << " from " << theMessage.name()
              << '\n';
#endif

    for (int i = 0; i < theMessage.services_size(); i++)
    {
      const auto& service = theMessage.services(i);
      const bool is_prefix = service.has_is_prefix() && service.is_prefix();
      BackendServicePtr theService = std::make_shared<BackendService>(theServer,
                                                                      service.uri(),
                                                                      service.lastupdate(),
                                                                      service.allowcache(),
                                                                      theMessage.seqnum(),
                                                                      is_prefix);

      // Add this BackendService to the list of Services
      itsServices.addService(theService,
                             service.uri(),
                             host.load(),
                             boost::numeric_cast<unsigned int>(host.throttle()));

      // Generate a unique URI to allow using only this particular host.
      std::string itsDirectURI;
      itsDirectURI = "/";
      itsDirectURI += theMessage.name();
      itsDirectURI += service.uri();

      // Add per-host service URI
      itsServices.addService(theService,
                             itsDirectURI,
                             host.load(),
                             boost::numeric_cast<unsigned int>(host.throttle()));
    }

    // We have received a valid response, increment the counter
    ++itsReceivedResponses;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Sputnik
}  // namespace Engine
}  // namespace SmartMet
