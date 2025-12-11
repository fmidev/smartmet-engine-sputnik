#pragma once

#include "BackendServer.h"
#include <memory>
#include <string>

namespace SmartMet
{
/*! \brief This struct contains info request information from a backend server.
 *
 * This information is collected from backends through protobuf messages
 * and is used to track available admin/info requests across the cluster.
 */
class BackendInfoRequest
{
 private:
  std::shared_ptr<BackendServer> itsBackend;
  std::string itsName;          // Name of the info request (/info?what=<name>)
  int itsLastUpdate;            // Unix timestamp for the last data update
  int itsSequenceNumber;        // Sequence number from the broadcast message

 public:
  // Accessors
  std::shared_ptr<BackendServer> Backend() const { return itsBackend; }
  const std::string& Name() const { return itsName; }
  int LastUpdate() const { return itsLastUpdate; }
  int SequenceNumber() const { return itsSequenceNumber; }

  ~BackendInfoRequest() = default;

  BackendInfoRequest(std::shared_ptr<BackendServer> theBackend,
                     std::string theName,
                     int theLastUpdate,
                     int theSequenceNumber)
      : itsBackend(std::move(theBackend)),
        itsName(std::move(theName)),
        itsLastUpdate(theLastUpdate),
        itsSequenceNumber(theSequenceNumber)
  {
  }

  BackendInfoRequest() = delete;
  BackendInfoRequest(const BackendInfoRequest& other) = delete;
  BackendInfoRequest& operator=(const BackendInfoRequest& other) = delete;
  BackendInfoRequest(BackendInfoRequest&& other) = delete;
  BackendInfoRequest& operator=(BackendInfoRequest&& other) = delete;
};

}  // namespace SmartMet
