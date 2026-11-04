This page documents the raw UDP protocol of a server: the classic ping and
LAN discovery, and our `HELLOINFO` (server details) and map preview. No ENet
and no packet id: the first bytes of the datagram are the packet.

# Connection

Open a raw UDP socket and send the datagram to the server's game port
(`32887` by convention). A server answers with a datagram to the source
address.

UDP proves nobody's address, so a query is also a way to make a server send
its answer to a forged victim. Our server therefore budgets answers per
source address, and every answer is no larger than its query where it
matters. **Pad `HELLOINFO` to 1200 bytes**: the padded query gets 5 answers a
second (burst 10) per address, the bare 9-byte word only 0.5 (burst 3).

# Packets

## Ping Start
`Client -> Server`

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| 5 bytes | `HELLO` | String | fixed length, never send anything more |

## Ping Acknowledge
`Server -> Client`

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| 2 bytes | `HI` | String | fixed length |

The round trip is the ping; halve it yourself.

## Ping LAN
`Client -> Server`

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| 8 bytes | `HELLOLAN` | String | fixed length |

A server answers only to a local address (10/8, 172.16/12, 192.168/16,
127/8, 169.254/16, `::1`, fc00::/7, fe80::/10). Send it as a broadcast and to
`127.0.0.1` on ports `32882`..`32897` to find servers on the local network.

## LAN Info
`Server -> Client`

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| variable | see below | String | JSON |

| Param | Notes |
| --- | --- |
| `name` | server name |
| `players_current` | players, bots included |
| `players_max` | slots |
| `map` | map name |
| `map_shape` | 64 hex digits identifying the map |
| `game_mode` | mode name |
| `game_version` | `"1.0"` or `"0.75"` |

```json
{"name":"Alpha","players_current":3,"players_max":32,"map":"Raindance",
 "map_shape":"<64 hex>","game_mode":"ctf","game_version":"1.0"}
```

## Info Query
`Client -> Server`

`HELLOINFO`, **padded to 1200 bytes** with any bytes after the word.

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| 9 bytes | `HELLOINFO` | String | bare: heavily rate limited |
| 1200 bytes | `HELLOINFO` + padding | String | the normal form |

## Info Answer
`Server -> Client`

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| variable | see below | String | JSON, a few KB |

`kind` is always `"info"`.

| Param | Notes |
| --- | --- |
| `name`, `map`, `game_mode`, `package` | strings |
| `map_shape` | SHA-256 of the map's solid cells, 64 hex digits |
| `map_preview` | byte length of the top-view picture, `0` when there is none |
| `protocol` | `"1.0"` or `"0.75"` |
| `players_max`, `players_current`, `bots`, `humans` | counts |
| `teams` | `[{name, score, players}, ...]` |
| `score_limit` | decimal |
| `features` | `{perks, classes, assists, spotting, squad_spawn, aim_assist}` booleans |
| `classes` | class titles, when the match has classes |
| `rotation` | up to 32 map names |
| `plugins` | comma-separated names |
| `players` | up to 32 `{name, team, kills, deaths, bot, ping}`, best by kills |

A 0.75/0.76 server of ours answers `protocol: "0.75"`; a third-party 0.75
server does not answer at all — do not ask it.

## Map Preview
`Client -> Server`

The details panel can show the map's top view. It is a 256 x 256 RGB picture,
zlib-compressed, framed as:

```
"SDP1" | Uint16 width | Uint16 height | zlib(RGB rows)
```

`row 0` is map `y` 0. `info.map_preview` is the length of that stream and
`info.map_shape` identifies it.

Ask for it in pieces:

| Length | Value | Field Type | Notes |
| --- | --- | --- | --- |
| 1200 bytes | `HELLOPREVIEW` + `Uint32 offset` + padding | String + Uint32 | the query must be padded; a short one is refused |

## Map Preview Answer
`Server -> Client`

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Magic | 4 bytes | `"SDPC"` |
| Total | Uint32 | the whole stream's length |
| Offset | Uint32 | echoed |
| Shape | 64 bytes | the map's shape, hex, no NUL |
| Data | Uint8 array | up to `query size - 72` bytes of the stream |

The answer is never larger than the query, so a forged source amplifies
nothing. Keep up to 8 chunks in flight, ask again every 0.5 s, and give up
after 3 s **without a chunk** (a slow link is not a failure). Check `SDP1`,
the size and the inflate before showing it, and cache by `map_shape`. A
browser cannot send UDP, so a web client has no preview.

# Rate Limits

| Query | Answers a second | Burst |
| --- | --- | --- |
| `HELLOINFO`, padded | 5 | 10 |
| `HELLOINFO`, bare | 0.5 | 3 |
| `HELLOLAN` | 5 | 10 |
| `HELLOPREVIEW` | 40 | 40 |
| any answer the server sends | 200 | 10 |

Refusals are counted and logged once a second
(`EVT net action=query_refused why=rate|rate_short|not_lan|preview_short
count=`).

# Other Resources

* [Master Server](protocolmaster.md) — where a client finds the addresses.
* [Protocol 0.75](protocol075.md) and [Protocol 1.0](protocol10.md).
