#include "URIPrefixMap.h"
#include <boost/algorithm/string/predicate.hpp>

using SmartMet::URIPrefixMap;
namespace ba = boost::algorithm;

URIPrefixMap::~URIPrefixMap() = default;

namespace
{
std::string backend_service_id(const std::string& /* prefix */,
                               const SmartMet::BackendServicePtr& backendService)
{
  const auto backend_ptr = backendService->Backend();
  std::string id = backend_ptr->Name() + ":" + std::to_string(backend_ptr->Port()) + ":" +
                   std::to_string(backendService->SequenceNumber());
  return id;
}
}  // namespace

void URIPrefixMap::addPrefix(const std::string& prefix, const BackendServicePtr& backendService)
{
  const std::string id = backend_service_id(prefix, backendService);
  std::unique_lock<std::mutex> lock(mutex);
  const bool added = prefixMap[prefix].insert(id).second;
#if defined(MYDEBUG)
  if (added)
  {
    std::cout << "URIPrefixMap::addPrefix: prefix=" << prefix << " id=" << id << std::endl;
  }
  else
  {
    std::cout << "URIPrefixMap::addPrefix: prefix=" << prefix << " failed to add id (already present)" << id
              << std::endl;
  }
#else
  (void)added;   // Silence warning when MYDEBUG is not defined
#endif
}

void URIPrefixMap::removeBackend(const std::string& prefix, const BackendServicePtr& backendService)
{
  const std::string id = backend_service_id(prefix, backendService);
  std::unique_lock<std::mutex> lock(mutex);
  auto curr = prefixMap.find(prefix);
  if (curr != prefixMap.end())
  {
    const bool removed = curr->second.erase(id);
    if (removed)
    {
#if defined(MYDEBUG)
      std::cout << "URIPrefixMap::removeBackend: prefix=" << prefix << " id=" << id << " : removed"
                << std::endl;
#endif
      if (curr->second.empty())
      {
        prefixMap.erase(curr);
      }
    }
    else
    {
#if defined(MYDEBUG)
      std::cout << "URIPrefixMap::removeBackend: prefix=" << prefix << " id=" << id
                << " : failed to remove (not found)" << std::endl;
#endif
    }
  }
}

std::string URIPrefixMap::operator()(const std::string& uri) const
{
  std::unique_lock<std::mutex> lock(mutex);
  for (const auto& curr : prefixMap)
  {
    if (ba::starts_with(uri, curr.first))
    {
#if defined(MYDEBUG)
      std::cout << Fmi::SecondClock::local_time() << "Translated URI '" << uri
                << "' to prefix '" << curr.first << std::endl;
#endif
      return curr.first;
    }
  }
  return uri;
}
