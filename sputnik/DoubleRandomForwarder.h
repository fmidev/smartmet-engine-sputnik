#pragma once

/*! \brief Contains double random forwarding logic used in Broadcast
 *
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Random forwarder
 *
 * Old style random sampling forwarder.
 */

class DoubleRandomForwarder : public BackendForwarder
{
 public:
  DoubleRandomForwarder();

  ~DoubleRandomForwarder() override;

  DoubleRandomForwarder(const DoubleRandomForwarder& other) = delete;
  DoubleRandomForwarder& operator=(const DoubleRandomForwarder& other) = delete;
  DoubleRandomForwarder(DoubleRandomForwarder&& other) = delete;
  DoubleRandomForwarder& operator=(DoubleRandomForwarder&& other) = delete;

  std::size_t getBackend(Spine::Reactor& theReactor) override;
};

using BackendForwarderPtr = boost::shared_ptr<BackendForwarder>;

}  // namespace SmartMet
