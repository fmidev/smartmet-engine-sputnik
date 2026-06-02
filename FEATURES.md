# smartmet-engine-sputnik — Feature List

A structured inventory of capabilities provided by the sputnik engine.
Use as a checklist when drafting release notes. When new functionality
is added, append the new entry under the matching section (and bump
the *Last updated* line at the bottom).

`smartmet-engine-sputnik` (output: `sputnik.so`) is the **cluster
management engine** of SmartMet Server. It performs UDP-based service
discovery and load-balanced request forwarding between frontend and
backend server instances. The same engine plays two roles depending
on configuration: a **frontend** discovers backends and routes
requests, a **backend** advertises its services and replies to
discovery probes.

---

## 1. Two operating modes

A single engine class (`Engine`) is launched into one of two roles by
the server via `launch(BroadcastMode, Reactor*)`:

- **Frontend mode** — periodically broadcasts UDP discovery requests
  to a configured list of backend listeners, collects replies, builds
  a live routing table, and is consulted by `smartmet-plugin-frontend`
  to forward HTTP requests.
- **Backend mode** — binds a UDP socket on
  `udpListenerAddress:udpListenerPort`, listens for service-discovery
  requests, and answers with the backend's URI map, current load
  average, and admin-request name list.

## 2. Service discovery protocol

- **UDP broadcast** — wire format defined in `BroadcastMessage.proto`,
  generated at build time into `sputnik/BroadcastMessage.pb.*`.
- **Message types**:
  - `SERVICE_DISCOVERY_REQUEST` — sent by frontends.
  - `SERVICE_DISCOVERY_REPLY` — sent by backends.
- **Heartbeat** — frontends send requests on `heartbeat.interval` and
  await replies within `heartbeat.timeout`.
- **Stale pruning** — backends missing `heartbeat.max_skipped_cycles`
  consecutive replies are pruned from the routing table.
- **Backend self-suppression** — a backend skips its reply when paused
  or when `Reactor::isLoadHigh()` reports overload.

## 3. URI routing

- **`Services`** — central frontend routing table mapping each URI to
  a list of backends + a forwarder.
- **`URIPrefixMap`** — backends can register URI **prefixes** via the
  protobuf `Service.is_prefix` flag; the frontend checks prefixes
  before exact-URI lookup.
- **`BackendService`** — per-URI backend entry (server, service path,
  active connections, load).
- **`BackendServer`** — physical backend identity (hostname, http
  address/port, comment).
- **`BackendInfo` / `BackendInfoRequest`** — admin metadata.

## 4. Load-balancing strategies

Seven `BackendForwarder` strategies are available; chosen with
`forwarding = ...` in `sputnik.conf`:

| Config value | Class | Algorithm |
|---|---|---|
| `random` | `RandomForwarder` | Uniform random selection. |
| `doublerandom` | `DoubleRandomForwarder` | Pick two random, choose the better one ("power-of-two-choices"). |
| `inverseload` | `InverseLoadForwarder` | Weight by `1/(1 + a*load)`. |
| `inverseconnections` | `InverseConnectionsForwarder` | Weight by `1/(1 + a*connections)`. |
| `leastconnections` | `LeastConnectionsForwarder` | Pick the backend with fewest active connections. |
| `exponentialconnections` | `ExponentialConnectionsForwarder` | Exponential weighting by connection count. |
| `sticky` | `StickyForwarder` | Session affinity via rendezvous (HRW) hashing, with hotspot exclusion. |

- **`balance_factor`** config tunes the `a` coefficient for the
  weighted strategies.
- **`BackendForwarder::redistribute()`** — re-weights when the backend
  list changes.
- **`BackendForwarder::rebalance()`** — optional per-request hook.

## 5. Sticky / session-affinity forwarding

`StickyForwarder` keeps a client on the same backend across requests:

- **Cookie key** — `sticky_cookie` (default `smartmet-session-id`)
  when present.
- **Fallback key** — effective client IP (`X-Forwarded-For` or peer
  address) combined with the User-Agent.
- **Rendezvous hashing (HRW)** — deterministic backend selection per
  key.
- **Hotspot exclusion** — backends with active connections >
  `balance_factor * min_connections + slack` are excluded from
  selection.

## 6. Backend health tracking

- **`BackendSentinel`** — per-backend throttling tracker. Counts
  unanswered HTTP attempts; the backend is marked unresponsive when
  the throttle limit is reached.
- **Sequence-number cleanup** — backends that miss the current
  heartbeat sequence get pruned from the routing table.
- **All-backends-gone watchdog** — `Services::removeBackend` issues
  `SIGKILL` if every backend disappears, forcing a fast restart of
  the frontend process.
- **`BackendConnectionFinishedHook`** — registered with the Reactor
  to update liveness from HTTP response completions, not only from
  UDP heartbeats.

## 7. Pause / resume

- **`setPause()`** — pause this engine instance (a backend won't
  reply to discovery requests while paused).
- **`setPauseUntil(deadline)`** — auto-resume at a wall-clock
  deadline.
- **`setContinue()`** — resume.
- **`pause` config key** — start in the paused state on launch.

## 8. Configuration

libconfig++ format (`cnf/sputnik.conf.sample` is the reference). Keys
split by role:

### Frontend
- **`backendUdpListeners`** (required) — list of `address:port` pairs
  to probe.
- **`frontendUdpAddress`**, **`frontendUdpPort`** — local bind for
  the frontend.
- **`forwarding`** — strategy selector (see §4).
- **`balance_factor`** — tunes weighted forwarders.
- **`sticky_cookie`** — affinity cookie name (sticky forwarder).
- **`heartbeat.interval`** — discovery cadence.
- **`heartbeat.timeout`** — reply window.
- **`heartbeat.max_skipped_cycles`** — stale-pruning threshold.

### Backend
- **`hostname`**, **`comment`** — identification.
- **`httpAddress`** — backend's HTTP bind address.
- **`udpListenerAddress`**, **`udpListenerPort`** — UDP bind for
  discovery replies.
- **`throttle`** — `BackendSentinel` threshold.
- **`pause`** — start paused.
- **`httpPort`** is read from the Reactor configuration (not from
  `sputnik.conf`).

## 9. Async runtime

- **Boost.Asio `io_context`** on a dedicated thread for UDP I/O.
- **Non-blocking launch** — `launch()` returns almost immediately
  after starting the background handler.
- **Thread-safe Services table** — readers (the frontend plugin)
  consult the routing table concurrently with the io_context
  thread.

## 10. Engine factory and lifecycle

- **`engine_class_creator`** C factory symbol for `dlopen`-based
  loading by SmartMet Server.
- **Inherits `Spine::SmartMetEngine`** — standard `init()` /
  `shutdown()` hooks.

## 11. Documentation

- **`README.md`** — overview + API description.
- **`docs/`** — additional documentation.

## 12. Build & integration

- **Output**: `sputnik.so`.
- **Loaded at**: `$(prefix)/share/smartmet/engines/sputnik.so`.
- **Build**: `make` (runs `protoc` first to generate the protobuf
  sources).
- **Format**: `make format` runs clang-format (Google-based, Allman
  braces, 100-col).
- **Install**: `make install` — headers under
  `$(includedir)/smartmet/engines/sputnik/`, `.so` under
  `$(enginedir)`.
- **RPM**: `make rpm`.
- **No unit tests** — `make test` is a CI stub.
- **Linked libraries**: `smartmet-library-spine`,
  `smartmet-library-macgyver`, Boost (thread, asio, random),
  libconfig++ (`configpp`), protobuf.
- **Generated files** — `BroadcastMessage.pb.h/cpp` are not checked
  in; the Makefile regenerates them via `protoc` and `make clean`
  removes them.
- **CI**: CircleCI on RHEL 8 / RHEL 10 with the
  `fmidev/smartmet-cibase-{8,10}` Docker images.

---

*Last updated: 2026-06-01.*
