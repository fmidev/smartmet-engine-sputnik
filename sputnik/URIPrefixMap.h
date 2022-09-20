#pragma once

#include "BackendService.h"
#include <string>
#include <set>
#include <map>
#include <mutex>

namespace SmartMet
{
  using BackendServicePtr = boost::shared_ptr<BackendService>;

  class URIPrefixMap
  {
  public:
    URIPrefixMap();
    virtual ~URIPrefixMap();

    void addPrefix(const std::string& prefix, const BackendServicePtr& backendService);
    void removeBackend(const std::string& prefix, const BackendServicePtr& backend);

    std::string operator () (const std::string& uri) const;

  private:
    mutable std::mutex mutex;
    std::map<std::string, std::set<std::string> > prefixMap;
  };
}
