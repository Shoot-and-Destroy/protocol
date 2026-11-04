# The server list and how to join

A client needs to find servers and open one connection. This document covers
both: the HTTP list our master serves, the classic 0.75 list, the UDP
queries that fetch a server's details, the WebRTC signalling a browser uses,
join codes, and the connect token our servers ask for.

It applies to both protocols. The list's `game_version` says which one a row
speaks.

---

## 1. The two kinds of list

| List | Address | Shape | Who is on it |
| --- | --- | --- | --- |
| Ours | `https://master.shootanddestroy.obex.pw/v1/servers` | JSON array, one row per server, plus the WebRTC signalling below | Shoot and Destroy servers, 1.0 by default, 0.75 when a server announces that profile |
| Classic | `https://services.buildandshoot.com/serverlist.json` (and other 0.75 masters) | JSON array, `serverlist.json` | public 0.75 servers |

A client should read both and **merge by address**, ours winning: a server
listed twice appears once, as ours. Our row carries the classic keys with
the same meanings, so one parser reads both; ours adds `id`, `key`,
`identifier6`, `webrtc`, `dtls_fingerprint`, `info` and `ice_servers`.

Empty lists are a normal condition (a master moved, a network blocks it).
Say so in the UI instead of showing "0 servers".

---

## 2. Our list: `GET /v1/servers`

An array, the most players first:

```json
[{
  "name": "Alpha",
  "identifier": "aos10://203.0.113.5:32887",
  "map": "hallway",
  "game_mode": "ctf",
  "country": "BR",
  "players_current": 3,
  "players_max": 32,
  "game_version": "1.0",
  "identifier6": "aos10://[2001:db8::5]:32887",
  "id": "9f86d081884c7d65",
  "key": "<base64 Ed25519 public key>",
  "webrtc": true,
  "dtls_fingerprint": "sha-256 AB:CD:...",
  "info": { "kind": "info", "protocol": "1.0", "...": "..." },
  "ice_servers": [
    { "urls": "stun:stun.example.net:3478" },
    { "urls": "turn:turn.example.net:3478", "username": "u", "credential": "p" }
  ],
  "last_seen": 1792500000
}]
```

| Key | Meaning |
| --- | --- |
| `name` | shown name, 1..64 bytes |
| `identifier` | the address to join: IPv4 when the server has one, else IPv6 |
| `identifier6` | the IPv6 address when there is one |
| `map`, `game_mode` | at most 32 and 16 bytes |
| `players_current`, `players_max` | 0..255; bots count as players |
| `game_version` | `"1.0"` or `"0.75"` |
| `id`, `key` | the server's identity on our master (needed for the WebRTC offer) |
| `webrtc` | true when the server takes browser players |
| `dtls_fingerprint` | the server's self-signed DTLS certificate, as SDP shows it |
| `info` | the server's `HELLOINFO` answer (section 5), when it has one; **may be absent** |
| `ice_servers` | STUN/TURN to use for this server (`urls`, `username`, `credential`) |
| `last_seen` | Unix seconds |

Rows are **not** permanent: a server that stops announcing disappears within
90 s. Fetch the list when the player opens the server browser and on Refresh;
do not poll faster than every 10 s. `GET /v1/servers/{id}` is one row.

A row's `game_version` decides the protocol for a plain ENet join: `1.0` →
`aos10://`, connect magic; `0.75` → `aos://`, connect data 3. A client that
speaks both should still try 1.0 first, then 0.75, then 0.76.

### 2.1 Our 1.0 addresses

```
aos10://host:port
aos10://[2001:db8::5]:32887
```

The `aos10://` scheme is ours; it means protocol 1.0. A bare `host:port` and
a plain IPv4 are also valid input (the protocol attempt order then decides).
Port defaults to 32887.

Our master serves an **IPv4** identifier when the server has both families,
and an IPv6 one when it has only IPv6. A client on an IPv6-only machine must
use `identifier6`; on a dual-stack machine use `identifier`. Probe once at
start-up whether the machine has an IPv4 route (a `connect()` on a UDP socket
to a public address, sending nothing) and prefer IPv4 while it works.

### 2.2 The classic 0.75 list

`GET .../serverlist.json` is the array of section 1 with at least:

| Key | Example |
| --- | --- |
| `name` | `"Deuces Wild"` |
| `identifier` | `"aos://16777343:32887"` |
| `map` | `"Hallway"` |
| `game_mode` | `"ctf"` |
| `country` | `"US"` |
| `latency` | `120` (ms, measured by the master) |
| `players_current`, `players_max` | `16`, `32` |
| `last_updated` | `1301931731` |
| `game_version` | `"0.75"` |

Ignore unknown keys and expect duplicate entries. The `identifier` is the
0.75 address form: see section 1.2 of [`0.75.md`](0.75.md).

---

## 3. Addressing recap

| Form | Meaning |
| --- | --- |
| `aos://<word>:<port>` | 0.75/0.76; `<word>` is the IPv4 address as a little-endian integer (`127.0.0.1` = `16777343`) |
| `aos://1.2.3.4:32887` | the same, dotted |
| `aos://[2001:db8::5]:32887` | IPv6 (our servers only) |
| `aos10://1.2.3.4:32887` | protocol 1.0 |
| `ABC-DEF` | a friend's join code (section 6) |

The 0.75 word is `a | b << 8 | c << 16 | d << 24` for `a.b.c.d`:
`127.0.0.1` is `16777343` (`0x0100007F`).

---

## 4. LAN discovery

A server answers `HELLOLAN` (8 raw bytes over UDP) only to a local address:

```json
{"name":"Alpha","players_current":3,"players_max":32,"map":"Raindance",
 "map_shape":"<64 hex>","game_mode":"ctf","game_version":"1.0"}
```

Send it as a UDP broadcast and to `127.0.0.1` on ports **32882..32897** when
the player opens the list; list every answer with a LAN flag. It is also the
only way to find a server that has no master or no internet.

---

## 5. Server details over UDP: `HELLOINFO`

Our servers answer a raw UDP query with a JSON description, on the game's
port. The row's `info` is the same object.

| Direction | Bytes |
| --- | --- |
| client | `"HELLOINFO"` **padded to 1200 bytes** (any byte after the word) |
| server | the JSON object, a few KB |

**Padding is a rule, not a nicety.** UDP proves no address: an answer to a
forged source is an amplifier. The server budgets 5 padded queries a second
per address (burst 10), 0.5/s for the bare 9-byte word (burst 3), and answers
a bare query too. A client that sends the bare word gets throttled; pad it.

Answer fields (`kind` is always `"info"`):

| Key | Meaning |
| --- | --- |
| `name`, `map`, `game_mode`, `package` | strings |
| `map_shape` | SHA-256 of the map's solid cells, 64 hex digits; identifies the map however the file is named |
| `map_preview` | byte length of the map's top-view picture, 0 when there is none |
| `protocol` | `"1.0"` or `"0.75"` (protocol revision for 1.0) |
| `players_max`, `players_current`, `bots`, `humans` | counts |
| `teams` | `[{name, score, players}, ...]` |
| `score_limit` | decimal |
| `features` | `{perks, classes, assists, spotting, squad_spawn, aim_assist}` booleans |
| `classes` | class titles, when the match has classes |
| `rotation` | up to 32 map names |
| `plugins` | comma-separated plugin names |
| `players` | up to 32 `{name, team, kills, deaths, bot, ping}`, best by kills |

Show it in the details panel. A 0.75/0.76 server of ours answers `protocol:
"0.75"`; a third-party 0.75 server does not answer at all — do not ask it
(the panel then shows the list row alone).

### 5.1 The map preview

The details page can show the map's top view. It is a 256 x 256 RGB picture,
zlib-compressed, framed as:

```
"SDP1" | u16 width | u16 height | zlib(RGB rows)     (row 0 is map y 0)
```

The whole stream is length `info.map_preview`, identified by `map_shape`. Ask
for it in pieces:

```
client: "HELLOPREVIEW" | u32 offset | padding to 1200 bytes
server: "SDPC" | u32 total | u32 offset | 64 hex map_shape | up to
        (query size - 72) bytes of the stream
```

The answer is never larger than the query (which is why the query is padded).
Keep up to 8 chunks in flight, ask again every 0.5 s, and give up after 3 s
**without any chunk** (a slow link is not a failure). Check `SDP1`, the size
and the inflate before showing it; cache by `map_shape`. There is no preview
from a 0.75/0.76 server and none in a browser build (a page cannot send UDP).

---

## 6. Joining from a browser: WebRTC

Only protocol 1.0 goes over WebRTC. The browser opens two DataChannels and
trades one SDP offer/answer through our master; after that it is a normal 1.0
connection ([`1.0.md`](1.0.md) section 2.2).

### 6.1 The flow

```
browser                              master                       game server
  open "reliable" (defaults)
  open "unreliable" ({ordered:false, maxRetransmits:0})
  createOffer(); wait iceGatheringState == "complete"
  POST /v1/servers/{id}/offer  ------>  long poll  ------------>  (the server has one poll waiting)
      {"sdp": "<offer>"}                              <--------  POST /v1/answer {"sdp": "<answer>"}
  <---------------------------- 200 {"sdp": "<answer>"}
  setRemoteDescription(answer)
  on both channels open:
      reliable.send("connect 135272729")     (text, the 1.0 connect magic in decimal)
      binary messages = one 1.0 packet each
```

The server sends text `disconnect <reason>` and closes; the browser closing
its channels is the disconnect.

Rules:

* **Do not trickle ICE.** Wait for `iceGatheringState == "complete"` before
  posting: the answer carries every candidate of the server, and the server
  answers only once its own gathering is complete.
* **Send `connect <n>` before `InitiateConnection`.** A binary message that
  arrives before the text is dropped, and the handshake then times out. Send
  the connect text first, then start the protocol.
* A binary message is one 1.0 packet, **at most 16 KB** (the map's fragments
  are 8 KB).
* The label `reliable` maps to ENet channel 0, `unreliable` to channel 1.
  Any other label is closed. The server sends everything on `reliable` and
  reads both.

### 6.2 The endpoints

**Offer by server id:**

```
POST https://master.shootanddestroy.obex.pw/v1/servers/{id}/offer
Content-Type: application/json
{"sdp": "<SDP offer>"}                       (at most 32 KB)
```

| Status | Body | When |
| --- | --- | --- |
| 200 | `{"sdp": "<answer>"}` | the server answered |
| 400 | `{"error": ...}` | no `sdp` |
| 404 | | no such server |
| 409 | | the server takes no WebRTC (`webrtc: false`) |
| 429 | `Retry-After: 60` | more than 10 offers a minute from one address |
| 503 | `{"error": "<reason>"}` | the server rejected it, or 32 offers are waiting |
| 504 | | no answer within the offer timeout |

Every response carries `Access-Control-Allow-Origin: *`, so a page can call
it directly.

**Join code:** `GET /v1/join/{code}` returns the row of the server holding
the code (404 `{"error": "nobody is playing under this code now"}` when none
does); `POST /v1/join/{code}/offer` is the offer above, by code.

### 6.3 Join codes (playing with a friend, no port forwarding)

A listen server a player starts from the game announces a six-character code
(shaped `ABC-DEF`, with no `I`, `L`, `O`, `Q`, `0` or `1`). A friend types
the code in the address box (or starts the game with it); the client reads
`GET /v1/join/{code}` and makes the WebRTC offer to that server. The UDP port
is never forwarded.

For a client implementation: treat a string matching `^[A-Za-z]{3}[ -]?[A-Za-z0-9]{3}$`
or starting with `sadcode://` as a **code**, not a hostname, and join it over
WebRTC. A six-letter plain hostname exists in theory; codes are the trade-off
we accepted.

### 6.4 Errors a player can read

"Nobody is playing under that code", "The host refused the connection",
"The host is full", "This server does not take browser players". Never show
"ICE", "SDP", "NAT" or "WebRTC" to a player.

---

## 7. The connect token (1.0 over UDP, ours)

Our server admits a UDP player from a **non-local** address only with a token
from the master, so a join flood is rate-limited there. After the Greeting:

```
POST https://master.shootanddestroy.obex.pw/v1/token
{"address": "203.0.113.5:32887", "nonce": "<base64 of the Greeting's 128 bytes>"}
-> {"certificate": "<base64>", "signature": "<base64>"}
```

Send that as the `ClientCertificate` with `isValid = 1`.

The certificate is `"SADT"`, version `u8` 1, the server's Ed25519 public key
(32), expiry `u64` little-endian (Unix seconds, 60 s on), the nonce's length
(`u8`) and the nonce. The signature is Ed25519 by the master over the
certificate.

`404` when no server is listed at that address, `429` over the limit. Never
cache a token: one token is one connection. Not needed for a local address,
for WebRTC (the offer went through the master), or while the server has no
token key.

---

## 8. Announcing a server

Only needed if you run a server of your own and want it listed.

Every call is a **POST of a signed envelope**:

```json
{"key": "<base64 Ed25519 public key>",
 "body": "<JSON text>",
 "sig": "<base64 Ed25519 signature over body's bytes>"}
```

The body is a JSON object as a **string**, so the signature covers its exact
bytes. Every body has `type` and `time` (Unix **milliseconds**). The master
refuses a body more than 5 minutes off its clock, a body it has seen before
(replay), and a type that is not the endpoint's. The server's key is its
identity: its `id` is the first 16 hex digits of the key's SHA-256.

### 8.1 `POST /v1/announce`

Body fields: `type`, `time`, `name`, `port`, `map`, `mode`,
`players_current`, `players_max`, `country`, `webrtc_port`,
`dtls_fingerprint`, `info` (the `HELLOINFO` object), `protocol` (`"1.0"` or
`"0.75"`), `ice_servers`, and optionally `join_code` and `unlisted`.

Answer: `{"id", "identifier", "stale_after", "token_key"}`.

* The listed address is the **request's own source address**, never one in
  the body. A server with IPv4 and IPv6 announces once over each.
* The master probes the UDP port with `HELLOINFO` and refuses an unreachable
  server with `502`.
* Announce at start, every 30 s and on a change; address, players and map
  changes are picked up. `{"type": "leave"}` unlists at shutdown.
* `join_code` needs `webrtc_port`; a code another server holds is refused
  with `409` (pick a fresh one). `unlisted` keeps the row off the public list
  and skips the UDP probe.
* Errors: 400 malformed/out of range, 401 signature/time/replay, 403 key not
  allowed, 404 not listed, 410 a stale answer, 429 too many servers from one
  address.

### 8.2 `POST /v1/poll` and `POST /v1/answer`

A server with `webrtc_port` long-polls `{"type":"poll"}`; the master holds it
up to 25 s and answers `{"offers":[{"id","sdp","ip"}]}` as soon as one
arrives. Within 10 s answer with `{"type":"answer","offer":"<id>","sdp":...}`
or `{"type":"reject","offer":"<id>","reason":"server full"}`. Poll again at
once, one poll at a time.

---

## 9. Client checklist

1. Read the list over HTTPS, with a timeout (5 s) and a retry (once), and
   handle "not JSON" (a captive portal, a parked domain) as unreachable.
2. Merge our list with the classic 0.75 list by address, ours winning.
3. Filter empty/full servers only if the player asked; keep the chosen row
   selected when the list refreshes.
4. Join a row: `aos10://` → 1.0 over ENet; `aos://` → 0.75; a code → WebRTC.
5. Ask `HELLOINFO` (padded) for the details panel and `HELLOLAN` on the local
   network; both are optional.
6. In a browser, only our 1.0 rows are joinable, and only when
   `webrtc: true`; treat the rest as "This server does not take browser
   players".
7. Fetch a connect token when the server asks (a `Kick` says so), and retry
   the join once after getting it.
8. Never show protocol jargon to a player.
