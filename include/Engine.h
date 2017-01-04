// ======================================================================
/*!
 * \brief Class SmartMet::Broadcast::Engine
 */
// ======================================================================

#pragma once

#include "Services.h"

#include <boost/function.hpp>
#include <boost/asio.hpp>

#include <libconfig.h++>

#include <spine/Reactor.h>
#include <spine/SmartMetEngine.h>

#include <string>
#include <iostream>

namespace SmartMet
{
class BroadcastMessage;
namespace Spine
{
class Table;
}
namespace Engine
{
namespace Sputnik
{
/** \brief Enum to represent the Broadcast operating mode
 *
 */
enum BroadcastMode
{
  Backend,
  Frontend,
  Unknown
};

const unsigned short COMM_UDP_PORT = 31337;  /// UDP Port that all the communication engines use

/** \brief The Broadcast Engine declaration
 *
 */
class Engine : public SmartMet::Spine::SmartMetEngine
{
 public:
  Services itsServices;  /// Services object includes the list known services.

 private:
  /** \brief Constructor
   *
   * Constructs the engine. Sets up the broadcasting endpoint.
   */
  Engine();

  /** \brief Sets up Broadcast for backend mode operation
   *
   * Binds the socket to listen for broadcasts and fires the broadcast reply loop.
   */
  void backendMode();

  /** \brief Sets up Broadcast for frontend mode operation
   *
   * Reads configuration for forwarding mode and balance factor, sets up
   * the socket to send broadcasts and starts the service discovery loop.
   */
  void frontendMode();

  /** \brief Method to stop listening for backend responses when in service discovery loop.
   *
   * This deadline timer implements the frontend listening for backend responses. It is started
   * when discovery starts and calling this function marks the end of the listening phase.
   */
  void handleDeadlineTimer(const boost::system::error_code& err);

  BroadcastMode itsMode;  ///< Mode of this Broadcast engine

  unsigned int itsFrontendSequence;  ///< The current frontend sequence.

  unsigned int itsReceivedResponses;  ///< Number of received responses from the last heartbeat

  unsigned int itsSkippedCycles;  ///< Number of hearbeat cycles that have gone unanswered

  libconfig::Config config;  ///< Configuration object

  std::vector<std::string> itsBackendUdpListeners;  ///< List of backend UDP listeners
                                                    ///(ipAddress1:udpPort1, ipAddress2:udpPort2)

  std::string itsHostname;            ///< Backend hostname
  std::string itsHttpAddress;         ///< Backend HTTP address
  unsigned int itsHttpPort;           ///< Backend HTTP port
  std::string itsUdpListenerAddress;  ///< Backend UDP listener address
  unsigned short itsUdpListenerPort;  ///< Backend UDP listener port
  std::string itsComment;             ///< Backend comment
  unsigned int itsThrottleLimit;      ///< Max number of unanswered connections allowed

  SmartMet::Spine::Reactor* itsReactor;  ///< The reactor pointer for URI map retrieval

  boost::asio::io_service itsIoService;  ///< The IO Service for UDP handling

  boost::asio::ip::udp::socket itsSocket;  ///< The UDP socket for broacasting/replying

  boost::asio::ip::udp::endpoint itsRemoteEnd;  ///< The remote endpoint to which replies are sent
  boost::asio::ip::udp::endpoint itsBackendSocket;

  boost::asio::ip::udp::endpoint itsBroadcastEnd;  ///< The broadcast endpoint to which broadcasts
  /// are sent.

  boost::array<char, 8192> itsReceiveBuffer;  ///< Buffer for incoming UDP messages

  boost::shared_ptr<boost::thread> itsAsyncThread;  ///< Async thread for the IO Service.

  boost::asio::deadline_timer itsResponseDeadlineTimer;  ///< Timer to handle the deadline of
                                                         /// backend responses

 protected:
  virtual void init();
  void shutdown();

 public:
  /** \brief Destructor
   *
   */

  ~Engine();

  /** \brief Constructor
   *
   * Constructs the engine with the given configuration.
   */

  Engine(const char* theConfig);

  /** \brief Starts the engine in given mode.
   *
   * Starts the engine in the given mode (frontend/backend).
   * @param theMode The mode of this Broadcast instance.
   * @param theReactor Pointer to the Reactor.
   */
  void launch(BroadcastMode theMode, SmartMet::Spine::Reactor* theReactor);

  /** \brief Method to start listening for frontend broadcasts
   *
   */
  void startListening();

  /** \brief Method to handle received frontend broadcasts
   *
   * This method is called when backend receives a UDP message.
   */
  void handleBackendRead(const boost::system::error_code& e, std::size_t bytes_transferred);

  /** \brief Method to start the service discovery loop
   *
   */
  void startServiceDiscovery();

  /** \brief Method to handle received backend responses
   *
   * This method is called when frontend receives a UDP message.
   */
  void handleFrontendRead(const boost::system::error_code& e, std::size_t bytes_transferred);

  /** \brief Makes and serializes a discovery request message (frontend behaviour)
   *
   * @param theMessageBuffer Buffer into which the message is serialized.
   * @param theSequenceNumber The current sequence number of the frontend.
   */
  void sendDiscoveryRequest(std::string& theMessageBuffer, int theSequenceNumber);

  /** \brief Makes and serializes a discovery response message (backend behaviour)
   *
   * @param theMessageBuffer Buffer into which the message is serialized.
   * @param theSequenceNumber The current sequence number received from the frontend.
   */
  void sendDiscoveryReply(std::string& theMessageBuffer, int theSequenceNumber);

  /** \brief Processes received discovery request (backend behaviour)
   *
   * Ensures received message is of correct type and sends response.
   */
  void processRequest(BroadcastMessage& theMessage, std::string& theResponseBuffer);

  /** \brief Processes received discovery response (frontend behaviour)
   *
   * Ensures received message is of correct type and updates the frontend Services.
   */
  void processReply(BroadcastMessage& theMessage);

  /** \brief Set a backend alive (reset the throttle counter)
   *
   * If this is not called before throttle counter reaches the maximum
   * the backend is marked as unresponsive
   */

  void setBackendAlive(const std::string& theHostName,
                       int thePort,
                       SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus);

  void status(std::ostream& out) const;
  boost::shared_ptr<SmartMet::Spine::Table> backends(const std::string& service = "") const;
  Services::BackendList getBackendList(const std::string& service = "") const;

  std::string URI() const;
};

}  // namespace Sputnik
}  // namespace Engine
}  // namespace SmartMet