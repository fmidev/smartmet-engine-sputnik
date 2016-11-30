#pragma once

/*! \brief Contains random forwarding logic used in Broadcast
 *
 */

#include "BackendForwarder.h"

namespace SmartMet
{
/*! \brief Random forwarder
 *
 * Old style random sampling forwarder.
 */

class RandomForwarder : public BackendForwarder
{
 public:
  RandomForwarder();

  virtual ~RandomForwarder();

 private:
  void redistribute();
};

typedef boost::shared_ptr<BackendForwarder> BackendForwarderPtr;

}  // namespace SmartMet
