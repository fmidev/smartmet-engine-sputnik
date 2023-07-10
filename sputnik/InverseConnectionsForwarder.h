#pragma once

/*! \brief Load balancing based on the number of active connections
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Inverse load forwarder
 *
 * Forwards requests by weighting backends using the forumula
 * w(l) = 1 / (1 + a*c) where 'c' is the number of active connections.
 */

class InverseConnectionsForwarder : public BackendForwarder
{
 public:
  explicit InverseConnectionsForwarder(float balancingCoefficient);

  ~InverseConnectionsForwarder() override;

  InverseConnectionsForwarder() = delete;
  InverseConnectionsForwarder(const InverseConnectionsForwarder& other) = delete;
  InverseConnectionsForwarder& operator=(const InverseConnectionsForwarder& other) = delete;
  InverseConnectionsForwarder(InverseConnectionsForwarder&& other) = delete;
  InverseConnectionsForwarder& operator=(InverseConnectionsForwarder&& other) = delete;

 private:
  void redistribute(Spine::Reactor& theReactor) override;
  void rebalance(Spine::Reactor& theReactor) override;
};

}  // namespace SmartMet
