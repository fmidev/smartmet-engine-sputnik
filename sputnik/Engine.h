// ======================================================================
/*!
 * \brief Class SmartMet::Broadcast::Engine
 */
// ======================================================================

#pragma once

#include "Services.h"
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <spine/Reactor.h>
#include <spine/SmartMetEngine.h>
#include <spine/Thread.h>
#include <iostream>
#include <memory>
#include <string>

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
 private:
  /** \brief Constructor
   *
   * Constructs the engine. Sets up the broadcasting endpoint.
   */

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

  unsigned int itsFrontendSequence = 0;  ///< The current frontend sequence.

  unsigned int itsReceivedResponses = 0;  ///< Number of received responses from the last heartbeat

  unsigned int itsSkippedCycles = 0;  ///< Number of hearbeat cycles that have gone unanswered

  std::vector<std::string> itsBackendUdpListeners;  ///< List of backend UDP listeners
                                                    ///(ipAddress1:udpPort1, ipAddress2:udpPort2)

  std::string itsHostname = "localhost";              ///< Backend hostname
  std::string itsHttpAddress = "127.0.0.1";           ///< Backend HTTP address
  unsigned int itsHttpPort = 80;                      ///< Backend HTTP port
  std::string itsUdpListenerAddress = "127.0.0.1";    ///< Backend UDP listener address
  unsigned short itsUdpListenerPort = COMM_UDP_PORT;  ///< Backend UDP listener port
  std::string itsComment;                             ///< Backend comment
  unsigned int itsThrottleLimit = 0;  ///< Max number of unanswered connections allowed

  unsigned int itsHeartBeatInterval = 5;
  unsigned int itsHeartBeatTimeout = 2;
  unsigned int itsMaxSkippedCycles = 2;

  std::string itsForwardingMode = "random";  //< Forwarding mode
  float itsBalanceFactor = 2.0F;             // Balancing factor

  Spine::Reactor* itsReactor = nullptr;  ///< The reactor pointer for URI map retrieval

  Services itsServices;  /// Services object includes the list known services.

  boost::asio::io_service itsIoService;  ///< The IO Service for UDP handling

  boost::asio::ip::udp::socket itsSocket;  ///< The UDP socket for broacasting/replying

  boost::asio::ip::udp::endpoint itsRemoteEnd;  ///< The remote endpoint to which replies are sent
  boost::asio::ip::udp::endpoint itsBackendSocket;

  boost::asio::ip::udp::endpoint itsBroadcastEnd;  ///< The broadcast endpoint to which broadcasts
  /// are sent.

  boost::array<char, 8192> itsReceiveBuffer;  ///< Buffer for incoming UDP messages

  std::shared_ptr<boost::thread> itsAsyncThread;  ///< Async thread for the IO Service.

  boost::asio::deadline_timer itsResponseDeadlineTimer;  ///< Timer to handle the deadline of
                                                         /// backend responses

  // Sputnik may be paused for a while via an external request

  mutable Spine::MutexType itsPauseMutex;
  mutable bool itsPaused{false};
  mutable boost::optional<Fmi::DateTime> itsPauseDeadLine{};

 protected:
  void init() override;
  void shutdown() override;

 public:
  Engine() = delete;

  Engine(const Engine& other) = delete;
  Engine& operator=(const Engine& other) = delete;
  Engine(Engine&& other) = delete;
  Engine& operator=(Engine&& other) = delete;

  /** \brief Destructor
   *
   */

  ~Engine() override;

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
  void launch(BroadcastMode theMode, Spine::Reactor* theReactor);

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
  std::shared_ptr<SmartMet::Spine::Table> backends(const std::string& service = "") const;
  Services::BackendList getBackendList(const std::string& service = "") const;

  std::string URI() const;

  const Services& getServices() const { return itsServices; }
  Services& getServices() { return itsServices; }

  bool isPaused() const;
  void setPause();
  void setPauseUntil(const Fmi::DateTime& theDeadLine);
  void setContinue();
};

}  // namespace Sputnik
}  // namespace Engine
}  // namespace SmartMet
