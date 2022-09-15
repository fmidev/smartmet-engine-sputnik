#include "URIPrefixMap.h"
#include <boost/algorithm/string/predicate.hpp>

using SmartMet::URIPrefixMap;
namespace ba = boost::algorithm;

URIPrefixMap::URIPrefixMap() = default;
URIPrefixMap::~URIPrefixMap() = default;

void URIPrefixMap::addPrefix(const std::string& prefix, const BackendServicePtr& backendService)
{
    const auto backend_ptr = backendService->Backend();
    const std::string host_port = backend_ptr->Name() + ":" + std::to_string(backend_ptr->Port());
#if defined(MYDEBUG)
    std::cout << boost::posix_time::second_clock::local_time() << " Add prefix '" << prefix
              << " for '" << host_port << std::endl;
#endif
    std::unique_lock<std::mutex> lock(mutex);
    prefixMap[prefix].insert(host_port);
}

void URIPrefixMap::removeBackend(const std::string& prefix, const BackendServicePtr& backendService)
{
    const auto backend_ptr = backendService->Backend();
    const std::string host_port = backend_ptr->Name() + ":" + std::to_string(backend_ptr->Port());
#if defined(MYDEBUG)
    std::cout << boost::posix_time::second_clock::local_time() << " Remove prefix '" << prefix
              << " from '" << host_port << std::endl;
#endif
    std::unique_lock<std::mutex> lock(mutex);
    decltype(prefixMap)::iterator curr = prefixMap.find(prefix);
    if (curr != prefixMap.end()) {
        curr->second.erase(host_port);
    }
    if (curr->second.empty()) {
        prefixMap.erase(curr);
    }
}

void URIPrefixMap::removeBackend(const BackendServicePtr& backendService)
{
    const auto backend_ptr = backendService->Backend();
    const std::string host_port = backend_ptr->Name() + ":" + std::to_string(backend_ptr->Port());
    decltype(prefixMap)::iterator curr, next;
    std::unique_lock<std::mutex> lock(mutex);
    for (curr = prefixMap.begin(); curr != prefixMap.end(); curr = next) {
        next = curr;
        ++next;
        curr->second.erase(host_port);
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

