#pragma once

/*! \brief Load balancing based on the number of active connections, exponentially decreasing
 * weights
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Inverse load forwarder
 *
 * Forwards requests by weighting backends using the forumula
 * w(l) = exp(-alpha * c) where 'c' is the number of active connections.
 */

class ExponentialConnectionsForwarder : public BackendForwarder
{
 public:
  explicit ExponentialConnectionsForwarder(float balancingCoefficient);

  ~ExponentialConnectionsForwarder() override;

  ExponentialConnectionsForwarder() = delete;
  ExponentialConnectionsForwarder(const ExponentialConnectionsForwarder& other) = delete;
  ExponentialConnectionsForwarder& operator=(const ExponentialConnectionsForwarder& other) = delete;
  ExponentialConnectionsForwarder(ExponentialConnectionsForwarder&& other) = delete;
  ExponentialConnectionsForwarder& operator=(ExponentialConnectionsForwarder&& other) = delete;

 private:
  void redistribute(Spine::Reactor& theReactor) override;
  void rebalance(Spine::Reactor& theReactor) override;
};

}  // namespace SmartMet
