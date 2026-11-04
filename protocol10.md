This page documents **protocol 1.0**, the protocol of the Shoot and Destroy
server. It starts from OpenSpades' abandoned `ngspades` branch
("WorldOfSpades 0.1"), with that reference's defects fixed and the packets a
whole game needs added: chat, building, damage and game modes.

Current revision: **1.3** (three weapon slots, the compact player trajectory,
the jetpack bits). The revision is informational; nothing is negotiated on it.
There is no backward compatibility inside 1.0 yet: a wire change updates both
ends together.

# Connection

1.0 uses [ENet](http://enet.bespin.org/) 1.3 over UDP (IPv4 and IPv6), and
WebRTC DataChannels in a browser. The 32-bit **connect data** picks the
protocol:

| Number | Protocol |
|--------|----------|
| 135272729 (`0x08101919`) | 1.0 |

```c
ENetHost *host = enet_host_create(NULL, 1, 2, 0, 0);
enet_host_compress_with_range_coder(host);
ENetPeer *peer = enet_host_connect(host, &address, 2, 0x08101919);
```

| Setting | Value |
|---------|-------|
| Port | `32887` by convention, the same as 0.75 |
| Channels | 2: channel 0 reliable for everything, channel 1 unreliable sequenced for movement |
| Compression | ENet's range coder, mandatory |
| Framing | one game packet per ENet packet: one `Uint8` id, then the payload |

A client sends its own body (`Client Side Entity Update`, and an
`Entity Update` carrying only a trajectory) on channel 1 unreliable.
Everything else is reliable on channel 0. An update that must be seen before
an action — right before a `Hit Entity` or a block build/dig — goes reliable
on channel 0, or the server does not yet know you are firing or holding the
right tool and refuses the action.

A server answers a vanilla server's connect data with reason 3 ("wrong
protocol") and no `Kick`; treat that as "not a 1.0 server" and reconnect with
connect data 3.

## WebRTC (browsers)

A browser cannot send UDP. It reaches our servers over WebRTC DataChannels;
the packets are identical, only the carrier changes. Signalling goes through
our master ([Master Server](protocolmaster.md)).

* Two DataChannels, created **before** the offer: `reliable` (defaults) is
  channel 0, `unreliable` (`{ordered: false, maxRetransmits: 0}`) is channel
  1. Any other label is closed by the server.
* A **text** message is control. After both channels are open, send
  `connect 135272729` on `reliable` (the connect data in decimal). Anything
  else is refused with ENet's refusal (`3` = wrong protocol).
* A **binary** message is exactly one 1.0 packet, at most **16 KB**.
* The server ends a connection with the text `disconnect <reason>`, then
  closes the peer. A binary message before `connect` is dropped.

## Disconnect Reasons

| Number | Reason | Notes |
| --- | --- | --- |
| 0 | Unknown | |
| 1 | Internal server error | |
| 2 | Server full | |
| 3 | Kicked | a `Kick` packet with the text came first |
| 4 | Server stopped | |
| 5 | Timeout | a handshake step took too long |
| 6 | Malformed packet | |
| 7 | Protocol mismatch | the protocol name was not ours |

Vanilla uses the same field with another table (1 banned, 2 connection limit,
3 wrong protocol). The meaning depends on the protocol the connection was
opened with. The ambiguous case: connecting with the 1.0 magic to a vanilla
server gives `3` with no `Kick`.

## Timeouts

The server drops a connection that does not advance: 10 s for
`Initiate Connection`, 10 s for the certificate, 300 s for the map transfer.
There is no timeout in the game; ENet's peer timeout applies.

## Handshake

```
client                                            server
  ENet connect (0x08101919) or WebRTC "connect" -->
             <-------------------------------- Greeting (1)
  Initiate Connection (2) ------------------------>
             <------------------------ Server Certificate (3)
  Client Certificate (4) ------------------------->
             <------------------------ Game State Header (6)
             <------------------------ Map Data (7) x N
             <------------------------ Map Data Final (11)
  Map Data Acknowledge (10) ---------------------->  (after decoding)
             <------------------------ Game State Final (8)
  Generic Command "join <team> <weapon>" --------->
             <------------------------ Generic Command "local-player <id>"
             <------------------------ Entity Update / Player Update / Terrain Update
```

A world change (a new round, a reset) restarts the transfer at
`Game State Header` for every connection. The player is removed and the
client has to `join` again. Packets still in flight are dropped, not a
protocol error.

# Packets

All packets start with an unsigned byte naming their type. Unknown ids are
**ignored**, not a reason to disconnect.

## Data Types

All multi-byte fixed-width values are little-endian.

| Shorthand | Details |
| --- | --- |
| Uint8, Uint16, Uint32 | fixed width |
| Float | 32-bit IEEE-754 |
| Vec3 | three Floats |
| Varint | unsigned LEB128, at most 10 bytes |
| Svarint | zigzag, then Varint |
| Bytes | Varint length, then raw bytes (at most 1 MiB) |
| String | Bytes of UTF-8, no NUL inside |
| Map | repeated `String key, String value`, ended by an empty key |
| Color | three bytes in the order **B, G, R** |
| Block Coord | Svarint x, Svarint y, Svarint z |

Varint examples: `0 -> 00`, `127 -> 7F`, `128 -> 80 01`, `300 -> AC 02`.
Zigzag examples: `0 -> 00`, `-1 -> 01`, `1 -> 02`, `-2 -> 03`.

A decoder must reject a truncated packet, a varint over 10 bytes, a string
over 1 MiB or with a NUL in it, a mask bit it does not know, and an
out-of-range enum. **Mask bits are not self-describing**: an unknown bit's
payload cannot be measured, so new mask bits and new trailing fields change
both ends together.

## Table of Contents

* [Greeting](#greeting)
* [Initiate Connection](#initiate-connection)
* [Server Certificate / Client Certificate](#server-certificate--client-certificate)
* [Kick](#kick)
* [Game State Header](#game-state-header)
* [Map Data / Map Data Final](#map-data--map-data-final)
* [Map Data Acknowledge](#map-data-acknowledge)
* [Game State Final](#game-state-final)
* [Generic Command](#generic-command)
* [Entity Update](#entity-update)
* [Entity Event](#entity-event)
* [Entity Die](#entity-die)
* [Entity Remove](#entity-remove)
* [Client Side Entity Update](#client-side-entity-update)
* [Terrain Update](#terrain-update)
* [Player Update](#player-update)
* [Player Remove](#player-remove)
* [Player Action](#player-action)
* [Chat Message](#chat-message)
* [Chat Broadcast](#chat-broadcast)
* [Game Mode State](#game-mode-state)
* [Hit Entity](#hit-entity)
* [Hit Terrain](#hit-terrain)
* [Damage](#damage)
* [Build Block](#build-block)
* [Build Line](#build-line)
* [Throw Grenade](#throw-grenade)

## Greeting
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 1 |
| Total size | `1 + 1 + 5 + 1 + 2 + 128` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Magic | String | always `"Hello"` |
| Nonce | Bytes | 128 random bytes |

Hex: `01 05 48 65 6C 6C 6F 80 01 <128 bytes>`.

The nonce is used by the connect token. A client that receives **any other
packet id before the Greeting** must treat the server as not 1.0 (about 1 in
50 public 0.75 servers accept the magic and then speak 0.75).

## Initiate Connection
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 2 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Protocol Name | String | exactly `"WorldOfSpades 1.0"` |
| Major Version | Uint16 | `1` |
| Minor Version | Uint16 | `3` (the protocol revision) |
| Revision | Uint16 | your build, informational |
| Package | String | your build, at most 256 bytes |
| Environment | String | OS/CPU, at most 1024 bytes |
| Locale | String | e.g. `pt_BR`, at most 256 bytes |
| Player Name | String | UTF-8, at most 63 bytes |
| Nonce | Bytes | 256 random bytes |
| Map Quality | Uint8 | 0 = send VXL; 1..100 = ngmap at that quality |

Any other protocol name gets `Protocol mismatch` (7). `Map Quality` 0 means
"I cannot decode ngmap, send `map-format=vxl`"; 1..100 means "ngmap is fine",
clamped by the server to 20..100. The server may still answer VXL (encoding
failed, map too large): **trust `map-format` in the header**, not what you
asked for.

## Server Certificate / Client Certificate

| Info | Value |
| --- | --- |
| Packet IDs | 3 (`Server -> Client`), 4 (`Client -> Server`) |
| Total size | 1 byte, or variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Is Valid | Uint8 | 0 = no certificate, nothing follows |
| Certificate | Bytes | only if `Is Valid != 0` |
| Signature | Bytes | only if `Is Valid != 0` |

Both sides send `isValid = 0` normally; nothing is validated. The packets
exist so accounts can be added later. Our server uses the client certificate
to carry a **connect token** ([Master Server](protocolmaster.md)).

## Kick
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 5 |
| Total size | `1 + text` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Reason | String | shown to the player |

Sent before a disconnect with reason 3. Show the text.

## Game State Header
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 6 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Properties | Map | see below |

| Key | Example | Meaning |
| --- | --- | --- |
| `map-name` | `Pinpoint` | |
| `map-format` | `ngmap` or `vxl` | how to decode the stream |
| `map-size` | `512,512,64` | width, height, depth |
| `map-bytes` | `1834211` | total stream size, for a progress bar |
| `map-crc32` | `3f2a91c7` | CRC32 (zlib's) of the exact stream, 8 hex digits |

**Map cache.** A client that already holds the stream answers with
`Generic Command "map-cached 1"`; the server then sends no `Map Data` at all
and goes straight to `Map Data Final`. Answer `map-cached 0` when you do not.
The server waits 3 seconds for the answer. The key is the **stream**, not the
map: the CRC32 and byte count of the bytes the server would send, so two
qualities of one map, or one map after an edit, are different cache entries.

## Map Data / Map Data Final
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet IDs | 7 (`Map Data`), 11 (`Map Data Final`, empty) |
| Total size | `1 + n` / 1 |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Fragment | Bytes | one piece of the stream, in order |

Append every fragment; the concatenation is one stream. `Map Data Final` ends
it. Decode the map, apply the `Terrain Update`s that arrived during the
transfer in order, then acknowledge. The server keeps broadcasting terrain
edits to loading connections, so keep them until the map is ready.

### Map Formats

**`vxl`** is the zlib-compressed VXL, exactly the bytes 0.75 sends in
`Map Chunk`. One loader serves both protocols.

**`ngmap`** is a zlib stream (2-byte header, Adler-32 trailer). Decompressed:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Signature | Uint32 | `0x1145148A` |
| Width | Uint16 | |
| Height | Uint16 | |
| Depth | Uint16 | at most 64 |
| Quality | Uint8 | 1..100 |
| Geometry | column entries | one per column, boustrophedon: `y` 0..h-1, `x` 0..w-1 on even `y`, w-1..0 on odd `y` |
| JPEG Length | Uint32 | |
| JPEG | Uint8 array | the lossy colour tiles |
| Colour Blocks | to the end | |

A column entry is a format byte then either a run-length list of `Uint8`
toggle positions (the first byte carries `0x80` when the column starts solid
at z 0, the last byte is always `depth`) or a bitmap of 16-voxel slices.
Colours are cut into 8x8x8 cells with one of six formats (constant, linear,
planar, volumetric; raw or interpolated, some in JPEG tiles).

**Do not re-implement ngmap from this summary.** Port
`GameMap_ngmap.cpp` from the ngspades reference (about 1,900 lines) or ship
the C++ codec behind a C interface. It needs zlib and a JPEG decoder. Use
`Map Quality = 0` and skip it entirely if your client only needs to work.

## Map Data Acknowledge
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 10 |
| Total size | 1 byte |

No payload. Send it after decoding the map; the server waits for it before
`Game State Final`.

## Game State Final
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 8 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| World Parameters | Map | see below |
| Entity Count | Varint | |
| Entities | Entity Update Item x count | each with Create |
| Player Count | Varint | |
| Players | Player Update Item x count | each with Create |

After this packet the connection is in the game. Take the world parameters,
then build the world: every body and every player already in the match.

### World Parameters

All values are strings, parsed as numbers. Unknown keys are ignored.

| Key | Default | Meaning |
| --- | --- | --- |
| `player-jump-vel` | `0.36` | upward velocity added on jump |
| `fall-damage-vel` | `0.58` | landing speed where fall damage starts |
| `fall-damage-fatal-vel` | `1` | landing speed that kills |
| `fog-distance` | `96` | where bodies and effects stop (32..256) |
| `fog-color` | `0.6,0.7,0.9` | r,g,b 0..1 |
| `gamemode` | | the mode's wire name (`ctf`, `ngctf`, `tc`, `arena`, `zombie`, `koth`, ...) |
| `team1-name`, `team2-name` | | team names |
| `team1-color`, `team2-color` | | r,g,b 0..255 |
| `capture-limit` | | captures or rounds to win |
| `aim-assist` | `0` | 1 = the server allows aim assist |
| `jetpack-fuel`, `jetpack-regen`, `jetpack-thrust`, `jetpack-max-rise`, `jetpack-air-control`, `jetpack-glide-control`, `jetpack-ignition`, `jetpack-refill-delay`, `jetpack-ceiling` | | jetpack figures, only when the server has the jetpack |
| `sentry-lifetime` | | seconds a sentry lives |

## Generic Command
`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 9 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Parts | String x n | `parts[0]` is the command |

Unknown commands are logged and ignored on both sides. Rare, human-readable
actions live here; frequent or structured data gets its own packet id.

A client sends:

| Command | Meaning |
| --- | --- |
| `join <team> <weapon> [<class>]` | create your player: team 0/1 (255 spectator), weapon 0 rifle, 1 SMG, 2 shotgun; class 0..3 in a classes mode |
| `leave` | remove your player, back to watching |
| `map-cached <0\|1>` | the answer to the map header |
| `team <n>` | change team in game |
| `weapon <n>` | change weapon in game |
| `class <n>` | pick a class (classes modes) |
| `ping <info\|danger> x y z [n]` | a mark for your team; `n` is a phrase number |
| `ping spot <target> x y z` | spot an enemy |
| `squad <n>` | join squad n of your team (0 = alone) |
| `squad-rally` | the squad leader drops the rally point |
| `light <0\|1>` | your flashlight |
| `device <platform> <input>` | `desktop android iphone console browser` / `kbm pad touch` |
| `host <cmd> <args>` | bot management (permission checked) |

The server sends `local-player <id>` (you control that player and entity; no
argument = nothing), and the markers and notices: `hit`, `kill`, `perk-*`,
`sentry-spot`, `spot`, `spot-refused`, `revive-marker`, `revive-channel`,
`revived`, `squads`, `squad`, `squad-refused`, `squad-rally`, `restock`,
`wallet`, `fog`, `light`, `sky`, `outfit`, `team-outfits`, `device`,
`kill-assist`, `match-player`, `match-end`, `classes`, `class`,
`class-info`, `host-status`, `host-bots`, `rocket-refused`, `rocket-push`,
`drill-charges`, `drill-refused`, `crate`. Ignore the ones you do not use.

## Entity Update
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 30 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Forced | Uint8 | 1 = apply even to entities this client owns |
| Items | Entity Update Item x n | to the end of the packet |

An update is a **delta**: keep the last value of each part and change only
what the mask announces. An item with `mask == 0` is never sent. An item for
an unknown id without Create is ignored.

A client ignores parts for entities it owns, **unless** `Forced` is set or
the item has Create: that is how the server corrects a client it disagrees
with.

## Entity Event
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 31 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | |
| Event Type | Uint8 | 0 Jump, 1 Reload Weapon, 2 Suicide, 3 Explode, 4 Fire, 5 Use Perk |
| Param | Varint | event-specific, 0 if unused |

`Reload Weapon` on a player tells every client but the reloader to play the
reload sound and refill their view of that player's clip (without it you stop
drawing their shots once your count hits 0). `Explode` means a grenade,
rocket or explosive went off. `Jump` lets a body jump at once.

## Entity Die
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 32 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | the victim |
| Death Type | Uint8 | 0 unspecified, 1 player, 2 explode |
| Param | Varint | for a player: `reason \| respawn << 8` |
| Damage Info | optional | present only when bytes remain |

Damage info:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Damage Type | Uint8 | low 7 bits, bit 7 = a killer follows |
| Fire Position | Vec3 | |
| Hit Position | Vec3 | |
| From Entity | Varint | the killer, only with bit 7 |

**Check for the end of the packet before reading the damage type**: a death
without damage info has no such byte. `reason` is the 0.75 kill type: 0
weapon, 1 headshot, 2 melee, 3 grenade, 4 fall, 5 team change, 6 weapon
change, 7 sentry, 8 explosive. This one packet drives the kill feed.

## Entity Remove
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 33 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | |

The entity is gone: a grenade exploded or was cleaned up, a rocket hit, a
sentry was destroyed, a crate was taken or expired, a body left. Remove it at
once.

## Client Side Entity Update
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 35 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Items | Entity Update Item x n | to the end; never with Create |

You report your own body: trajectory, input when it changed, tool, block
colour. Movement is client-authoritative, but the server checks it:

* items for entities you do not own are dropped;
* a change of trajectory type is refused and corrected;
* the new position must be within **0.1 block** of the server's extrapolation
  along the old velocity, and the velocity within 0.1, or the server sends a
  `forced` correction.

Send at a steady rate (30 Hz), unreliable on channel 1, with the input
whenever it changed; one reliably on channel 0 right before a hit or a block
edit.

## Terrain Update
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 40 |
| Total size | variable |

A list of edits to the end of the packet, as deltas from a cursor that
starts at `(0,0,0)`:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| DX | Int8 | if `-128` (0x80): the cursor moves — read CX, CY, CZ, then DX again |
| DY | Int8 | |
| DZ | Int8 | |
| Health | Uint8 | 0 = destroyed, otherwise created |
| Red, Green, Blue | Uint8 x 3 | only when created; **R, G, B** here |
| Create Type | Uint8 | only when created: 0 unspecified, 1 player |
| Destroy Type | Uint8 | only when destroyed: 0 unspecified, 1 damage, 2 collapse |

The absolute cursor fields are unsigned Varints. **The cursor does not follow
the edits**: deltas are always relative to the last explicit cursor.

Example: create an orange block (255,128,0) at (10,20,30), health 100, by a
player:

```
28  0A 14 1E  64  FF 80 00  01
```

Destroy the block at (300,5,63); x does not fit, so the cursor moves first:

```
28  80 AC 02 05 3F  00 00 00  00  01
```

A create with health below 100 is a **cracked block**. Cells marked
`Collapse` are falling structures: remove them and build the falling piece
from their colours and connections. Apply a whole packet, then run block
physics once.

## Player Update
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 50 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Items | Player Update Item x n | to the end |

## Player Remove
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 51 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player IDs | Varint x n | to the end |

## Player Action
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 60 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Event Type | Uint8 | 0 Jump, 1 Reload Weapon, 2 Suicide, 5 Use Perk |
| Param | Varint | event-specific |

Your request to act on your own body: jump (the zombie leap carries its
charge in `param`), reload, the pause menu's respawn, or place a perk. The
server answers with an `Entity Event` when it accepts.

## Chat Message
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 70 |
| Total size | `2 + text` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Channel | Uint8 | 0 all, 1 team |
| Text | String | UTF-8, at most 255 bytes |

A client cannot send channel 2 (that would forge server notices). Hex, `gg`
to the team: `46 01 02 67 67`.

## Chat Broadcast
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 71 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| From | Varint | player id, `0xFFFFFFFF` = the server |
| Channel | Uint8 | 0 all, 1 team, 2 system |
| Text | String | UTF-8, at most 255 bytes |

Player 3 says `hi` to all: `47 03 00 02 68 69`.
The server says `Round over`:
`47 FF FF FF FF 0F 02 0A 52 6F 75 6E 64 20 6F 76 65 72`.

## Game Mode State
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 80 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| State | Map | key/value strings |

A packet whose **first** entry is `mode` is a snapshot: forget all game mode
state, then apply. Any other packet is a delta: each key replaces its value,
an empty value removes it. A snapshot comes right after `Game State Final`
and at every round start; a delta whenever something changes.

Numbers are decimal, lists comma-separated with no spaces, positions are
**block cells**.

| Key | Value | Meaning |
| --- | --- | --- |
| `mode` | `ctf`, `ngctf`, `tc`, `arena`, `zombie`, `koth`, ... | the mode; starts a snapshot |
| `team1-name`, `team2-name` | text | |
| `team1-color`, `team2-color` | `r,g,b` | 0..255 |
| `team1-score`, `team2-score` | decimal | captures, rounds or territories |
| `capture-limit` | decimal | what wins |
| `team1-intel`, `team2-intel` | `base`, `dropped`, `carried` | the intel belonging to that team |
| `territory-count` | decimal `N` | TC |
| `territory<i>` | `holder,capturing,permille,rate` | holder 0 team 1, 1 team 2, 2 nobody; share 0..1000; change a second |
| `arena-state` | `waiting`, `countdown`, `fight`, `end` | Arena |
| `arena-round`, `arena-time`, `arena-winner` | | Arena |
| `zombie-team`, `zombie-humans` | | Zombie |
| `koth-radius`, `koth-state`, `koth-round`, `koth-holder`, `koth-on`, `koth-points`, `koth-winner` | | King of the hill |

Extrapolate a territory as `permille + rate * elapsed`, clamped to 0..1000.

**The objectives are entities**: each intel a `Flag`, each tent a
`Command Post`, KotH's hill and TC's territories `Checkpoint`s. Their ids are
fixed: `0x7FFF0000` + 0 team 1's intel, 1 team 2's, 2 team 1's tent, 3 team
2's, 4 the hill, 5 + i territory i. A carried intel has its trajectory's
parent set to the carrier. The server sends the objective's `Entity Update`
**before** the `Game Mode State` that goes with it.

Babel is sent as `ctf`. A client that does not know a mode shows the scores.

## Hit Entity
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 90 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | who was hit |
| Hit Type | Uint8 | 0 unspecified, 1 torso, 2 head, 3 limb |
| Damage Type | Uint8 | `Weapon1` (7) or `Weapon2` (8): the slot fired from |
| Fire Position | Vec3 | your eye |
| Hit Position | Vec3 | the point the pellet met |

You report hits; the server decides damage. Our server refuses a gun hit
whose fire position is more than 4 blocks from the shooter's eye, whose hit
position is more than 3 blocks (plus lag tolerance) from the target, or whose
line crosses a solid block. A melee hit is damage type `Melee` (4).

## Hit Terrain
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 91 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Block | Block Coord | the cell hit |
| Damage Type | Uint8 | 5 Dig, 4 Melee, 7..9 a gun |
| Fire Position | Vec3 | |
| Hit Position | Vec3 | |

The spade's primary is `Melee` (destroy one block), its secondary `Dig` (the
block and the ones above and below). A gun's hit is `Weapon1`..`Weapon3`; the
slot is not checked for a block.

Our server keeps **block health**: a gun sends a `Hit Terrain` for every
pellet that meets a block, and the server subtracts the weapon's damage
(rifle 50, SMG 34, shotgun 20 a pellet, revolver 34) from the cell's 100. At
100 the block breaks; below it the cell goes out in the tick's
`Terrain Update` as a solid cell with health `100 - damage` — draw the crack.
A crack heals 10 s after its last hit. The spade still counts on the client
and sends only the hit that breaks the block.

## Damage
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 92 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | who took it |
| Damage Type | Uint8 | low 7 bits; bit 7 = a source follows |
| From Entity | Varint | only with bit 7 |
| To Entity | Varint | |
| Fire Position | Vec3 | |
| Hit Position | Vec3 | |
| Amount | Uint8 | hp removed |

Use it for the hit indicator (direction from the fire position), screen
effects and the local health. The authoritative health is the body's
`Health` field.

## Build Block
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 93 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Block | Block Coord | |
| Color | Color | B, G, R |

Example, block (256,255,62) with r=0x11, g=0x22, b=0x33:

```
5D  80 04 FE 03 7C  33 22 11
```

The server answers with a `Terrain Update` using create type `Player` and the
colour it accepted. Its rules apply: the block tool must be in hand, no
sprinting, a solid neighbour, the block count, an editable cell. A refused
build is answered with a `Terrain Update` restoring the cell.

**Colour order trap:** `Build Block`/`Build Line` send B, G, R;
`Terrain Update` sends R, G, B.

## Build Line
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 94 |
| Total size | variable |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| From | Block Coord | |
| To | Block Coord | |
| Color | Color | B, G, R |

Example, a line from (10,20,30) to (10,20,25), red:

```
5E  14 28 3C  14 28 32  00 00 FF
```

## Throw Grenade
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 95 |
| Total size | 29 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Origin | Vec3 | where it leaves the hand |
| Velocity | Vec3 | blocks per second |
| Fuse | Float | seconds until it explodes, 0..3 |

A thrown grenade becomes an entity with a Gravity trajectory, and an
`Entity Event Explode` when the fuse runs out. None of these go to the
thrower: your client shows your own grenade from the moment you throw it and
keeps it until `Explode` (the fuse is not on the wire).

# Entity Update Item

Used by `Entity Update`, `Client Side Entity Update` and `Game State Final`.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Entity ID | Varint | |
| Mask | Uint8 | which parts follow, in the order below |

| Bit | Part | Payload |
| --- | --- | --- |
| 0 | Create | Uint8 entity type; plus, for a player, the skins and weapons; plus, for an owned type, owner and team |
| 1 | Flags | Uint8 |
| 2 | Trajectory | see below |
| 3 | Player Input | Uint8 flags, Int8 xmove, Int8 ymove |
| 4 | Tool | Uint8 |
| 5 | Block Color | Color |
| 6 | Health | Uint8 |

**The fields follow the order above, not the bit order.**

Create for a **player**:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Body Skin | Bytes | `class/<name>` or `default` |
| Weapon Skin 1..3 | Bytes x 3 | `rifle`, `smg`, `shotgun`, `rocket`, `revolver`, `grenade`, `drill`, empty |
| Weapon Params 1..3 | Weapon Params x 3 | see below |

Create for an **owned type** (`Command Post`, `Flag`, `Checkpoint`, and every
type from `Sentry` up) adds:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Owner | Varint | player id, or `0xFFFFFFFF` for none |
| Team | Uint8 | 0, 1 or 255 |

A skin is lowercase ASCII, digits and `_ / -`, at most 32 bytes.

Entity flags: bit 0 `playerClip`, bit 1 `weaponClip`, bit 2 `fly`, bit 3
`jetpack`. Bits 4..7 are invalid.

Player input: bit 0 primary, bit 1 secondary, bit 2 chat (typing), bit 3
sprint, bit 4 jetpack thrust, bits 6..7 stance (0 stand, 1 crouch, 2 prone).
Bit 5 is unused. `xmove` is strafe, `ymove` forward, both -127..127; the
physics divides by 127 and clamps the length to 1.

## Weapon Params

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Flags | Uint8 | bit 0 full automatic, bit 1 slow reload, bit 2 penetrative, bits 3-4 type |
| Fire Interval | Uint16 | ms between shots |
| Reload Time | Uint16 | ms |
| Raise Time | Uint16 | ms before it can fire after being taken out |
| Max Damage | Uint8 | percent of 100 hp |
| Magazine | Uint8 | rounds |

The field order on the wire is **not** a C struct's order: reload and raise
come before damage. The 0.75 rifle is `{0, 500, 2500, 500, 49, 10}`.

## Trajectory

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Type | Uint8 | low 7 bits; bit 7 = a parent follows |
| Origin | Vec3 | |
| Velocity | Vec3 | |
| Rotation | Vec3 | compact quaternion `x,y,z`, `w >= 0`; not for Player |
| Angular Velocity | Vec3 | not for Player |
| Euler | Vec3 | Player only: pitch, roll, yaw (roll always 0) |
| Parent | Varint | only with bit 7 |

| Value | Type | Motion |
| --- | --- | --- |
| 0 | Constant | stays at the origin |
| 1 | Linear | `origin + velocity * t` |
| 2 | Gravity | `origin + velocity * t + 0.5 * g * t^2`, `g = (0,0,32)` |
| 3 | Player | the player physics, driven by the last input |
| 4 | Rigid Body | free body (vehicles; unused by our game) |
| 5 | Player Compact | the player trajectory in 17 bytes |

**Player Compact**: type, origin x/y as Uint16 (`* 64`), origin z as Int16
(`* 64`, rounded **up**, toward the sky), velocity x/y/z as Int16 (`* 1024`),
pitch and yaw as Int16 (`* 32767 / (pi/2)` and `* 32767 / pi`), then the
parent. The decoder hands it on as an ordinary Player trajectory. A client
sends its own trajectory in floats; the server validates it.

# Player Update Item

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Varint | |
| Mask | Uint8 | which parts follow |

| Bit | Part | Payload |
| --- | --- | --- |
| 0 | Create | String name |
| 1 | Score | Varint |
| 2 | Flags | Uint8 (bit 0 is admin) |
| 3 | Team | Uint8: 0, 1, 255 spectator |
| 4 | Stats | Varint kills, Varint deaths |
| 5 | Ping | Varint ms |
| 6 | Perks | Uint8 points, Uint8 inventory, Uint8 next threshold (owner only) |

The fields are written **in that order**, which is not the bit order.

# Enumerations

| Entity Type | Value |
| --- | --- |
| Player | 0 |
| Grenade | 1 |
| Rocket | 2 |
| Command Post | 3 |
| Flag | 4 |
| Checkpoint | 5 |
| Vehicle | 6 |
| Sentry | 7 |
| Landmine | 8 |
| Dynamite | 9 |
| Drill | 10 |
| Ammo Crate | 11 |
| Health Crate | 12 |

| Tool Slot | Value |
| --- | --- |
| Melee | 0 |
| Block | 1 |
| Weapon 1 (primary gun) | 2 |
| Weapon 2 (second gun) | 3 |
| Weapon 3 (grenades or drill) | 4 |

| Damage Type | Value |
| --- | --- |
| Unknown | 0 |
| Suicide | 1 |
| Fall | 2 |
| Poison | 3 |
| Melee | 4 |
| Dig | 5 |
| Block | 6 |
| Weapon 1 | 7 |
| Weapon 2 | 8 |
| Weapon 3 | 9 |
| Grenade | 10 |
| Vehicle Explosion | 11 |
| Vehicle | 12 |
| Sentry | 13 |

A player's body has **the same id as the player**. Players use ids 0..255;
everything else uses 256 and up. Bots are player records with no connection.

# Minimum Client

1. ENet with 2 channels and the 1.0 connect data (or WebRTC), ignoring
   unknown ids.
2. The primitive codec with the strict decoder rules.
3. `Initiate Connection` with `"WorldOfSpades 1.0"` and `Map Quality = 0`,
   so you only need the VXL loader; certificates with `isValid = 0`.
4. The map: header, fragments, final, decode, apply the kept terrain edits,
   acknowledge.
5. `Game State Final`, `Entity Update`, `Player Update`, `Player Remove`,
   `Entity Remove` with the delta rule, the trajectory and its compact form.
6. `join <team> <weapon>`, read `local-player`, then send
   `Client Side Entity Update` at 30 Hz.
7. `Hit Entity`, `Hit Terrain`, `Damage`, `Entity Die`.
8. `Build Block`, `Build Line`, `Throw Grenade`, `Chat Message`,
   `Chat Broadcast`, `Terrain Update`.
9. `Game Mode State` for CTF, plus the `Flag` and `Command Post` entities.

Then ngmap, classes, perks, the match summary.

# Implementation Notes

1. **Masks are not self-describing.** One unknown bit means the packet cannot
   be decoded; reject it.
2. **Field order is not bit order** in entity and player items.
3. **`Terrain Update` is R,G,B; every other colour is B,G,R.**
4. **Compact trajectory z rounds up** (toward the sky). Rounding to nearest
   puts a standing body inside the floor and the client's step-up makes every
   body bob.
5. **Ignore `Entity Update` parts for your own entity** unless forced or
   Create, or you fight your own prediction.
6. **Send the update before the action.** A hit or a build without a
   preceding input report is refused.
7. **Check the end before reading `Entity Die`'s damage info.**
8. **Unknown packet ids and map keys are ignored; unknown mask bits are
   errors.**
9. **The server may restart the transfer at any time**; drop the world, take
   the new map, `join` again.
10. **A `Map Data Acknowledge` is mandatory** after decoding.
11. **`map-format` decides the decoder**, not the quality you asked for.
12. **A WebRTC message over 16 KB is refused.**
13. **Text is UTF-8**; there is no CP437 in 1.0.

# Other Resources

* [Protocol 0.75](protocol075.md) — the coordinate system and the classic
  protocol both share.
* [Master Server](protocolmaster.md) — the list, WebRTC signalling, tokens.
* [Extensions](extensions.md) — the 0.75 extension registry.
* `examples/c/` — the dependency-free C codec this document describes.
* `examples/godot/` — the same in GDScript, with a WebRTC client.
