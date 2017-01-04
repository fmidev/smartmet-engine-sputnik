#include "Engine.h"
#include "Services.h"
#include "BroadcastMessage.pb.h"

#include <spine/Reactor.h>
#include <spine/Exception.h>

#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

#ifndef SPUTNIK_HEARTBEAT_INTERVAL
#define SPUTNIK_HEARTBEAT_INTERVAL 5
#endif

#ifndef SPUTNIK_HEARTBEAT_TIMEOUT
#define SPUTNIK_HEARTBEAT_TIMEOUT 2
#endif

#ifndef SPUTNIK_MAX_SKIPPED_CYCLES
#define SPUTNIK_MAX_SKIPPED_CYCLES 2
#endif

namespace SmartMet
{
namespace Engine
{
namespace Sputnik
{
Engine::Engine(const char* theConfig)
    : itsServices(),
      itsMode(Unknown),
      itsFrontendSequence(0),
      itsReceivedResponses(0),
      itsSkippedCycles(0),
      itsHostname("localhost"),
      itsHttpAddress("127.0.0.1"),
      itsHttpPort(80),
      itsUdpListenerAddress("127.0.0.1"),
      itsUdpListenerPort(COMM_UDP_PORT),
      itsComment(""),
      itsThrottleLimit(),
      itsReactor(NULL),
      itsIoService(),
      itsSocket(itsIoService, boost::asio::ip::udp::v4()),
      itsRemoteEnd(),
      itsBroadcastEnd(),
      itsReceiveBuffer(),
      itsAsyncThread(),
      itsResponseDeadlineTimer(itsIoService)
{
  try
  {
    // Read and parse configuration

    SmartMet::Spine::ConfigBase conf(theConfig);

    // Frontend parameters

    conf.get_config_array("backendUdpListeners", itsBackendUdpListeners);

    // Backend parameters

    itsHostname = conf.get_optional_config_param<std::string>("hostname", "localhost");
    itsHttpAddress = conf.get_optional_config_param<std::string>("httpAddress", "127.0.0.1");
    itsHttpPort = conf.get_optional_config_param<int>("httpPort", 80);
    itsUdpListenerAddress =
        conf.get_optional_config_param<std::string>("udpListenerAddress", "127.0.0.1");
    itsUdpListenerPort = conf.get_optional_config_param<int>("udpListenerPort", COMM_UDP_PORT);
    itsComment = conf.get_optional_config_param<std::string>(
        "comment", "No comments associated with this server");

    // Setup the correct values for broadcast

    itsBackendSocket.address(boost::asio::ip::address_v4::from_string(itsUdpListenerAddress));
    itsBackendSocket.port(itsUdpListenerPort);

    // Reuse address for easier restart
    boost::asio::ip::udp::socket::reuse_address reuse(true);
    itsSocket.set_option(reuse);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Constructor failed!", NULL);
  }
}

// No-op, since construction is trivial
void Engine::init()
{
}
// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the engine
 */
// ----------------------------------------------------------------------

void Engine::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (sputnik)\n";
    itsIoService.stop();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Constructor failed!", NULL);
  }
}

Engine::~Engine()
{
  // Debug message
  std::cout << "\t + Broadcast Service Discovery Engine shutting down" << std::endl;
}

// Start actively listen to UDP broadcasts on the network
void Engine::backendMode()
{
  try
  {
    unsigned int throttle;

    if (config.lookupValue("throttle", throttle))
    {
      itsThrottleLimit = throttle;
    }
    else
    {
      itsThrottleLimit = 0;
    }

    try
    {
      itsSocket.bind(itsBackendSocket);
    }
    catch (boost::system::system_error& err)
    {
      throw SmartMet::Spine::Exception(
          BCP, "Error: Broadcast Backend can't bind datagram socket: " + std::string(err.what()));
    }

    // Start the async loop for incoming UDP/IP requests
    startListening();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::frontendMode()
{
  try
  {
    // Setup backend forwarding mode, only relevant to frontend configuration
    const char* randOption = config.exists("forwarding") ? config.lookup("forwarding")
                                                         : static_cast<const char*>("random");

    std::string randMode(randOption);

    // Get the balancing coefficient if any
    float bcoeff = config.exists("balance_factor") ? config.lookup("balance_factor") : 2.0f;

    std::cout << "Using Broadcast balancing configuration: balance factor => " << bcoeff
              << ", mode => " << randMode << std::endl;

    itsServices.setForwarding(randMode, bcoeff);

    boost::system::error_code e;

    // Set is as broadcast
    itsSocket.set_option(boost::asio::socket_base::broadcast(true), e);
    if (e)
    {
      // Log error and exit
      throw SmartMet::Spine::Exception(
          BCP,
          "Error: Broadcast Frontend was unable to set UDP/IP broadcast option: " + e.message());
    }

    itsSocket.bind(boost::asio::ip::udp::endpoint(), e);
    // Bind the socket to local address
    if (e)
    {
      // Log error and exit
      throw SmartMet::Spine::Exception(BCP,
                                       "Error: Frontend can't bind local address: " + e.message());
    }

    startServiceDiscovery();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::handleDeadlineTimer(const boost::system::error_code& err)
{
  try
  {
    if (err == boost::asio::error::operation_aborted)
    {
      // The timer was manually cancelled, should not happen;
      return;
    }

    // Cancel all and any pending async operations
    itsSocket.cancel();

    // Clean the service list of old entires only if we have received at least one response
    if (itsReceivedResponses > 0)
    {
      itsServices.latestSequence(boost::numeric_cast<int>(itsFrontendSequence));
      // Reset the skipped cycle counter, skipped cycles must be sequential
      itsSkippedCycles = 0;
    }
    else
    {
      // No responses received, see if we have exceeded maximum skipped cycles

      ++itsSkippedCycles;

      if (itsSkippedCycles > SPUTNIK_MAX_SKIPPED_CYCLES)
      {
        // Maximum skipped cycles exceeded, clean the service map
        itsServices.latestSequence(boost::numeric_cast<int>(itsFrontendSequence));

        // Reset the skipped cycle counter
        itsSkippedCycles = 0;
      }
    }

    // Reset the response counter
    itsReceivedResponses = 0;

    // Sleep until the next heart beat
    boost::this_thread::sleep(boost::posix_time::seconds(SPUTNIK_HEARTBEAT_INTERVAL));

    if (itsShutdownRequested)
      return;

    startServiceDiscovery();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::handleFrontendRead(const boost::system::error_code& e, std::size_t bytes_transferred)
{
  try
  {
    if (e == boost::asio::error::operation_aborted)
    {
      // The operation was aborted by the deadline timer;
      return;
    }

    if (!e)
    {
#ifdef MYDEBUG
      std::cout << "Broadcast received data from " << itsRemoteEnd.address().to_string() << ":"
                << itsRemoteEnd.port() << std::endl;
#endif

      // Construct a std::string from the buffer contents
      std::string receiveBuffer(itsReceiveBuffer.begin(), bytes_transferred);
      SmartMet::BroadcastMessage message;
      message.ParseFromString(receiveBuffer);

      // Call the Reply processing function
      processReply(message);
    }

    // Start a new receive event
    itsSocket.async_receive_from(boost::asio::buffer(itsReceiveBuffer),
                                 itsRemoteEnd,
                                 boost::bind(&Engine::handleFrontendRead,
                                             this,
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::startServiceDiscovery()
{
  try
  {
    itsFrontendSequence = (itsFrontendSequence + 1) % 65535;

    std::string theRequestBuffer;
    sendDiscoveryRequest(theRequestBuffer, boost::numeric_cast<int>(itsFrontendSequence));

    // Send the discovery requests
    try
    {
      for (auto it = itsBackendUdpListeners.begin(); it != itsBackendUdpListeners.end(); ++it)
      {
        std::string st = *it;

        auto idx = st.find(":");
        if (idx != std::string::npos)
        {
          std::string addr = st.substr(0, idx);
          std::string port = st.substr(idx + 1);

          // std::cout << addr << ":" << port << "\n";

          boost::asio::ip::udp::endpoint dest;
          dest.address(boost::asio::ip::address_v4::from_string(addr));
          dest.port(std::stoi(port));
          itsSocket.send_to(boost::asio::buffer(theRequestBuffer), dest);
        }
      }
    }
    catch (boost::system::system_error& err)
    {
      std::cerr << "Error: Broadcast failed to send discovery request: " << err.what() << std::endl;
    }

    itsSocket.async_receive_from(boost::asio::buffer(itsReceiveBuffer),
                                 itsRemoteEnd,
                                 boost::bind(&Engine::handleFrontendRead,
                                             this,
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));

    // Reset the response deadline timer
    itsResponseDeadlineTimer.expires_from_now(
        boost::posix_time::seconds(SPUTNIK_HEARTBEAT_TIMEOUT));
    itsResponseDeadlineTimer.async_wait(
        boost::bind(&Engine::handleDeadlineTimer, this, boost::asio::placeholders::error));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::startListening()
{
  try
  {
    itsSocket.async_receive_from(boost::asio::buffer(itsReceiveBuffer),
                                 itsRemoteEnd,
                                 boost::bind(&Engine::handleBackendRead,
                                             this,
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::handleBackendRead(const boost::system::error_code& e, std::size_t bytes_transferred)
{
  try
  {
    if (itsShutdownRequested)
      return;

    if (!e)
    {
// No error, proceed
#ifdef MYDEBUG
      std::cout << "Broadcast received data from " << itsRemoteEnd.address().to_string() << ":"
                << itsRemoteEnd.port() << std::endl;
#endif

      // Construct a std::string from the buffer contents
      std::string receiveBuffer(itsReceiveBuffer.begin(), bytes_transferred);
      std::string responseBuffer;
      SmartMet::BroadcastMessage message;
      message.ParseFromString(receiveBuffer);

      // Call the Reply processing function
      processRequest(message, responseBuffer);

      // Send response back to sender if responseBuffer is not empty
      if (!responseBuffer.empty())
      {
        // Send response back to sender
        try
        {
          itsSocket.send_to(boost::asio::buffer(responseBuffer), itsRemoteEnd);
        }
        catch (boost::system::system_error& err)
        {
          std::cerr << "Error: Broadcast failed to send response: " << err.what() << std::endl;
        }
      }
    }
    else
    {
      // Some error occurred in receive
      std::cerr << "Broadcast UDP receive encountered an error: " << e.message() << std::endl;
    }
    // Go back to listen the socket
    startListening();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::launch(BroadcastMode theMode, SmartMet::Spine::Reactor* theReactor)
{
  try
  {
    // Save reactor instance permanently for callbacks
    itsReactor = theReactor;

    // Save working mode and callback function permanently
    itsMode = theMode;

    // Launch the appropriate method

    switch (itsMode)
    {
      case Backend:
        backendMode();
        break;
      case Frontend:
        frontendMode();
        // Add hook for response based backend hearbeat
        if (!theReactor->addBackendConnectionFinishedHook(
                "sputnik_backend_hearbeat_hook",
                boost::bind(&Engine::setBackendAlive, this, _1, _2, _3)))
        {
          throw SmartMet::Spine::Exception(BCP, "Broadcast failed to add backend heartbeat hook");
        }
        break;
      case Unknown:
        break;
    }

    // Fire the thread to process async handlers
    itsAsyncThread.reset(
        new boost::thread(boost::bind(&boost::asio::io_service::run, &itsIoService)));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate status report
 */
// ----------------------------------------------------------------------

void Engine::status(std::ostream& out) const
{
  try
  {
    // Information about Broadcast
    out << "<h2>Broadcast Cluster Information</h2>" << std::endl << std::endl;

    out << "<div>This server is ";
    switch (itsMode)
    {
      case Backend:
        out << "a BACKEND";
        break;
      case Frontend:
        out << "a FRONTEND";
        break;
      case Unknown:
        out << " in a UNINITIALIZED STATE";
        break;
    }
    out << "</div>";

    out << "<ul>" << std::endl
        << "<li>Host: " << itsHostname << "</li>" << std::endl
        << "<li>Comment: " << itsComment << "</li>" << std::endl
        << "<li>HTTP Interface: " << itsHttpAddress << ":" << itsHttpPort << "</li>" << std::endl
        << "<li>Throttle Limit: " << itsThrottleLimit << "</li>" << std::endl
        << "<li>Broadcast Interface: " << itsUdpListenerAddress << ":" << itsUdpListenerPort
        << "</li>" << std::endl
        << "</ul>" << std::endl;

    if (itsMode == Backend)
    {
      // List backend services
      out << "<h3>Services currently provided by this backend server</h3>" << std::endl;

      // Read the URI list

      if (!itsReactor)
        out << "<div>List not available, Reactor not initialized</div>" << std::endl;
      else
      {
        const auto theHandlers = itsReactor->getURIMap();

        out << "<ol>" << std::endl;
        for (auto iter = theHandlers.begin(); iter != theHandlers.end(); ++iter)
        {
          out << "<li>" << iter->first << "</li>" << std::endl;
        }
        out << "</ol>" << std::endl;
      }
    }
    else
    {
      // List frontend services
      out << "<h3>Services known by the frontend server</h3>" << std::endl;
      itsServices.status(out);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::setBackendAlive(const std::string& theHostName,
                             int thePort,
                             SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus)
{
  try
  {
#ifdef MYDEBUG
    std::cout << "Backend connection to " << theHostName << " finished with status " << theStatus
              << std::endl;
#endif
    if (theStatus == SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus::EXIT_OK)
    {
      if (!itsServices.queryBackendAlive(theHostName, thePort))
      {
        std::cout << boost::posix_time::second_clock::local_time() << "Backend " << theHostName
                  << ":" << thePort << " set alive" << std::endl;
      }

      itsServices.setBackendAlive(theHostName, thePort);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief List backends with given service or all services
 */
// ----------------------------------------------------------------------

boost::shared_ptr<SmartMet::Spine::Table> Engine::backends(const std::string& service) const
{
  try
  {
    return itsServices.backends(service);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return backend list for given service or services
 */
// ----------------------------------------------------------------------

Services::BackendList Engine::getBackendList(const std::string& service) const
{
  try
  {
    return itsServices.getBackendList(service);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the URI with the http:// part
 */
// ----------------------------------------------------------------------

std::string Engine::URI() const
{
  try
  {
    std::ostringstream out;
    out << "http://" << itsHttpAddress << ':' << itsHttpPort;
    return out.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Sputnik
}  // namespace Engine
}  // namespace SmartMet

// C language function to construct/destruct this dynamic C++ module
//  *** DO NOT TOUCH UNLESS YOU KNOW EXACTLY WAS YOU ARE DOING ***
extern "C" void* engine_class_creator(const char* configfile, void* /* user_data */)
{
  // Create a new instance of the required class
  return new SmartMet::Engine::Sputnik::Engine(configfile);
}

extern "C" const char* engine_name()
{
  // Return the name of this engine's human readable name
  return "Sputnik";
}