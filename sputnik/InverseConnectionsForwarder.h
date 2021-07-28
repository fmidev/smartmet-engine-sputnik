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
  InverseConnectionsForwarder(float balancingCoefficient);

  ~InverseConnectionsForwarder() override;

 private:
  void redistribute(Spine::Reactor& theReactor) override;
  void rebalance(Spine::Reactor& theReactor);
};

}  // namespace SmartMet
