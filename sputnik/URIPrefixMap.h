#pragma once

#include "BackendService.h"
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace SmartMet
{
using BackendServicePtr = std::shared_ptr<BackendService>;

class URIPrefixMap
{
 public:
  URIPrefixMap() = default;
  virtual ~URIPrefixMap();

  URIPrefixMap(const URIPrefixMap& other) = delete;
  URIPrefixMap& operator=(const URIPrefixMap& other) = delete;
  URIPrefixMap(URIPrefixMap&& other) = delete;
  URIPrefixMap& operator=(URIPrefixMap&& other) = delete;

  void addPrefix(const std::string& prefix, const BackendServicePtr& backendService);
  void removeBackend(const std::string& prefix, const BackendServicePtr& backend);

  std::string operator()(const std::string& uri) const;

 private:
  mutable std::mutex mutex;
  std::map<std::string, std::set<std::string> > prefixMap;
};
}  // namespace SmartMet
