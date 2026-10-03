#define BOOST_TEST_MODULE "SputnikForwarderTest"

#include "BackendServer.h"
#include "BackendService.h"
#include "InverseLoadForwarder.h"
#include "LeastConnectionsForwarder.h"
#include "RandomForwarder.h"
#include "StickyForwarder.h"
#include "URIPrefixMap.h"
#include <boost/test/included/unit_test.hpp>
#include <spine/HTTP.h>
#include <spine/Options.h>
#include <spine/Reactor.h>
#include <map>
#include <memory>

using namespace SmartMet;

namespace
{
std::unique_ptr<Spine::Reactor> make_reactor()
{
  Spine::Options opts;
  opts.configfile = "cnf/reactor.conf";
  opts.parseConfig();
  return std::make_unique<Spine::Reactor>(opts);
}

Spine::Reactor& reactor()
{
  static auto instance = make_reactor();
  return *instance;
}

BackendServicePtr service(const std::string& host, int port, int sequence, bool prefix = true)
{
  auto server = std::make_shared<BackendServer>(host, host, port, "", 1.0F, 0);
  return std::make_shared<BackendService>(server, "/x", 0, true, sequence, prefix);
}

Spine::HTTP::Request request(const std::string& ip, const std::string& agent = "")
{
  Spine::HTTP::Request req;
  req.setClientIP(ip);
  if (!agent.empty())
    req.setHeader("User-Agent", agent);
  return req;
}

template <typename Forwarder>
void add_backends(Forwarder& forwarder, int n, float load = 1.0F)
{
  for (int i = 0; i < n; i++)
    forwarder.addBackend("backend" + std::to_string(i), 8080, load, reactor());
}

}  // namespace

BOOST_AUTO_TEST_CASE(uri_prefix_map)
{
  URIPrefixMap map;
  map.addPrefix("/wmts", service("b1", 8080, 1));
  map.addPrefix("/edr", service("b1", 8080, 1));
  map.addPrefix("/edr/collections", service("b2", 8080, 1));

  // URIs under a prefix map to the prefix
  BOOST_CHECK_EQUAL(map("/wmts"), "/wmts");
  BOOST_CHECK_EQUAL(map("/wmts/1.0.0/WMTSCapabilities.xml"), "/wmts");
  BOOST_CHECK_EQUAL(map("/edr/locations"), "/edr");

  // The longest matching prefix wins
  BOOST_CHECK_EQUAL(map("/edr/collections/pal/position"), "/edr/collections");

  // A prefix must end at a path separator, as in the spine content handler dispatch
  BOOST_CHECK_EQUAL(map("/wmtsx"), "/wmtsx");
  BOOST_CHECK_EQUAL(map("/edrfoo/bar"), "/edrfoo/bar");

  // Other URIs are returned as is
  BOOST_CHECK_EQUAL(map("/timeseries"), "/timeseries");

  // The prefix stays until the last backend defining it is removed
  map.addPrefix("/wmts", service("b2", 8080, 1));
  map.removeBackend("/wmts", service("b1", 8080, 1));
  BOOST_CHECK_EQUAL(map("/wmts/x"), "/wmts");
  map.removeBackend("/wmts", service("b2", 8080, 1));
  BOOST_CHECK_EQUAL(map("/wmts/x"), "/wmts/x");

  // Removing an unknown backend is harmless
  map.removeBackend("/edr", service("b9", 8080, 1));
  BOOST_CHECK_EQUAL(map("/edr/x"), "/edr");
}

BOOST_AUTO_TEST_CASE(random_forwarder)
{
  RandomForwarder forwarder;
  BOOST_CHECK_THROW(forwarder.getBackend(reactor(), request("1.2.3.4")), std::exception);

  add_backends(forwarder, 3);
  std::map<std::size_t, int> counts;
  for (int i = 0; i < 3000; i++)
    counts[forwarder.getBackend(reactor(), request("1.2.3.4"))]++;

  BOOST_CHECK_EQUAL(counts.size(), 3U);
  for (const auto& item : counts)
  {
    BOOST_CHECK_LT(item.first, 3U);
    BOOST_CHECK_GT(item.second, 800);  // about 1000 each
  }
}

BOOST_AUTO_TEST_CASE(inverse_load_forwarder)
{
  InverseLoadForwarder forwarder(1.0F);
  forwarder.addBackend("light", 8080, 1.0F, reactor());
  forwarder.addBackend("heavy", 8080, 9.0F, reactor());

  // Weights 1/(1+1) and 1/(1+9), hence the light backend gets 5/6 of the requests
  int light = 0;
  const int n = 6000;
  for (int i = 0; i < n; i++)
    if (forwarder.getBackend(reactor(), request("1.2.3.4")) == 0)
      light++;

  BOOST_CHECK_GT(light, 4700);
  BOOST_CHECK_LT(light, 5300);
}

BOOST_AUTO_TEST_CASE(least_connections_forwarder)
{
  LeastConnectionsForwarder forwarder;
  forwarder.addBackend("lc1", 8080, 1.0F, reactor());
  forwarder.addBackend("lc2", 8080, 1.0F, reactor());
  forwarder.addBackend("lc3", 8080, 1.0F, reactor());

  // Busy backends are avoided
  reactor().startBackendRequest("lc1", 8080);
  reactor().startBackendRequest("lc3", 8080);
  for (int i = 0; i < 100; i++)
    BOOST_CHECK_EQUAL(forwarder.getBackend(reactor(), request("1.2.3.4")), 1U);

  reactor().removeBackendRequests("lc1", 8080);
  reactor().removeBackendRequests("lc3", 8080);
}

BOOST_AUTO_TEST_CASE(sticky_forwarder)
{
  StickyForwarder forwarder(2.0F, "session");
  BOOST_CHECK_THROW(forwarder.getBackend(reactor(), request("1.2.3.4")), std::exception);

  add_backends(forwarder, 5);

  // The same client always gets the same backend
  const auto a = forwarder.getBackend(reactor(), request("10.0.0.1", "agent"));
  for (int i = 0; i < 10; i++)
    BOOST_CHECK_EQUAL(forwarder.getBackend(reactor(), request("10.0.0.1", "agent")), a);

  // Different clients are spread over the backends
  std::map<std::size_t, int> counts;
  for (int i = 0; i < 1000; i++)
    counts[forwarder.getBackend(reactor(), request("10.0.1." + std::to_string(i % 250), "a" + std::to_string(i)))]++;
  BOOST_CHECK_EQUAL(counts.size(), 5U);

  // The leftmost X-Forwarded-For address identifies the client
  auto r1 = request("192.168.0.1", "agent");
  r1.setHeader("X-Forwarded-For", "10.0.0.1, 192.168.0.1");
  BOOST_CHECK_EQUAL(forwarder.getBackend(reactor(), r1), a);

  // The cookie overrides the address
  auto c1 = request("10.0.0.1", "agent");
  c1.setHeader("Cookie", "foo=bar; session=abc123");
  auto c2 = request("10.9.9.9", "other");
  c2.setHeader("Cookie", "session=abc123");
  BOOST_CHECK_EQUAL(forwarder.getBackend(reactor(), c1), forwarder.getBackend(reactor(), c2));

  // Rendezvous hashing: removing a backend which was not chosen does not move the client
  const std::string chosen = "backend" + std::to_string(a);
  const std::string other = (a == 0 ? "backend1" : "backend0");
  forwarder.removeBackend(other, 8080, reactor());
  const auto b = forwarder.getBackend(reactor(), request("10.0.0.1", "agent"));
  // The indices shift if an earlier backend was removed
  const auto expected = (other < chosen ? a - 1 : a);
  BOOST_CHECK_EQUAL(b, expected);

  // A hotspot is avoided: give the chosen backend many more requests than the others
  for (int i = 0; i < 50; i++)
    reactor().startBackendRequest(chosen, 8080);
  BOOST_CHECK_NE(forwarder.getBackend(reactor(), request("10.0.0.1", "agent")), expected);
  reactor().removeBackendRequests(chosen, 8080);
  BOOST_CHECK_EQUAL(forwarder.getBackend(reactor(), request("10.0.0.1", "agent")), expected);
}
