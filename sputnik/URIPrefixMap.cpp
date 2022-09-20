#include "URIPrefixMap.h"
#include <boost/algorithm/string/predicate.hpp>

using SmartMet::URIPrefixMap;
namespace ba = boost::algorithm;

URIPrefixMap::URIPrefixMap() = default;
URIPrefixMap::~URIPrefixMap() = default;

namespace
{
    std::string backend_service_id(const std::string& prefix,
                                   const SmartMet::BackendServicePtr& backendService)
    {
        const auto backend_ptr = backendService->Backend();
        const std::string id = backend_ptr->Name()
            + ":" + std::to_string(backend_ptr->Port())
            + ":" + std::to_string(backendService->SequenceNumber());
        return id;
    }
}

void URIPrefixMap::addPrefix(const std::string& prefix, const BackendServicePtr& backendService)
{
    const std::string id = backend_service_id(prefix, backendService);
    std::unique_lock<std::mutex> lock(mutex);
    if (prefixMap[prefix].insert(id).second) {
#if defined(MYDEBUG)
        std::cout << "URIPrefixMap::addPrefix: prefix=" << prefix << " id=" << id << std::endl;
#endif
    } else {
        std::cout << "URIPrefixMap::addPrefix: prefix=" << prefix
                  << " failed to remove id " << id << std::endl;
    }
}

void URIPrefixMap::removeBackend(const std::string& prefix, const BackendServicePtr& backendService)
{
    const std::string id = backend_service_id(prefix, backendService);
    std::unique_lock<std::mutex> lock(mutex);
    decltype(prefixMap)::iterator curr = prefixMap.find(prefix);
    if (curr != prefixMap.end()) {
        if (curr->second.erase(id)) {
#if defined(MYDEBUG)
            std::cout << "URIPrefixMap::removeBackend: prefix=" << prefix
                      << " id=" << id << " : removed" << std::endl;
#endif
        } else {
            std::cout << "URIPrefixMap::removeBackend: prefix=" << prefix
                      << " id=" << id << " : failed to remove (not found)" << std::endl;
        }
        if (curr->second.empty()) {
            prefixMap.erase(curr);
        }
    }
}

std::string URIPrefixMap::operator () (const std::string& uri) const
{
    std::unique_lock<std::mutex> lock(mutex);
    for (const auto& curr : prefixMap) {
        if (ba::starts_with(uri, curr.first)) {
#if 1 or defined(MYDEBUG)
            std::cout << boost::posix_time::second_clock::local_time() << "Translated URI '" << uri
                      << "' to prefix '" << curr.first << std::endl;
#endif
            return curr.first;
        }
    }
    return uri;
}

