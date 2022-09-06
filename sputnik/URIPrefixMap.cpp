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
    prefixMap[prefix].insert(host_port);
#ifdef MYDEBUG
    std::cout << boost::posix_time::second_clock::local_time() << "Add prefix '" << prefix <<
              << " for '" << host_port << std::endl;
#endif
}

void URIPrefixMap::removeBackend(const std::string& prefix, const BackendServicePtr& backendService)
{
    const auto backend_ptr = backendService->Backend();
    const std::string host_port = backend_ptr->Name() + ":" + std::to_string(backend_ptr->Port());
    decltype(prefixMap)::iterator curr = prefixMap.find(prefix);
    if (curr != prefixMap.end()) {
#ifdef MYDEBUG
    std::cout << boost::posix_time::second_clock::local_time() << "Remove prefix '" << prefix <<
              << " from '" << host_port << std::endl;
#endif
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
    for (const auto& curr : prefixMap) {
        if (ba::starts_with(uri, curr.first)) {
#ifdef MYDEBUG
            std::cout << boost::posix_time::second_clock::local_time() << "Translated URI '" << uri
                      << "' to prefix '" << curr.first << std::endl;
#endif
        }
    }
    return uri;
}

