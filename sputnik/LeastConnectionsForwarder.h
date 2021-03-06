#pragma once

/*! \brief Load balancing based on the number of active connections
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Inverse least connections forwarder
 *
 * Forwards requests by choosing the backend with the least number of active connections.
 */

class LeastConnectionsForwarder : public BackendForwarder
{
 public:
  LeastConnectionsForwarder();

  virtual ~LeastConnectionsForwarder();

 private:
  void redistribute(Spine::Reactor& theReactor);
  void rebalance(Spine::Reactor& theReactor);
};

}  // namespace SmartMet
