#pragma once

/*! \brief Sticky forwarding logic: rendezvous (HRW) hashing with connection safety.
 *
 */

#include "BackendForwarder.h"
#include <string>

namespace SmartMet
{
/*! \brief Sticky forwarder
 *
 * Routes a client to the same backend as long as the backend set is unchanged.
 *
 * The selection key is chosen in priority order:
 *   1. A client-chosen cookie (configurable name) if present -- this lets a
 *      client opt into a stable identity of its own choosing.
 *   2. Otherwise the effective client IP (X-Forwarded-For if present, else the
 *      socket peer) combined with the User-Agent, to separate distinct clients
 *      that share a single cloud/NAT egress address.
 *
 * The key is mapped to a backend with rendezvous (Highest Random Weight)
 * hashing, so adding or removing one backend only remaps the keys that were
 * pinned to the changed backend.
 *
 * As a safety measure backends whose active-connection count is significantly
 * higher than the least-loaded backend are excluded from selection entirely;
 * their keys spill deterministically to the next-best backend.
 */

class StickyForwarder : public BackendForwarder
{
 public:
  StickyForwarder(float balancingCoefficient, std::string cookieName);
  ~StickyForwarder() override;

  StickyForwarder(const StickyForwarder& other) = delete;
  StickyForwarder& operator=(const StickyForwarder& other) = delete;
  StickyForwarder(StickyForwarder&& other) = delete;
  StickyForwarder& operator=(StickyForwarder&& other) = delete;

  std::size_t getBackend(Spine::Reactor& theReactor,
                         const Spine::HTTP::Request& theRequest) override;

 private:
  std::string extractKey(const Spine::HTTP::Request& theRequest) const;

  std::string itsCookieName;  /// Affinity cookie name; empty disables the cookie step.
};

using BackendForwarderPtr = std::shared_ptr<BackendForwarder>;

}  // namespace SmartMet
