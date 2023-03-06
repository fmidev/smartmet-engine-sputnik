#pragma once

/*! \brief Contains random forwarding logic used in Broadcast
 *
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Random forwarder
 *
 * Old style random sampling forwarder.
 */

class RandomForwarder : public BackendForwarder
{
 public:
  RandomForwarder();
  ~RandomForwarder() override;

  RandomForwarder(const RandomForwarder& other) = delete;
  RandomForwarder& operator=(const RandomForwarder& other) = delete;
  RandomForwarder(RandomForwarder&& other) = delete;
  RandomForwarder& operator=(RandomForwarder&& other) = delete;

  std::size_t getBackend(Spine::Reactor& theReactor) override;
};

using BackendForwarderPtr = boost::shared_ptr<BackendForwarder>;

}  // namespace SmartMet
