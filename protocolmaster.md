This page documents the server lists a client reads and a server announces
itself to: the classic 0.75 master over ENet, and our HTTP master, which also
carries the WebRTC signalling for browsers and the connect tokens.

# Connection

## Classic Master (0.75)

The classic master is an ENet server. Connect like a game server, with
connect data `31`:

| Number | Meaning |
| --- | --- |
| 31 | the classic master protocol |

Send the request to `master.buildandshoot.com` on port `32886`, with ENet 1.3
and the range coder.

## Our Master (HTTP)

Everything is plain HTTPS and JSON. Every response carries
`Access-Control-Allow-Origin: *`, so a page can call it.

```
https://master.shootanddestroy.obex.pw
```

A server announces itself over IPv4 and IPv6 separately; a client reads the
list and joins either. The list is also the WebRTC signalling relay, so a
browser can join a server whose UDP port is not reachable.

# Packets

## Classic: Major Update
`Server -> Master`

Sent on start-up or after a map change. The master assumes the player count
resets to zero. There is no packet id.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Max Players | Uint8 | |
| Port | Uint16 | |
| Name | CP437 String | at most 31 bytes |
| Game Mode | CP437 String | at most 7 bytes |
| Map | CP437 String | at most 20 bytes |

## Classic: Count Update
`Server -> Master`

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player Count | Uint8 | sent whenever it changes |

## Our Master: `GET /v1/servers`
`Client <- Master`

An array, the most players first. Our rows carry the classic keys with the
same meanings, so one 0.75 parser reads both.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| `name` | String | 1..64 bytes |
| `identifier` | String | the address to join: IPv4 when present, else IPv6 |
| `identifier6` | String | the IPv6 address when there is one |
| `map`, `game_mode` | String | at most 32 and 16 bytes |
| `players_current`, `players_max` | Number | 0..255; bots count |
| `game_version` | String | `"1.0"` or `"0.75"` |
| `id`, `key` | String | the server's identity on our master |
| `webrtc` | Boolean | true when the server takes browser players |
| `dtls_fingerprint` | String | the server's DTLS certificate, as SDP shows it |
| `info` | Object | the `HELLOINFO` answer, may be absent |
| `ice_servers` | Array | STUN/TURN for this server (`urls`, `username`, `credential`) |
| `last_seen` | Number | Unix seconds |

Example row:

```json
{
  "name": "Alpha",
  "identifier": "aos10://203.0.113.5:32887",
  "identifier6": "aos10://[2001:db8::5]:32887",
  "map": "hallway",
  "game_mode": "ctf",
  "country": "BR",
  "players_current": 3,
  "players_max": 32,
  "game_version": "1.0",
  "id": "9f86d081884c7d65",
  "webrtc": true,
  "dtls_fingerprint": "sha-256 AB:CD:...",
  "ice_servers": [{"urls": "stun:stun.example.net:3478"}],
  "last_seen": 1792500000
}
```

Rows are not permanent: a server that stops announcing disappears within
90 s. Fetch the list when the player opens the browser and on Refresh; do not
poll faster than every 10 s. `GET /v1/servers/{id}` is one row.

A row's `game_version` decides the protocol for a plain ENet join: `1.0` is
`aos10://` with the 1.0 magic, `0.75` is `aos://` with connect data 3.

A client on an IPv6-only machine uses `identifier6`; on a dual-stack machine
use `identifier`.

### The Classic List Format

`GET /serverlist.json` on a classic master is the same array with at least:

| Key | Example | Notes |
| --- | --- | --- |
| `name` | `"Deuces Wild"` | |
| `identifier` | `"aos://16777343:32887"` | |
| `map` | `"Hallway"` | |
| `game_mode` | `"ctf"` | |
| `country` | `"US"` | ISO 3166-1 alpha-2 |
| `latency` | `120` | ms, measured by the master |
| `players_current`, `players_max` | `16`, `32` | |
| `last_updated` | `1301931731` | Unix seconds |
| `game_version` | `"0.75"` | |

Merge both lists by address, ours winning: a server listed twice appears
once, as ours.

## Our Master: `POST /v1/announce`
`Server -> Master`

Every call is a **signed envelope**:

```json
{"key": "<base64 Ed25519 public key>",
 "body": "<JSON text>",
 "sig": "<base64 Ed25519 signature over body's bytes>"}
```

`body` is a JSON object as a string, so the signature covers its exact bytes.
Every body has `type` and `time` (Unix **milliseconds**). The master refuses
a body more than 5 minutes off its clock, a body it has seen before (replay),
and a type that is not the endpoint's.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| `type`, `time` | String, Number | `"announce"` |
| `name` | String | 1..64 bytes |
| `port` | Number | the game's UDP port |
| `map`, `mode` | String | at most 32 and 16 bytes |
| `players_current`, `players_max` | Number | |
| `country` | String | at most 3 bytes |
| `webrtc_port` | Number | the ICE UDP mux port; 0 or absent = no browsers |
| `dtls_fingerprint` | String | as in SDP |
| `info` | Object | the `HELLOINFO` answer |
| `protocol` | String | `"1.0"` (default) or `"0.75"` |
| `ice_servers` | Array | at most 8 |
| `join_code` | String | optional, six unambiguous characters |
| `unlisted` | Boolean | keep the row off the public list |

Answer: `{"id", "identifier", "stale_after", "token_key"}`.

The listed address is the **request's own source address**, never one in the
body. The master probes the UDP port with `HELLOINFO` and refuses an
unreachable server with `502`. Announce at start, every 30 s and on a change;
`{"type": "leave"}` unlists at shutdown.

Errors: `400` malformed or out of range, `401` signature/time/replay, `403`
key not allowed, `404` not listed, `410` a stale answer, `429` too many
servers from one address.

## Our Master: `POST /v1/poll` and `POST /v1/answer`
`Server <-> Master`

A server with `webrtc_port` long-polls `{"type": "poll"}`; the master holds
it up to 25 s and answers:

```json
{"offers": [{"id": "<offer id>", "sdp": "<SDP offer>", "ip": "<browser's address>"}]}
```

Within 10 s answer with `{"type": "answer", "time": ..., "offer": "<id>",
"sdp": "<SDP answer>"}` or `{"type": "reject", "offer": "<id>", "reason":
"server full"}`. Poll again at once, one poll at a time.

## Our Master: `POST /v1/servers/{id}/offer`
`Browser -> Master -> Server`

The browser's way in.

```json
{"sdp": "<SDP offer>"}
```

| Status | Body | When |
| --- | --- | --- |
| 200 | `{"sdp": "<answer>"}` | the server answered |
| 400 | `{"error": ...}` | no `sdp` |
| 404 | | no such server |
| 409 | | the server takes no WebRTC |
| 429 | `Retry-After: 60` | more than 10 offers a minute from one address |
| 503 | `{"error": "<reason>"}` | the server rejected it, or 32 offers are waiting |
| 504 | | no answer within the offer timeout |

The flow:

```
browser                              master                       game server
  open "reliable" (defaults)
  open "unreliable" ({ordered:false, maxRetransmits:0})
  createOffer(); wait iceGatheringState == "complete"
  POST /v1/servers/{id}/offer  ------>  long poll  ------------>  (one poll waiting)
      {"sdp": "<offer>"}                              <--------  POST /v1/answer {"sdp": ...}
  <---------------------------- 200 {"sdp": "<answer>"}
  setRemoteDescription(answer)
  on both channels open:  reliable.send("connect 135272729")   (text)
  binary messages = one 1.0 packet each
```

Do not trickle ICE: wait for `iceGatheringState == "complete"` before
posting. Send `connect <n>` **before** `Initiate Connection`, or the
handshake times out. The server ends with the text `disconnect <reason>`.

## Our Master: Join Codes
`Client -> Master`

A listen server announces a six-character code (`ABC-DEF`, with no `I`, `L`,
`O`, `Q`, `0` or `1`). A friend types it in the address box; the client joins
over WebRTC and the host forwards no port.

| Call | Meaning |
| --- | --- |
| `GET /v1/join/{code}` | the row of the server holding the code; `404` `{"error": "nobody is playing under this code now"}` when none does |
| `POST /v1/join/{code}/offer` | the offer above, by code |

The code is given as pasted: `abc-def`, `ABC DEF` and `ABCDEF` are the same.

## Our Master: `POST /v1/token`
`Client -> Master`

Our server admits a UDP player from a **non-local** address only with a token
from the master, so a join flood is rate-limited there. After the Greeting:

```json
{"address": "203.0.113.5:32887", "nonce": "<base64 of the Greeting's 128 bytes>"}
```

Answer: `{"certificate": "<base64>", "signature": "<base64>"}`, sent as the
`Client Certificate` with `isValid = 1`:

| Part | Bytes |
| --- | --- |
| Certificate | `"SADT"`, version `Uint8` 1, the server's Ed25519 public key (32), expiry `Uint64` little-endian (Unix seconds, 60 s on), nonce length `Uint8`, the nonce |
| Signature | Ed25519 by the master over the certificate (64) |

`404` when no server is listed at that address, `429` over the limit. Never
cache a token: one token is one connection. Not needed for a local address,
for WebRTC (the offer went through the master), or while the server has no
token key.

# Client Checklist

1. Read the list over HTTPS with a timeout (5 s) and a retry (once); treat
   "not JSON" as unreachable.
2. Merge our list with the classic list by address, ours winning.
3. Keep the selected row when the list refreshes.
4. Join a row: `aos10://` is 1.0 over ENet, `aos://` is 0.75, a code is
   WebRTC.
5. Ask [`HELLOINFO`](protocolping.md) for the details panel.
6. In a browser only our 1.0 rows are joinable, and only when
   `webrtc: true`.
7. Fetch a connect token when the server asks, and retry the join once.
8. Never show protocol jargon to a player.

# Other Resources

* [Protocol 0.75](protocol075.md) and [Protocol 1.0](protocol10.md).
* [Ping](protocolping.md) — `HELLO`, `HELLOLAN`, `HELLOINFO`, map previews.
