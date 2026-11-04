# Shoot and Destroy — wire protocols

The network protocols of **Ace of Spades** and its revivals, written so a
person or an AI agent can implement them in another game:

| Document | What it covers |
| --- | --- |
| [`0.75.md`](0.75.md) | Ace of Spades 0.75 and 0.76, the classic protocol every public server speaks (and BetterSpades, OpenSpades, the Voxlap client). |
| [`1.0.md`](1.0.md) | Protocol 1.0, the protocol of the Shoot and Destroy server. It starts from OpenSpades' abandoned `ngspades` branch ("WorldOfSpades 0.1") with its defects fixed and the packets a whole game needs added. |
| [`master.md`](master.md) | The server list: how a client finds servers, how a server announces itself, WebRTC signalling for browsers, join codes, connect tokens, LAN discovery and map previews. |
| [`examples/c/`](examples/c/) | Dependency-light C99: the 1.0 primitive codec and packet encoders, the 0.75 packed structs, an ENet connection, and the master list. |
| [`examples/godot/`](examples/godot/) | Godot 4 GDScript: the same codecs, the master list and a WebRTC client that also runs in a web export. |

Everything here describes bytes on the wire. It does not describe our
game's rules beyond what the protocol needs.

---

## 1. Which protocol do I implement?

* **Your game joins public Ace of Spades servers** → implement **0.75**
  (`0.76` is a small variation, section 1 of [`0.75.md`](0.75.md)).
* **Your game joins Shoot and Destroy servers, or you want browser
  players** → implement **1.0**. A browser can only reach 1.0, over WebRTC.
* **Your game is a server that old clients join** → implement the 0.75
  server role ([`0.75.md`](0.75.md) section 12).
* **Your game is a server everyone joins** → implement 1.0 (ENet on
  desktop, WebRTC in a browser).

A single client can speak both: try 1.0 first, fall back to 0.75, then
0.76. That is what our client does and what section 2 of
[`1.0.md`](1.0.md) describes.

Protocols are never mixed inside one connection. The protocol is chosen at
ENet connect time from the 32-bit connect data:

| Connect data | Protocol |
| --- | --- |
| `3` | 0.75 |
| `4` | 0.76 |
| `0x08101919` (135272729) | 1.0 |

---

## 2. The rules that apply to everything

These are short, and each one has cost somebody hours.

1. **Little-endian, packed, no padding.** Every integer is little-endian;
   signed integers are two's complement. Structures on the wire have no
   padding between fields. In C use `#pragma pack(push, 1)`.
2. **One game packet per transport message.** The first byte is the packet
   id, the rest is the payload.
3. **Check the length before reading.** A short packet is malformed, not a
   packet with zeroes in the missing bytes.
4. **Ignore unknown packet ids and unknown keys.** The protocols grow
   without changing their name. An unknown id is not a reason to
   disconnect; log it and drop it.
5. **The server is authoritative about damage, scores and the map.** A
   client reports what it saw (the shot that hit, the block it broke) and
   the server decides.
6. **`z` grows downwards.** The map is `512 x 512 x 64`; `z = 0` is the
   sky, `z = 63` is the bottom water layer. A player position is its eye.
   See section 2.1 of [`0.75.md`](0.75.md).
7. **A colour is three bytes in B, G, R order** — except inside
   `TerrainUpdate`, which is R, G, B (section 9.1 of [`1.0.md`](1.0.md)).
   This is a real trap; two different primitives exist for it.
8. **Nothing about a player is required to be a body.** In 1.0 a player
   record (name, team, score) is separate from its entity (the body).
   Bots are players with no connection.

---

## 3. How to use these documents

Read the protocol document end to end once; it is ordered the way an
implementation has to be built. Then work in the order its last section
gives ("minimum for a working client"): transport, codec, handshake, map,
entities, input, combat, building, game mode. Every step has a test.

These documents are self-contained. If a detail is ambiguous, the code in
`examples/` is normative for the byte layout.

Golden vectors (hex dumps of real packets) are given throughout. Encode the
same inputs with your codec and compare bytes before you debug a live
connection; on the wire every mistake looks like "the server ignores me".

---

## 4. Testing against our servers

The list at `https://master.shootanddestroy.obex.pw/v1/servers`
([`master.md`](master.md)) is public; a row with `"game_version": "1.0"`
accepts protocol 1.0, and a row with `"game_version": "0.75"` is a server
running our 0.75 profile. Public 0.75 servers are on the classic list,
`https://services.buildandshoot.com/serverlist.json`.

Servers log every refused packet and every handshake step with a reason.
When a server refuses you, the reason is in its log, not on the wire — the
wire only gets a number (disconnect reasons) or a `Kick` text.

For local testing:

* **1.0**: run our server from a release of the game, `--profile 10` (the
  default) on UDP 32887. It speaks 1.0 only.
* **0.75**: run any open-source 0.75 server (pyspades / piqueserver), or our
  server's `--profile 075`, and join it with your client.
* **Your own server**: the checks are the ones in section 10 of
  [`0.75.md`](0.75.md) and section 11 of [`1.0.md`](1.0.md).

---

## 5. Licence

The text is licensed CC BY 4.0; the example code is MIT. The protocols
themselves are facts, not a copyrighted work: write your own implementation
and license it as you like. Ace of Spades, BetterSpades, OpenSpades and
pyspades are other people's projects; this repository is not affiliated
with them and redistributes none of their assets.
