#include "StickyForwarder.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace SmartMet
{
namespace
{
// 64-bit FNV-1a, implemented explicitly so that every frontend computes the
// same key->backend mapping regardless of the std::hash / Boost version in use.
// (Multiple frontends must agree, or a client could be pinned differently
// depending on which frontend it happens to reach.)
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

// A backend is excluded when its active connections exceed
//   balance_factor * min_connections + kConnectionSlack.
// The slack keeps small absolute differences from triggering exclusion when
// the cluster is nearly idle.
constexpr int kConnectionSlack = 4;

std::uint64_t fnv1a(std::string_view str, std::uint64_t seed)
{
  std::uint64_t hash = seed;
  for (unsigned char c : str)
  {
    hash ^= c;
    hash *= kFnvPrime;
  }
  return hash;
}

// Return the value of the named cookie within a Cookie header, or "" if absent.
// Splits on ';' so a name is matched whole (no "id" matching "userid").
std::string parseCookie(const std::string& header, const std::string& name)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, header, boost::algorithm::is_any_of(";"));
  for (auto& part : parts)
  {
    boost::algorithm::trim(part);
    auto eq = part.find('=');
    if (eq != std::string::npos && part.substr(0, eq) == name)
      return part.substr(eq + 1);
  }
  return {};
}
}  // namespace

StickyForwarder::~StickyForwarder() = default;

StickyForwarder::StickyForwarder(float balancingCoefficient, std::string cookieName)
    : BackendForwarder(balancingCoefficient), itsCookieName(std::move(cookieName))
{
}

std::string StickyForwarder::extractKey(const Spine::HTTP::Request& theRequest) const
{
  try
  {
    // 1. Client-chosen cookie wins: the client has opted into a stable identity.
    if (!itsCookieName.empty())
    {
      auto cookie = theRequest.getHeader("Cookie");
      if (cookie)
      {
        std::string value = parseCookie(*cookie, itsCookieName);
        if (!value.empty())
          return "c:" + value;
      }
    }

    // 2. Effective client IP: X-Forwarded-For (leftmost) if present, else peer.
    std::string ip;
    auto xff = theRequest.getHeader("X-Forwarded-For");
    if (xff)
    {
      auto comma = xff->find(',');
      ip = (comma == std::string::npos) ? *xff : xff->substr(0, comma);
      boost::algorithm::trim(ip);
    }
    if (ip.empty())
      ip = theRequest.getClientIP();

    // 3. Combine with User-Agent to separate distinct clients behind one IP.
    std::string userAgent;
    auto agent = theRequest.getHeader("User-Agent");
    if (agent)
      userAgent = *agent;

    return "u:" + ip + "|" + userAgent;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t StickyForwarder::getBackend(Spine::Reactor& theReactor,
                                        const Spine::HTTP::Request& theRequest)
{
  try
  {
    SmartMet::Spine::WriteLock lock(itsMutex);

    if (itsBackendInfos.empty())
      throw Fmi::Exception(BCP, "No backends available!");

    // --- Connection-based safety: exclude clear hotspots --------------------
    auto connections = theReactor.getBackendRequestStatus();

    std::vector<int> counts(itsBackendInfos.size());
    int min_count = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < itsBackendInfos.size(); ++i)
    {
      const auto& info = itsBackendInfos[i];
      counts[i] = connections[info.hostName][info.port];  // zero if not found
      min_count = std::min(min_count, counts[i]);
    }

    const double factor = (itsBalancingCoefficient > 1.0F ? itsBalancingCoefficient : 2.0);
    const int threshold = static_cast<int>(factor * min_count) + kConnectionSlack;
    // Backends at min_count always satisfy count <= threshold, so the eligible
    // set below is provably non-empty.

    // --- Rendezvous (HRW) hashing over the eligible backends ----------------
    const std::string key = extractKey(theRequest);
    const std::uint64_t keyHash = fnv1a(key, kFnvOffset);

    std::size_t bestIndex = 0;
    std::uint64_t bestScore = 0;
    bool first = true;
    for (std::size_t i = 0; i < itsBackendInfos.size(); ++i)
    {
      if (counts[i] > threshold)
        continue;  // hotspot: not selectable at all

      const auto& info = itsBackendInfos[i];
      const std::string id = info.hostName + ":" + std::to_string(info.port);
      const std::uint64_t score = fnv1a(id, keyHash);

      if (first || score > bestScore)
      {
        bestScore = score;
        bestIndex = i;
        first = false;
      }
    }

    return bestIndex;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace SmartMet
