#pragma once

/*! \brief Contains inverse forwarding logic used in Broadcast
 *
 * Declarations below contain different backend forwarding algorithms. To
 * add a new backend forwarder, derive from BackendForwarder and implement
 * the 'redistribute' method.
 *
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Inverse load forwarder
 *
 * Forwards requests by weighting backends using the forumula
 * w(l) = 1 / (1 + a*l) where 'l' is reported backend load
 * 'a' is the balancing coefficient .
 */

class InverseLoadForwarder : public BackendForwarder
{
 public:
  InverseLoadForwarder(float balancingCoefficient);

  ~InverseLoadForwarder() override;

 private:
  void redistribute(Spine::Reactor& theReactor) override;
};

}  // namespace SmartMet
