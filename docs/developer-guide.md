# Sputnik engine developer guide

This guide is for developers who change `smartmet-engine-sputnik`, the engine that holds a
SmartMet Server cluster together. It describes the discovery protocol, the two operating
modes, how the frontend's routing table is built and used, backend health tracking, the
forwarding strategies, and the pitfalls.

Related documents:

* [CLAUDE.md](../CLAUDE.md): notes on the forwarders, the intentional high-load
  retirement, and the ABI hazard of the inline accessors.
* The frontend plugin's [developer guide](https://github.com/fmidev/smartmet-plugin-frontend/blob/master/docs/developer-guide.md),
  the backend plugin's [developer guide](https://github.com/fmidev/smartmet-plugin-backend/blob/master/docs/developer-guide.md),
  and the server's [developer guide](https://github.com/fmidev/smartmet-server/blob/master/docs/developer-guide.md).
* [docker.md](docker.md).

## Contents

1. [Role in the cluster](#1-role-in-the-cluster)
2. [Building](#2-building)
3. [Source files](#3-source-files)
4. [The discovery protocol](#4-the-discovery-protocol)
5. [Backend mode](#5-backend-mode)
6. [Frontend mode and the routing table](#6-frontend-mode-and-the-routing-table)
7. [Forwarding strategies](#7-forwarding-strategies)
8. [Backend health](#8-backend-health)
9. [Configuration](#9-configuration)
10. [Binary compatibility](#10-binary-compatibility)
11. [Known pitfalls](#11-known-pitfalls)

---

## 1. Role in the cluster

The engine is loaded by every server in a cluster but does nothing until a plugin launches
it (`Engine::launch(mode, reactor)`):

* the **backend plugin** launches it in **backend mode**: answer the frontends' discovery
  requests with this server's services and load;
* the **frontend plugin** launches it in **frontend mode**: send discovery requests to all
  backends every few seconds, keep the routing table (`Services`) up to date, and pick a
  backend for each request.

Both modes run a Boost.Asio `io_context` on a thread of their own.

## 2. Building

```bash
make            # sputnik.so; runs protoc on BroadcastMessage.proto first
make install
make rpm
```

`BroadcastMessage.proto` generates `sputnik/BroadcastMessage.pb.{h,cpp}`, which are not
committed. There are no tests in this repository (`make test` passes only with `CI=true`);
the cluster behaviour is tested by the frontend's `RunClusterTests`.

## 3. Source files

| File | Role |
|------|------|
| `Engine.{h,cpp}` | Configuration, `launch()`, the backend listener, the frontend heartbeat loop, pause and continue, the `status` / `backends` output. |
| `Messages.cpp` | Building and handling `BroadcastMessage`s in both modes. |
| `Services.{h,cpp}` | The frontend's routing table: URI → backends + forwarder, backend removal and liveness, sentinels, info-request routing. |
| `BackendServer.h`, `BackendService.h`, `BackendInfo*.h` | A backend host, one URI it serves, the info requests it answers. |
| `URIPrefixMap.{h,cpp}` | Prefix-registered URIs. |
| `BackendForwarder.{h,cpp}` and `*Forwarder.{h,cpp}` | The forwarding strategies. |
| `BackendSentinel.{h,cpp}` | Per-backend count of unanswered transfers (throttling). |

## 4. The discovery protocol

Messages are `BroadcastMessage` protobufs over UDP:

| Field | Content |
|-------|---------|
| `name`, `messageType`, `seqnum` | Sender, `SERVICE_DISCOVERY_REQUEST` / `REPLY` (`BEACON` is defined but ignored), and the frontend's cycle number, echoed in replies. |
| `host` | The backend's HTTP address, port, comment, load average and throttle limit. |
| `services` | One entry per public URI: `uri`, `is_prefix`, and the unused `lastupdate` / `allowcache`. |
| `infoQuery` | The names of the backend's public admin requests (`/info?what=…`), so the frontend can route those too. |

A cycle: the frontend sends a request with a new sequence number to every address in
`backendUdpListeners`; each running backend replies; the frontend merges the replies into
`Services` until `heartbeat.timeout` expires; then it prunes the backends that did not
answer this cycle, sleeps `heartbeat.interval`, and starts the next cycle.

## 5. Backend mode

The backend binds `udpListenerAddress:udpListenerPort` and answers each discovery request
with its host information, its URI map (spine's `getURIMap()`, so only **public** content
handlers) and its public admin requests. The HTTP port it announces comes from the server
configuration, not from sputnik's configuration.

It **does not answer** while it is paused, or while the server reports high load
(`Reactor::isLoadHigh()`). The frontends then drop it at the end of their cycle, and pick it
up again from the first reply after it recovers. That is how `pause` takes a backend out of
rotation without failing requests already in flight.

Pausing: `setPause()` (forever), `setPauseUntil(deadline)`, `setContinue()`, or
`pause = true` in the configuration to start paused. The backend plugin exposes these as
admin requests.

## 6. Frontend mode and the routing table

`Services` holds, for every URI, the list of `BackendService`s that serve it and a
`BackendForwarder` that picks one. Replies add backends and URIs (a URI that appears for
the first time gets a new forwarder of the configured kind); each change triggers
`redistribute()` on the affected forwarders.

At the end of a cycle, `Services::latestSequence(seq)` removes every backend whose last
reply is older than the current cycle. If **no** backend replied in a cycle, pruning is
postponed for up to `heartbeat.max_skipped_cycles` cycles, so a single lost burst of UDP
does not empty the table.

The frontend plugin uses:

* `getServices().getService(request)`: choose a backend for a request (prefix match
  first, then exact URI);
* `removeBackend(host, port)`: retire a backend now (connection failure or high load);
* `queryBackendAlive()`, `setBackendAlive()`: liveness, fed by the server's
  backend-connection-finished hook;
* `getBackendList()`, `getInfoRequestBackendList()`, `backends()`, `status()`: admin views.

## 7. Forwarding strategies

`forwarding` selects the strategy used for every URI:

| Value | Choice |
|-------|--------|
| `random` (default) | Uniformly random. |
| `doublerandom` | Two random backends, the less loaded one wins. |
| `inverseload` | Weighted by `1 / (1 + a·load)`. |
| `inverseconnections` | Weighted by `1 / (1 + a·active connections)`. |
| `leastconnections` | Fewest active connections. |
| `exponentialconnections` | Exponentially decreasing weight with active connections. |
| `sticky` | Rendezvous (HRW) hashing of the `sticky_cookie` value, or of client IP + User-Agent, so a client keeps hitting the same backend; backends with more than `balance_factor · min + slack` active connections are skipped. |

`a` is `balance_factor` (2.0). The load values come from the backends' replies, the
connection counts from the frontend's own active requests to each backend.

## 8. Backend health

* **Heartbeat.** A backend that stops answering disappears at the end of the next cycle,
  that is within about `interval + timeout` (7 s by default).
* **Connection outcomes.** The frontend registers a backend-connection-finished hook with
  the Reactor. A successful response marks the backend alive (`setBackendAlive()`); the
  frontend plugin retires backends on connection failures and on high-load replies with
  `removeBackend()`.
* **Throttle.** A backend's `throttle` (0 = off) limits how many transfers to it may be
  unanswered at once; `BackendSentinel` counts them, and a backend over its limit is
  treated as unresponsive.
* **Empty table.** If `removeBackend()` leaves no services at all, the frontend process
  `SIGKILL`s itself, to be restarted by systemd. See the pitfalls.

## 9. Configuration

Frontend settings:

| Key | Default | Meaning |
|-----|---------|---------|
| `backendUdpListeners` | required | The backends' discovery addresses. |
| `frontendUdpAddress`, `frontendUdpPort` | `0.0.0.0`, 0 | Local address for the discovery socket. |
| `forwarding` | `random` | Strategy (§7). |
| `balance_factor` | 2.0 | Weighting coefficient. |
| `sticky_cookie` | `smartmet-session-id` | Affinity cookie for `sticky`. |
| `heartbeat.interval` | 5 | Seconds between cycles. |
| `heartbeat.timeout` | 2 | Seconds to collect replies. |
| `heartbeat.max_skipped_cycles` | 2 | Empty cycles tolerated before pruning. |

Backend settings: `hostname` (`localhost`), `httpAddress` (`127.0.0.1`),
`udpListenerAddress` (`127.0.0.1`), `udpListenerPort`, `comment`, `throttle` (0), and
`pause` (false). The HTTP port is the server's `port`.

## 10. Binary compatibility

The frontend plugin calls `Engine::getServices()`, an **inline** accessor that returns
`itsServices` by reference. Its offset is compiled into the plugin. Adding or reordering
data members **before** `itsServices` makes an old frontend operate on a misaligned
`Services`, including its `shared_mutex`, and it hangs at startup; adding
`itsStickyCookie` did exactly that once. Append new members at the **end** of `Engine`,
and treat any change of `Engine`'s or `Services`' members as an ABI change: bump the
version, rebuild the frontend and backend plugins, and deploy them together.

## 11. Known pitfalls

* **The frontend can kill itself under cluster-wide overload.** `removeBackend()` sends
  `SIGKILL` when the table becomes empty, and the frontend also retires backends on
  high-load replies. When every backend is overloaded at once, the last retirement kills
  the frontend. The heartbeat path handles the same situation without the kill (it empties
  the table after the tolerated empty cycles, and requests get 404). The fix is to kill only
  when the last backend is genuinely dead (see CLAUDE.md).
* **Overload looks like absence.** A backend under high load stops replying and is dropped
  from the table, so a load spike across the cluster shows up as missing backends rather
  than as errors.
* **Only public URIs are routed.** Private content handlers are never announced, so they
  are unreachable through a frontend.
* **`lastupdate` and `allowcache` are not used.** Do not rely on them.
* **Inline accessors are ABI** (§10).
