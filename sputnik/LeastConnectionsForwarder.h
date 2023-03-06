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

  ~LeastConnectionsForwarder() override;

  LeastConnectionsForwarder(const LeastConnectionsForwarder& other) = delete;
  LeastConnectionsForwarder& operator=(const LeastConnectionsForwarder& other) = delete;
  LeastConnectionsForwarder(LeastConnectionsForwarder&& other) = delete;
  LeastConnectionsForwarder& operator=(LeastConnectionsForwarder&& other) = delete;

 private:
  void redistribute(Spine::Reactor& theReactor) override;
  void rebalance(Spine::Reactor& theReactor) override;
};

}  // namespace SmartMet
