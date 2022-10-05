#pragma once

/*! \brief Base class for backend forwarding logic used in Broadcast
 *
 * To add a new backend forwarder, derive from BackendForwarder and implement
 * the 'redistribute' method.
 *
 */

#include "BackendInfo.h"
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/taus88.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <spine/Reactor.h>
#include <spine/Thread.h>
#include <string>
#include <vector>

namespace SmartMet
{
/*! \brief Base class for forwarder implementations.
 *
 * This defines the interface which Broadcast uses
 * to access the Forwarder.
 */

class BackendForwarder
{
 public:
  BackendForwarder() = delete;
  BackendForwarder(const BackendForwarder& other) = delete;
  BackendForwarder& operator=(const BackendForwarder& other) = delete;

  /*! \brief Constructor
   *
   * Construct the Forwarder.
   * @param balancingCoefficient The balancing coefficient
   * used in probability calculations.
   */

  BackendForwarder(float balancingCoefficient);

  /*! \brief Get the backend index from the Forwarder.
   *
   * Get a backend (using its index in the Broadcast service list)
   * using the underlying balancing algorithm.
   */

  std::size_t getBackend(Spine::Reactor& theReactor);

  /*! \brief Set the internal backend list explicitly
   *
   * Currently unused.
   */

  void setBackends(const std::vector<BackendInfo>& backends, Spine::Reactor& theReactor);

  /*! \brief Add backend to the underlying backend list
   *
   * This is called when Broadcast receives information
   * of a new backend.
   */

  void addBackend(const std::string& hostName, int port, float load, Spine::Reactor& theReactor);

  /*! \brief Remove backend from the underlying backend list
   *
   * This is called when Broadcast remove backend from its
   * backend list.
   */

  void removeBackend(const std::string& hostName, int port, Spine::Reactor& theReactor);

  /*! \brief Destructor
   *
   */

  virtual ~BackendForwarder();

 protected:
  /*! \brief Redistributes the internal forwarding probabilities
   *
   * This is called whenever the interal backend list changes.
   * This method must be overrided in the actual
   * Forwader implementations.
   */

  virtual void redistribute(Spine::Reactor& theReactor) = 0;

  /*! \brief Redistributes the internal forwarding probabilities
   *
   * This is called to update internal coefficients whenever
   * a backend is needed. By default this does nothing, as only
   * some load balancers update their state every time.
   */

  virtual void rebalance(Spine::Reactor& theReactor){};

  boost::taus88 itsGenerator;  /// The randomizer object for RNG.

  std::vector<BackendInfo> itsBackendInfos;  /// The internal backend list.

  boost::random::discrete_distribution<> itsDistribution;  /// The probability distribution for RNG.

  SmartMet::Spine::MutexType itsMutex;  /// The mutex to ensure RNG thread safety.

  float itsBalancingCoefficient;  /// The balancing coefficient for distribution generation.
};

using BackendForwarderPtr = boost::shared_ptr<BackendForwarder>;

}  // namespace SmartMet
