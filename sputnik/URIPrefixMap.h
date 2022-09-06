#pragma once

#include "BackendService.h"
#include <string>
#include <set>
#include <map>

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
    void removeBackend(const BackendServicePtr& backend);

    std::string operator () (const std::string& uri) const;

  private:
    std::map<std::string, std::set<std::string> > prefixMap;
  };
}
