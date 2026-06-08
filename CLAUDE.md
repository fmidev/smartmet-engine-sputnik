# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**smartmet-engine-sputnik** is the cluster management engine for SmartMet Server. It implements UDP-based service discovery and load-balanced request forwarding between frontend and backend server instances. The engine operates in one of two modes: **Frontend** (discovers backends and routes requests) or **Backend** (listens for discovery requests and advertises available services).

Part of the SmartMet Server ecosystem developed by the Finnish Meteorological Institute (FMI). See the parent workspace `CLAUDE.md` at `~/hub/CLAUDE.md` for full ecosystem context.

## Build Commands

```bash
make                # Build sputnik.so (requires protoc for BroadcastMessage.proto)
make clean          # Clean build artifacts including generated protobuf files
make format         # clang-format (Google-based, Allman braces, 100-col)
make rpm            # Build RPM package (runs protoc, then tar + rpmbuild)
make install        # Install sputnik.so to enginedir, headers to includedir/smartmet/engines/sputnik/
```

There are no unit tests in this engine. The `make test` target is a CI stub that passes only when `$CI=true`.

### Protobuf code generation

`BroadcastMessage.proto` at the repo root defines the UDP message format. Generated files (`sputnik/BroadcastMessage.pb.h`, `sputnik/BroadcastMessage.pb.cpp`) are not checked in. The Makefile `protoc` target generates them. `Engine.o` and `Messages.o` depend on the generated sources.

### Dependencies

Requires `smartmet-library-spine`, `smartmet-library-macgyver`, Boost (thread, asio, random), libconfig++ (`configpp`), and protobuf. The engine links against `-lsmartmet-spine -lsmartmet-macgyver -lprotobuf -lconfig++ -lboost_thread`.

### ABI hazard: `Engine.h` inline accessors

`Engine.h` exposes members through **inline accessors** that consumers compile in directly — notably `Services& getServices() { return itsServices; }`, called on the hot path by `smartmet-plugin-frontend` (`HTTP.cpp` getService/removeBackend/queryBackendAlive). Each consumer therefore bakes in its own compiled offset of `itsServices`.

**Adding or reordering any data member before `itsServices` is an ABI break.** A consumer built against the old header then reads `itsServices` at the wrong offset and operates on a misaligned `Services` — including its `boost::shared_mutex`, which yields a corrupted lock word: a `ReadLock` in `Services::getService` that waits for a write that no thread owns, hanging the frontend at startup (downgrading the engine "fixes" it by restoring the old layout). This is exactly what adding `itsStickyCookie` (sputnik 26.5.27, sticky forwarder) did to a frontend built against 26.4.13.

When changing `Engine`'s members: treat it as an ABI change — bump the version dependents `Require:`, rebuild and repackage **all** consumers, deploy them together, and note "ABI change" in the changelog. Cheap mitigation: append new members at the **end** of the class so the inline-exposed `itsServices` offset never moves.

## Architecture

### Two operating modes

The `Engine` class (inherits `SmartMet::Spine::SmartMetEngine`) is loaded dynamically by the server via the `engine_class_creator` C entry point. It is launched in either mode by the server calling `launch(BroadcastMode, Reactor*)`:

- **Backend mode**: Binds a UDP socket to `udpListenerAddress:udpListenerPort`, listens for `SERVICE_DISCOVERY_REQUEST` messages from frontends, and replies with `SERVICE_DISCOVERY_REPLY` containing the server's URI map, load average, and admin request names. A backend will suppress replies when paused or under high load (`Reactor::isLoadHigh()`).

- **Frontend mode**: Sends periodic UDP discovery requests to configured `backendUdpListeners`, collects replies within a `heartbeat.timeout` window, and maintains a `Services` object that maps URIs to available backends. Stale backends are pruned after `heartbeat.max_skipped_cycles` consecutive unanswered heartbeats. The frontend also registers a `BackendConnectionFinishedHook` with the Reactor to track backend liveness from HTTP response completions.

Both modes run an async Boost.Asio io_context on a dedicated thread.

### Service routing and load balancing

`Services` is the central routing table on the frontend. It maps each URI to a list of `BackendService` entries plus a `BackendForwarder` that selects which backend handles each request. Seven forwarding strategies are available (configured via `forwarding` in sputnik.conf):

| Config value | Class | Algorithm |
|---|---|---|
| `random` | `RandomForwarder` | Uniform random selection |
| `doublerandom` | `DoubleRandomForwarder` | Pick two random, choose the better one |
| `inverseload` | `InverseLoadForwarder` | Weight by `1/(1 + a*load)` |
| `inverseconnections` | `InverseConnectionsForwarder` | Weight by `1/(1 + a*connections)` |
| `leastconnections` | `LeastConnectionsForwarder` | Pick the backend with fewest active connections |
| `exponentialconnections` | `ExponentialConnectionsForwarder` | Exponential weighting by connection count |
| `sticky` | `StickyForwarder` | Session affinity via rendezvous (HRW) hashing, with active-connection hotspot exclusion |

The first six forwarders extend `BackendForwarder` and override `redistribute()` (called on backend list changes) and optionally `rebalance()` (called per-request); the `balance_factor` config parameter controls the `a` coefficient.

`StickyForwarder` instead overrides `getBackend()` directly (which now also receives the `Spine::HTTP::Request`). It keys each request on a client-set affinity cookie (`sticky_cookie`, default `smartmet-session-id`) when present, otherwise on the effective client IP (X-Forwarded-For or peer) combined with the User-Agent, and maps the key to a backend with rendezvous hashing. Backends whose active connections exceed `balance_factor * min_connections + slack` are excluded from selection.

### URI prefix routing

`URIPrefixMap` supports backends that register URI prefixes (via the `is_prefix` flag in the protobuf `Service` message). When the frontend receives a request, it first checks for a matching prefix before falling back to exact URI lookup.

### Backend health tracking

- `BackendSentinel`: Per-backend throttling tracker. Counts unanswered connections; the backend is considered unresponsive if the throttle limit is exceeded.
- Sequence-number based cleanup: Backends that don't respond to a heartbeat cycle get their services pruned from the routing table.
- If all backends disappear, `Services::removeBackend` issues `SIGKILL` to force a fast restart.

> **Intentional backpressure (don't "fix" it):** the frontend calls `Services::removeBackend` when a backend replies `1234` high-load (`smartmet-plugin-frontend` `HTTP.cpp`). This globally retires the busy backend for ~2-3s; the discovery heartbeat re-adds it as soon as it resumes announcing (an overloaded backend also suppresses its own reply, see Backend mode). This sheds load instead of bouncing requests, and it is also what keeps the deterministic `sticky` forwarder from looping on one backend (the just-denied backend is gone from the set on the next pick). So do **not** remove the retire-on-`1234` behavior, and do not add a per-request "excluded" set to `getBackend`.
>
> **Known defect:** the `SIGKILL`-on-empty above does not distinguish "backend genuinely dead" (`queryBackendAlive` false) from "backend transiently high-load". Under cluster-wide high load the HTTP path can retire the last backend on a `1234` and kill the frontend, whereas the heartbeat path handles the same state gracefully (404s, no suicide). The fix is to `SIGKILL` only for genuine-dead retirements.

### Pause/resume

The engine supports pause/resume via `setPause()`, `setPauseUntil(deadline)`, and `setContinue()`. When paused, the backend does not respond to discovery requests. Pause can also be set at startup via the `pause` config parameter.

## Configuration

Configuration uses libconfig++ format. See `cnf/sputnik.conf.sample` for a complete example. Key parameters:

**Frontend**: `backendUdpListeners` (required), `frontendUdpAddress`, `frontendUdpPort`, `forwarding`, `balance_factor`, `sticky_cookie`, `heartbeat.interval`, `heartbeat.timeout`, `heartbeat.max_skipped_cycles`

**Backend**: `hostname`, `httpAddress`, `udpListenerAddress`, `udpListenerPort`, `comment`, `throttle`, `pause`

The backend HTTP port (`httpPort`) is read from the Reactor configuration, not from sputnik.conf.

## Key namespaces

- `SmartMet::Engine::Sputnik` — the engine itself
- `SmartMet` — service management classes (`Services`, `BackendForwarder`, `BackendServer`, `BackendService`, `BackendSentinel`, `BackendInfoRequest`, `URIPrefixMap`)
