#pragma once

/*! \brief Base class for backend forwarding logic used in Broadcast
 *
 * To add a new backend forwarder, derive from BackendForwarder and implement
 * the 'redistribute' method.
 *
 */

#include <boost/noncopyable.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/taus88.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
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

/*! \brief This struct contains the backend information needed in forwarding and backend health
 * checking.
 *
 * This information is stored within the Forwarder and Sentinel objects and is used
 * to calculate the forwarding probabilities and backend removals.
 *
 */
struct BackendInfo
{
  BackendInfo(const std::string& theHostName, int thePort, float theLoad)
      : hostName(theHostName), port(thePort), load(theLoad)
  {
  }

  std::string hostName;
  int port;
  float load;

  mutable unsigned int throttle_counter;
};

class BackendForwarder : public boost::noncopyable
{
 public:
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

  std::size_t getBackend();

  /*! \brief Set the internal backend list explicitly
   *
   * Currently unused.
   */

  void setBackends(const std::vector<BackendInfo>& backends);

  /*! \brief Add backend to the underlying backend list
   *
   * This is called when Broadcast receives information
   * of a new backend.
   */

  void addBackend(const std::string& hostName, int port, float load);

  /*! \brief Remove backend from the underlying backend list
   *
   * This is called when Broadcast remove backend from its
   * backend list.
   */

  void removeBackend(const std::string& hostName, int port);

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

  virtual void redistribute() = 0;

  boost::taus88 itsGenerator;  /// The randomizer object for RNG.

  std::vector<BackendInfo> itsBackendInfos;  /// The internal backend list.

  boost::random::discrete_distribution<> itsDistribution;  /// The probability distribution for RNG.

  SmartMet::Spine::MutexType itsMutex;  /// The mutex to ensure RNG thread safety.

  float itsBalancingCoefficient;  /// The balancing coefficient for distribution generation.
};

typedef boost::shared_ptr<BackendForwarder> BackendForwarderPtr;

}  // namespace SmartMet
