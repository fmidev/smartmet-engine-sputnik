# smartmet-engine-sputnik

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem.

## Overview

The Sputnik engine facilitates communication in the SmartMet Server frontend/backend cluster. It uses UDP broadcasting for service discovery, allowing frontend servers to automatically discover backend servers and distribute load across them.

## API

The server loads the engine and constructs it with the name of its
configuration file. Constructing it does not start sending or
listening anything:

<pre><code>Engine(const char* theConfig);</code></pre>

The frontend and backend configuration parameters are taken from the
sputnik.conf file.

The backend and frontend plugins start the engine in their own mode:

<pre><code>void launch(BroadcastMode theMode, Spine::Reactor* theReactor);</code></pre>

In `Backend` mode the engine listens to the discovery requests of the
frontends on its UDP address and answers them with the services of
this server. In `Frontend` mode it sets up the configured forwarding
mode and starts sending SDR (Service Discovery Request) messages to
the backends periodically. In both modes the communication is handled
by asynchronous handlers running in a thread of their own, and
`launch()` returns almost immediately.

The frontend plugin picks a backend for each request from the routing
table returned by `getServices()`.


## BroadcastMessage

BroadcastMessage is a data packet delivered via UDP between the
frontend and the backend servers of a SmartMet server cluster.

The message processing is implemented via <a href="https://developers.google.com/protocol-buffers/">Google Protocol Buffers</a>. The current schema for UDP Messages (`BroadcastMessage.proto`) is:

<pre><code>package SmartMet;

message BroadcastMessage
{
 // Human-readable name of the sending host (REQUIRED)
 required string name = 1;

 // This specified the type of the BroadcastMessage (REQUIRED)
 enum MessageTypes
 {
   SERVICE_DISCOVERY_REQUEST = 0;
   SERVICE_DISCOVERY_REPLY = 1;
   SERVICE_DISCOVERY_BEACON = 2;
 }
 required MessageTypes messageType = 2;

 // This specified the BroadcastMessage sequence number, so that messages
 // target to correct machine in case of multiple broadcasts.
 required int32 seqnum = 3;

 // hostInfo submessage used for server discovery
 message HostInfo
 {
  required string ip = 1;      // IP address of the backend HTTP server
  required int32 port = 2;     // IP port of the backend HTTP server
  required string comment = 3; // Comment associated with the backend server
  required float load = 4;     // Current load of the backend server
  optional int32 throttle = 5; // How many unanswered transfers are allowed
 }
 optional HostInfo host = 4;

 // Service submessage used for service discovery
 message Service
 {
  required string uri = 1;       // URI of the service
  required int32 lastupdate = 2; // Unused, always 0
  required bool allowcache = 4;  // Unused, always false
  optional bool is_prefix = 5;   // Whether provided uri is prefix
 }
 repeated Service services = 8;

 message InfoQuery
 {
  required string name = 1;      // Name of info InfoQuery (/info?what=&lt;name&gt;)
  required int32 lastupdate = 2; // Unused, always 0
 }
 repeated InfoQuery infoQuery = 16;
}
</code></pre>

The unused fields stay required only so that clusters with servers
of different versions keep interoperating. `SERVICE_DISCOVERY_BEACON`
is defined but ignored.

## Events

The frontend sends a discovery request to every address in
`backendUdpListeners` every `heartbeat.interval` seconds. Each running
backend answers with its host information, its public URIs and its
public admin requests, unless it is paused or under high load. The
frontend collects the replies for `heartbeat.timeout` seconds and then
drops the backends which did not answer. Hence new backends, new or
removed plugins and stopped backends are noticed within one cycle
without any explicit broadcasts.

## Configuration

As mentioned earlier we should have to specify the configuration for
both the  frontend and the backend servers in the Sputnik engine. Sample
configurations for frontend and backend are discussed in the following
sections.


### Frontend Sputink configuration 

The frontend parameters are the list of the IP addresses and the UDP
ports that the backend servers are listening to. The frontend sends
its "ServiceDiscovery" requests to these UDP addresses. Notice that
this list can contain direct IP addresses and broadcast addresses.

<pre><code>
backendUdpListeners = ["192.168.122.255:31337","192.168.122.135:31338","192.168.122.135:31339"];
</code></pre>

The backend servers should be configured so that they are listening
one of these addresses, i.e., when the frontend sends a service
request to a broadcast address then the same broadcast address should
be configured in the backend as "udpListenerAddress" and
"udpListenerPort". The same is true when the frontend is using direct
IP addresses.  In this case the backend should listen exactly the same
addresses that the frontend is using.

The other frontend settings (`frontendUdpAddress`, `frontendUdpPort`,
`forwarding`, `balance_factor`, `sticky_cookie` and the `heartbeat`
group) are described in `cnf/sputnik.conf.sample` and in the
developer guide.

An Example showing the IP addresses and port addresses the frontend
and the backend can use is given below.

<pre><code>

  TYPE           FRONTEND                BACKEND 
  		 			 (udpListenerAddress+udpListenPort
  Broadcast      192.168.100.255:31337	 192.168.100.255:31337
  Direct IP      192.168.122.135:31000	 192.168.122.135:31000
  Direct IP      192.168.122.135:32000   192.168.122.135:32000
  Direct IP      192.168.122.138:31000   192.168.122.138:31000

</code></pre>

### Backend Sputnik configuration

The backend parameters with some example values are given below.

<pre><code>
hostname = "junuu";                     // Name of the host
httpAddress = "192.168.13.12";          // IP of this server
udpListenerAddress = "192.168.222.255"; // broadcast
udpListenerPort = 31337;
comment = "SmartMet server in junnu";
throttle = 0;                           // max unanswered transfers, 0 = unlimited
pause = false;                          // start paused
</code></pre>

The HTTP port announced to the frontends is the `port` of the server
itself, there is no setting for it in the Sputnik configuration.

A sample  backend Sputnik configuration is given below.

<pre><code>
hostname = "localhost";
httpAddress = "192.168.122.135";
udpListenerAddress = "192.168.122.255";
udpListenerPort = 31337;
comment = "SmartMet server in myhost";
</code></pre>

## Documentation

- [Developer guide](docs/developer-guide.md) — discovery protocol, backend and frontend modes, routing table, forwarders, health, binary compatibility
- [Docker configuration tutorial](docs/docker.md)

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues).
