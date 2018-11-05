#pragma once

#include <string>

namespace SmartMet
{
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

  BackendInfo() = delete;

  std::string hostName;
  int port;
  float load;
  mutable unsigned int throttle_counter = 0;
};

}  // namespace SmartMet
