#include "BroadcastMessage.pb.h"
#include "Engine.h"
#include "Services.h"

#include <spine/Exception.h>
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
    std::cout << "Sending request " << theSequenceNumber << std::endl;
#endif
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// Backend
void Engine::sendDiscoveryReply(std::string& theMessageBuffer, int theSequenceNumber)
{
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
      std::cerr << "Broadcast failed to acquire host information" << std::endl;
      return;
    }

    // Get the system load average
    double currentLoad;
    getloadavg(&currentLoad, 1);

    // Report the per-core load average
    currentLoad /= corenum;

    host->set_ip(itsHttpAddress);
    host->set_port(boost::numeric_cast<int32_t>(itsHttpPort));
    host->set_comment(itsComment);
    host->set_load(boost::numeric_cast<float>(currentLoad));
    host->set_throttle(boost::numeric_cast<int32_t>(itsThrottleLimit));

    // The Services
    SmartMet::BroadcastMessage::Service* theService;
    auto theHandlers = itsReactor->getURIMap();

    for (auto iter = theHandlers.begin(); iter != theHandlers.end(); ++iter)
    {
      theService = message.add_services();
      if (theService == nullptr)
      {
        std::cerr << "Broadcast found a null service" << std::endl;
      }
      else
      {
        theService->set_uri(iter->first);
        theService->set_lastupdate(0);
        theService->set_allowcache(false);
      }
    }

    message.SerializeToString(&theMessageBuffer);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// Backend
void Engine::processRequest(SmartMet::BroadcastMessage& theMessage, std::string& theResponseBuffer)
{
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// Frontend
void Engine::processReply(SmartMet::BroadcastMessage& theMessage)
{
  try
  {
    // Check that message type is correct
    if (theMessage.messagetype() != SmartMet::BroadcastMessage::SERVICE_DISCOVERY_REPLY)
    {
      // Packet was not a correct one for this Frontend
      return;
    }

    // Check that the required messages are present
    if (theMessage.has_host() == false || theMessage.services_size() == 0)
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
              << std::endl;
#endif

    for (int i = 0; i < theMessage.services_size(); i++)
    {
      const auto& service = theMessage.services(i);
      BackendServicePtr theService(new BackendService(theServer,
                                                      service.uri(),
                                                      service.lastupdate(),
                                                      service.allowcache(),
                                                      theMessage.seqnum()));

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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Sputnik
}  // namespace Engine
}  // namespace SmartMet
