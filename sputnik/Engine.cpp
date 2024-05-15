#include "Engine.h"
#include "BroadcastMessage.pb.h"
#include "Services.h"
#include <macgyver/DateTime.h>
#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <spine/Reactor.h>
#include <iostream>
#include <memory>

namespace SmartMet
{
namespace Engine
{
namespace Sputnik
{
Engine::Engine(const char* theConfig)
    : itsMode(Unknown),
      itsSocket(itsIoService, boost::asio::ip::udp::v4()),
      itsReceiveBuffer(),
      itsResponseDeadlineTimer(itsIoService)
{
  try
  {
    // Read and parse configuration

    SmartMet::Spine::ConfigBase conf(theConfig);

    // Frontend parameters

    conf.get_config_array("backendUdpListeners", itsBackendUdpListeners);

    itsForwardingMode = conf.get_optional_config_param<std::string>("forwarding", "random");
    itsBalanceFactor = conf.get_optional_config_param<float>("balance_factor", 2.0F);

    itsHeartBeatInterval = conf.get_optional_config_param<int>("heartbeat.interval", 5);
    itsHeartBeatTimeout = conf.get_optional_config_param<int>("heartbeat.timeout", 2);
    itsMaxSkippedCycles = conf.get_optional_config_param<int>("heartbeat.max_skipped_cycles", 2);

    // Backend parameters

    itsHostname = conf.get_optional_config_param<std::string>("hostname", "localhost");
    itsHttpAddress = conf.get_optional_config_param<std::string>("httpAddress", "127.0.0.1");
    itsHttpPort = conf.get_optional_config_param<int>("httpPort", 80);
    itsUdpListenerAddress =
        conf.get_optional_config_param<std::string>("udpListenerAddress", "127.0.0.1");
    itsUdpListenerPort = conf.get_optional_config_param<int>("udpListenerPort", COMM_UDP_PORT);
    itsComment = conf.get_optional_config_param<std::string>(
        "comment", "No comments associated with this server");

    itsThrottleLimit = conf.get_optional_config_param<int>("throttle", 0);

    // Setup the correct values for broadcast

    itsBackendSocket.address(boost::asio::ip::address_v4::from_string(itsUdpListenerAddress));
    itsBackendSocket.port(itsUdpListenerPort);

    // Reuse address for easier restart
    boost::asio::ip::udp::socket::reuse_address reuse(true);
    itsSocket.set_option(reuse);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}

// No-op, since construction is trivial
void Engine::init() {}
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
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
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
    try
    {
      itsSocket.bind(itsBackendSocket);
    }
    catch (const boost::system::system_error& err)
    {
      throw Fmi::Exception(
          BCP, "Error: Broadcast Backend can't bind datagram socket: " + std::string(err.what()));
    }

    // Start the async loop for incoming UDP/IP requests
    startListening();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::frontendMode()
{
  try
  {
    std::cout << "Using Broadcast balancing configuration: balance factor => " << itsBalanceFactor
              << ", mode => " << itsForwardingMode << std::endl;

    itsServices.setForwarding(itsForwardingMode, itsBalanceFactor);

    boost::system::error_code e;

    // Set is as broadcast
    itsSocket.set_option(boost::asio::socket_base::broadcast(true), e);
    if (e.value() != boost::system::errc::success)
    {
      // Log error and exit
      throw Fmi::Exception(
          BCP,
          "Error: Broadcast Frontend was unable to set UDP/IP broadcast option: " + e.message());
    }

    itsSocket.bind(boost::asio::ip::udp::endpoint(), e);
    // Bind the socket to local address
    if (e.value() != boost::system::errc::success)
    {
      // Log error and exit
      throw Fmi::Exception(BCP, "Error: Frontend can't bind local address: " + e.message());
    }

    startServiceDiscovery();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

      if (itsSkippedCycles > itsMaxSkippedCycles)
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
    boost::this_thread::sleep_for(boost::chrono::seconds(itsHeartBeatInterval));

    if (Spine::Reactor::isShuttingDown())
      return;

    startServiceDiscovery();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    itsSocket.async_receive_from(
        boost::asio::buffer(itsReceiveBuffer),
        itsRemoteEnd,
        [this](const boost::system::error_code& err, std::size_t bytes_transferred)
        { this->handleFrontendRead(err, bytes_transferred); });
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      for (const auto& st : itsBackendUdpListeners)
      {
        auto idx = st.find(':');
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
    catch (const boost::system::system_error& err)
    {
      std::cerr << "Error: Broadcast failed to send discovery request: " << err.what() << std::endl;
    }

    itsSocket.async_receive_from(
        boost::asio::buffer(itsReceiveBuffer),
        itsRemoteEnd,
        [this](const boost::system::error_code& err, std::size_t bytes_transferred)
        { this->handleFrontendRead(err, bytes_transferred); });

    // Reset the response deadline timer
    itsResponseDeadlineTimer.expires_from_now(std::chrono::seconds(itsHeartBeatTimeout));
    itsResponseDeadlineTimer.async_wait([this](const boost::system::error_code& err)
                                        { this->handleDeadlineTimer(err); });
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::startListening()
{
  try
  {
    itsSocket.async_receive_from(
        boost::asio::buffer(itsReceiveBuffer),
        itsRemoteEnd,
        [this](const boost::system::error_code& err, std::size_t bytes_transferred)
        { this->handleBackendRead(err, bytes_transferred); });
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::handleBackendRead(const boost::system::error_code& e, std::size_t bytes_transferred)
{
  try
  {
    if (Spine::Reactor::isShuttingDown())
      return;

    if (e)
    {
      // Some error occurred in receive
      std::cerr << "Broadcast UDP receive encountered an error: " << e.message() << std::endl;
    }
    else if (isPaused())
    {
      // std::cout << Spine::log_time_str() << " Sputnik paused, not responding" << std::endl;
    }
    else if (itsReactor->isLoadHigh())
    {
      std::cout << Spine::log_time_str() << " Sputnik will not respond to frontend, high load"
                << std::endl;
    }
    else
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

      if (Spine::Reactor::isShuttingDown())
        return;

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
        catch (const boost::system::system_error& err)
        {
          std::cerr << "Error: Broadcast failed to send response: " << err.what() << std::endl;
        }
      }
    }

    // Go back to listen the socket
    startListening();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::launch(BroadcastMode theMode, SmartMet::Spine::Reactor* theReactor)
{
  using namespace boost::placeholders;

  try
  {
    // Save reactor instance permanently for callbacks
    itsReactor = theReactor;

    itsServices.setReactor(*theReactor);

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
                [this](const std::string& theHostName,
                       int thePort,
                       SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus)
                { setBackendAlive(theHostName, thePort, theStatus); }))
        {
          throw Fmi::Exception(BCP, "Broadcast failed to add backend heartbeat hook");
        }
        break;
      case Unknown:
        break;
    }

    // Fire the thread to process async handlers
    itsAsyncThread = std::make_shared<boost::thread>([this]() { this->itsIoService.run(); });
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

      if (itsReactor == nullptr)
        out << "<div>List not available, Reactor not initialized</div>" << std::endl;
      else
      {
        const auto theHandlers = itsReactor->getURIMap();

        out << "<ol>" << std::endl;
        for (const auto& handler : theHandlers)
        {
          out << "<li>" << handler.first << "</li>" << std::endl;
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::setBackendAlive(const std::string& theHostName,
                             int thePort,
                             SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus)
{
  try
  {
#ifdef MYDEBUG
    std::cout << "Backend connection to " << theHostName << ":" << thePort
              << " finished with status " << static_cast<int>(theStatus) << std::endl;
#endif
    if (theStatus == SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus::EXIT_OK)
    {
      if (!itsServices.queryBackendAlive(theHostName, thePort))
      {
        std::cout << Fmi::SecondClock::local_time() << "Backend " << theHostName
                  << ":" << thePort << " set alive" << std::endl;
      }

      itsServices.setBackendAlive(theHostName, thePort);
    }
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

std::shared_ptr<SmartMet::Spine::Table> Engine::backends(const std::string& service) const
{
  try
  {
    return itsServices.backends(service);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set an indefinite pause (unless further instructed)
 */
// ----------------------------------------------------------------------

void Engine::setPause()
{
  std::cout << Spine::log_time_str() << " *** Sputnik paused" << std::endl;
  Spine::WriteLock lock(itsPauseMutex);
  itsPaused = true;
  itsPauseDeadLine = boost::none;
}

// ----------------------------------------------------------------------
/*!
 * \brief Pause until given deadline (unless further instructed)
 */
// ----------------------------------------------------------------------

void Engine::setPauseUntil(const Fmi::DateTime& theDeadLine)
{
  std::cout << Spine::log_time_str() << " *** Sputnik paused until "
            << Fmi::to_iso_string(theDeadLine) << std::endl;
  Spine::WriteLock lock(itsPauseMutex);
  itsPaused = true;
  itsPauseDeadLine = theDeadLine;
}

// ----------------------------------------------------------------------
/*!
 * \brief Continue running immediately (unless paused again)
 */
// ----------------------------------------------------------------------

void Engine::setContinue()
{
  std::cout << Spine::log_time_str() << " *** Sputnik instructed to continue" << std::endl;
  Spine::WriteLock lock(itsPauseMutex);
  itsPaused = false;
  itsPauseDeadLine = boost::none;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if Sputnik is paused
 */
// ----------------------------------------------------------------------

bool Engine::isPaused() const
{
  Spine::UpgradeReadLock readlock(itsPauseMutex);
  if (!itsPaused)
    return false;

  if (!itsPauseDeadLine)
    return true;

  auto now = Fmi::MicrosecClock::universal_time();

  if (now < itsPauseDeadLine)
    return true;

  // deadline expired, continue
  std::cout << Spine::log_time_str() << " *** Sputnik deadline expired, continuing" << std::endl;
  Spine::UpgradeWriteLock writelock(readlock);
  itsPaused = false;
  itsPauseDeadLine = boost::none;

  return false;
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
