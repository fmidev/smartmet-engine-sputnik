# smartmet-engine-sputnik

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem.

## Overview

The Sputnik engine facilitates communication in the SmartMet Server frontend/backend cluster. It uses UDP broadcasting for service discovery, allowing frontend servers to automatically discover backend servers and distribute load across them.

## API

The Sputnik-module is initialized by calling the constructor.

Calling the constructor will not, however, start sending or listening
anything. Call to appropriate API method is required for
that. theConfig must specify the libconfig++ library.

<pre><code>Sputnik(const std::string & theConfig); </code></pre>

The frontend and backend configuration parameters are taken from the
sputnik.conf file.

The launch method is invoked with different modes such as "Backend"
and "Frontend".

If Backend mode is true, start actively listen to the UDP broadcasts
in the network.

<pre><code>void launch(bool Backend); </code></pre>

If the mode is "Frontend", the Sputnik engine sets up the backend
forwarding mode thereby initializing the communication and starts the
SDR (Service Discovery Request) messages.

<pre><code>void launch(bool Frontend); </code></pre>

If the Backend or Frontend mode is false, the launch method will start
the asynchronous handlers, background tasks detached from the main
functionality, to process the communication between the frontend and
the backend. The asynchronous handler is invoked by a separate
boost::thread  and the launch() method returns almost immediately.


Using the broadcast method  a pre-formatted broadcast packet can be send  to all the  servers in the network.
<pre><code> int Broadcast(const std::string & theMessageBuffer); </code></pre>


## SputnikMessage

SputkikMessage is a data packet delivered via UDP broadcast to all the servers (both frontend and backend) in the SmartMet server message.

The message processing is implemented via <a href="https://developers.google.com/protocol-buffers/">Google Protocol Buffers</a>. The current schema for UDP Messages is:

<pre><code>package SmartMetserver; </code></pre>

<pre><code>
message SputnikMessage
{
// Human-readable name of the sending host (REQUIRED)
required string name = 1;
</code></pre>

<pre><code> 
// This specified the type of the SputnikMessage (REQUIRED)
enum MessageTypes
{
SERVICE_DISCOVERY_REQUEST = 0;
SERVICE_DISCOVERY_REPLY = 1;
SERVICE_DISCOVERY_BEACON = 2;
}
required MessageTypes messageType = 2;
</code></pre>

<pre><code>
// This specified the SputnikMessage sequence number, so that messages
// target to correct machine in case of multiple broadcasts.
required int32 seqnum = 3;
 </code></pre>

<pre><code>
// hostInfo submessage used for server discovery
message HostInfo {
required string ip = 1; // IP address of the backends HTTP server
required int32 port = 2; // IP port of the backends HTTP server
required string comment = 3; // Comment associated with this host
required float load = 4; // Current load of this server
}
optional HostInfo host = 4;
</code></pre>

<pre><code> 
// Service submessage used for service discovery
message Service
{
required string uri = 1; // URI of the service
required int32 lastupdate = 2;// Unix timestamp for the last data update
required bool allowcache = 4; // Whether caching of this service is allowed
</code></pre>

<pre><code> 
}
repeated Service services = 8;
</code></pre>

<pre><code>}</code></pre>

## Events

The UDP Broadcast must be done by the backend in the following events:

1. Data update in Service, i.e., plugin has uploaded some data and caches should be flushed.
2. Backend server Service additions, i.e. new plugin loaded.
3. Backend server Service removal, i.e. plugin unloaded.
4. Backend server start
5. Backend server shutdown

The UDP Broadcast must be done by the Frontend in the following events:

1. Frontend server start
2. Periodically to refresh Service and routing information

These UDP broadcasts are mostly done automatically by Sputnik using
the related Services-information.

Some broadcasts must specifically initiated by an engine/plugin in
situations where data refresh is needed are not known to Sputnik.

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
hostname = "junuu";  // Name of the host
httpAddress = "192.168.13.12"; // IP of this server
httpPort = 80;
udpListenerAddress = "192.168.222.255"; // broadcast 
udpListenerPort = 31337;
comment = "SmartMet server in junnu";
forwarding = "inverseload" #For frontend behaviour
</code></pre>

A sample  backend Sputnik configuration is given below.

<pre><code>
hostname = "localhost";
httpAddress = "192.168.122.135";
httpPort = 8090;
udpListenerAddress = "192.168.122.255";
udpListenerPort = 31337;
comment = "SmartMet server in myhost";
</code></pre>

## Documentation

- [Docker configuration tutorial](docs/docker.md)

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues).
