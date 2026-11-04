This page documents the Ace of Spades **0.75** protocol and its **0.76**
variation, the classic wire public servers speak. It is written from the
point of view of both ends: every packet says which direction it goes, what
the server sends or expects, and what a client has to send.

0.76 differs in exactly three places, marked `[0.76]` throughout: the ENet
connect data, the `Map Start` packet and the `World Update` entry. One
implementation can speak both.

# Connection

Ace of Spades uses [ENet](http://enet.bespin.org/) 1.3 over UDP. When you
connect, the 32-bit **connect data** picks the protocol:

| Number | Protocol |
|--------|----------|
| 3      | 0.75     |
| 4      | 0.76     |

```c
ENetHost *host = enet_host_create(NULL, 1, 1, 0, 0);
enet_host_compress_with_range_coder(host);
ENetPeer *peer = enet_host_connect(host, &address, 1, 3);
```

| Setting | Value |
|---------|-------|
| Port | `32887` by convention |
| Channels | 1 (channel 0) |
| Compression | ENet's range coder, mandatory in both directions |
| Reliability | everything reliable except `World Update` (2), sent unreliable |
| Framing | one game packet per ENet packet: one `Uint8` id, then the payload |

A server must check the length of every packet before reading it, and check
the `player_id` field of a client packet against the sender's id.

A server may send the map again in the middle of a match (a new round); a
client must handle `Map Start` at any time.

## Disconnect Reasons

The server puts a `Uint32` reason in the ENet disconnect event. ENet also
uses 0 for a timeout, so 0 is ambiguous.

| Number | Reason | Notes |
|--------|--------|-------|
| 0 | Timeout | or an unspecified server reason |
| 1 | Banned | |
| 2 | Kicked | some servers use it for "too many connections from your address" |
| 3 | Wrong protocol version | connect data was not 3 or 4 |
| 4 | Server full | |
| 5 | Server shutting down | |
| 8, 9, 10, 11 | Banned, full, kicked, wrong version | the same reasons after the map download; the Voxlap client only knows 0..4 before it |
| 20 | Invalid name | |
| 61 | Server shutting down | an alternative value some servers use |

A system `Chat Message` from player `255` sent just before the disconnect is
the kick text (the Kick Reason extension, see [Extensions](extensions.md)).
Show any unknown number as "unknown"; never index a table with it.

## About Addresses

```
aos://<address>[:<port>][<version suffix>]
```

* `<address>` is the IPv4 address as one decimal number in **little-endian**
  byte order: `aos://16777343` is `127.0.0.1` (`0x0100007F`). Byte 0 of the
  address is the low byte of the number.
* `<port>` defaults to `32887`.
* A suffix after the port (`:0.75`, `/0.76`) names a version. An unknown
  suffix means 0.75.

Accept a dotted address (`aos://1.2.3.4:32887`) and a bare `host:port` too.

## About Coordinates

The map is `512 x 512 x 64` voxels. `x` goes east, `y` goes south, and **`z`
grows downwards**: 0 is the top of the world, 63 is the bottom (water) layer.

* A float position `(0,0,0)` is the corner of voxel `(0,0,0)`;
  `(0.5,0.5,0.5)` is its centre. Floor each axis to get the voxel.
* A player's position is its **eye**, not its feet.
* The two bottom layers, `z = 62` and `z = 63`, cannot be dug.
* Terrain outside 0..511 on `x`/`y` is drawn as a copy of the map
  (toroidal wrap). Players, block edits and block physics do **not** wrap;
  grenades do.
* A look direction is the unit vector `(ox, oy, oz)`; `oz` is negative when
  looking up (because `z` grows downwards).

If your engine has `y` up, convert at the protocol boundary:
`engine = (x, 63 - z, y)`, and back with `(x, z, 63 - y)`.

## About Text

* **CP437** is the original encoding. Bytes `0x80`..`0xFF` are code page 437.
* **UTF-8** is sent by newer servers: a string beginning with byte `0xFF`
  and valid UTF-8 after it is UTF-8 (drop the `0xFF`). Anything else is
  CP437. To send, use CP437 when every character fits, otherwise `0xFF` +
  UTF-8 — and only to a server known to read it (its extended version
  answer's feature bit 0, or a `0xFF` line it wrote).

Chat text may contain colour control bytes: `0x01`-`0x02` team colours,
`0x03` spectators, `0x04` red, `0x05` green, `0x06` white, `0x07` grey,
`0x08` yellow. `\n` and `\r` break the line.

# Packets

All packets start with an unsigned byte naming their type; the sizes below
include that byte. Structures are packed, little-endian, no padding.

## Data Types

| Shorthand | Details |
| --- | --- |
| Uint8 | 8-bit unsigned |
| Int8 | 8-bit signed |
| Uint16 | 16-bit unsigned |
| Int32 | 32-bit signed |
| Uint32 | 32-bit unsigned |
| Float | 32-bit IEEE-754 |
| Vec3 | three Floats: x, y, z |
| Vec3i | three Int32: x, y, z |
| Color | three bytes in the order **B, G, R** |
| CP437 String | NUL-terminated string of that encoding |
| UTF-8 String | `0xFF` followed by UTF-8 (see About Text) |

## Table of Contents

* [Position Data](#position-data)
* [Orientation Data](#orientation-data)
* [World Update](#world-update)
* [Input Data](#input-data)
* [Weapon Input](#weapon-input)
* [Set HP / Hit](#set-hp--hit)
* [Grenade](#grenade)
* [Set Tool](#set-tool)
* [Set Color](#set-color)
* [Existing Player](#existing-player)
* [Short Player Data](#short-player-data)
* [Move Object](#move-object)
* [Create Player](#create-player)
* [Block Action](#block-action)
* [Block Line](#block-line)
* [State Data](#state-data)
* [Kill Action](#kill-action)
* [Chat Message](#chat-message)
* [Map Start](#map-start)
* [Map Chunk](#map-chunk)
* [Player Left](#player-left)
* [Territory Capture](#territory-capture)
* [Progress Bar](#progress-bar)
* [Intel Capture](#intel-capture)
* [Intel Pickup](#intel-pickup)
* [Intel Drop](#intel-drop)
* [Restock](#restock)
* [Fog Color](#fog-color)
* [Weapon Reload](#weapon-reload)
* [Change Team](#change-team)
* [Change Weapon](#change-weapon)
* [Handshake Init](#handshake-init)
* [Handshake Return](#handshake-return)
* [Version Get](#version-get)
* [Version Send](#version-send)
* [Map Cached](#map-cached)
* [Ext Info](#ext-info)
* [Player Properties](#player-properties)
* [Light](#light)

## Position Data
`Server -> Client`

Teleports or corrects **the receiver**. It carries no player id: it is always
about you.

| Info | Value |
| --- | --- |
| Packet ID | 0 |
| Total size | 13 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| X, Y, Z | Vec3 | the eye position |

The client sets its position, zeroes its velocity and keeps its look. Never
interpolate: it is a correction, not a movement.

`Client -> Server` uses the same packet to report its eye position, about
once a second while alive and right after a teleport. The server compares it
with its simulation and answers with `Position Data` when it disagrees.

## Orientation Data
`Server -> Client`

Turns the receiver to a direction. Rare; normally the look rides
`World Update`.

| Info | Value |
| --- | --- |
| Packet ID | 1 |
| Total size | 13 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| X, Y, Z | Vec3 | unit look vector |

`Client -> Server`: the client's look, sent when it changed by more than
about 0.5 degrees, up to ~120 times a second, and **right before each shot**
so the server can validate the hit.

## World Update
`Server -> Client`

The only unreliable packet. A newer update replaces the old one.

| Info | Value |
| --- | --- |
| Packet ID | 2 |
| Total size | `1 + 24 * n` (0.76: `1 + 25 * n`) |

0.75: one entry per player slot, indexed by id, up to 256 slots.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Position | Vec3 | eye position, `(0,0,0)` = not visible |
| Orientation | Vec3 | look vector |

`[0.76]` one entry per present player:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Position | Vec3 | |
| Orientation | Vec3 | |

Skip your own entry. **A zero position means "not visible": keep the last
known position.** Servers fill dead players, players out of the fog and free
slots with zeros. Interpolate between updates (about 60 a second).

## Input Data
`Server -> Client`

Another player's movement keys.

| Info | Value |
| --- | --- |
| Packet ID | 3 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Keys | Uint8 | bit packed, see below |

| Bit | Key |
| --- | --- |
| 1 | forward |
| 2 | back |
| 4 | left |
| 8 | right |
| 16 | jump |
| 32 | crouch |
| 64 | sneak |
| 128 | sprint |

For yourself only the jump bit matters (the server echoes the physics it
applied). `Client -> Server` sends the same packet whenever the keys change.

## Weapon Input
`Server -> Client`

Another player's fire buttons.

| Info | Value |
| --- | --- |
| Packet ID | 4 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Buttons | Uint8 | bit 0 primary (left), bit 1 secondary (right) |

It is not the raw mouse: a server sends 0 while a player sprints, switches
tool or reloads. Use it to draw tracers, swings and muzzle flashes for that
player; ignore it for yourself.

`Client -> Server` sends it whenever a button changes, except while
sprinting.

## Set HP / Hit

`Server -> Client` — **Set HP**

| Info | Value |
| --- | --- |
| Packet ID | 5 |
| Total size | 15 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| HP | Uint8 | the receiver's new health, 0..100 |
| Type | Uint8 | 0 fall damage, 1 anything else |
| Source | Vec3 | where the damage came from; zero for a fall |

A drop flashes the screen red and plays the hit sound. The value is
informational: at 0 a `Kill Action` follows.

`Client -> Server` — **Hit**. Packet id 5 again; the direction decides.

| Info | Value |
| --- | --- |
| Packet ID | 5 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Victim | Uint8 | player id |
| Hit Type | Uint8 | 0 torso, 1 head, 2 arms, 3 legs, 4 melee |

The client reports every hit its shot or swing made; the server decides
whether it counts and how much it hurts. Send only hits you really saw.

## Grenade
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 6 |
| Total size | 30 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | thrower |
| Fuse | Float | seconds until it explodes |
| Position | Vec3 | |
| Velocity | Vec3 | **blocks per 1/32 s** |

Gravity is 32 blocks/s². The server does not echo a grenade to its thrower.

`Client -> Server`: the throw, with the fuse left (up to 3 s), where it
leaves the hand, and velocity in blocks per 1/32 s
(`blocks_per_second / 32`).

## Set Tool
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 7 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Tool | Uint8 | 0 spade, 1 block, 2 gun, 3 grenade |

Ignore a value above 3. `Client -> Server` sends it whenever the tool changes.

## Set Color
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 8 |
| Total size | 5 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Color | Color | B, G, R |

Keep it even for a player who is not alive or not joined yet: later block
packets use it. `Client -> Server` sends it when the player picks a colour.

## Existing Player
`Client <-> Server`

A player already in the match (server to client), or the client's join
(client to server).

| Info | Value |
| --- | --- |
| Packet ID | 9 |
| Total size | `11 + name` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Team | Int8 | 0 team 1, 1 team 2, -1 (255 on the wire) spectator |
| Weapon | Uint8 | 0 rifle, 1 SMG, 2 shotgun |
| Tool | Uint8 | as in Set Tool |
| Kills | Uint32 | |
| Color | Color | B, G, R |
| Name | CP437 String | the classic limit is 15 characters |

A server sends it for every other player after the map and before
`State Data`. **It carries no position**: keep the player hidden until a
`World Update` or `Create Player` places them.

A client sends it after `State Data` to join, with its own id, the chosen
team, weapon, tool (gun), zero kills, its block colour and its name. The
server answers with `Create Player`.

The extended version answer reuses this id: the first byte after the id is
`'x'` (0x78), which no player id can be (see [Extensions](extensions.md)).

## Short Player Data
`Client -> Server`

The older join form.

| Info | Value |
| --- | --- |
| Packet ID | 10 |
| Total size | 4 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Team | Uint8 | |
| Weapon | Uint8 | |

Servers accept it as well as `Existing Player`.

## Move Object
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 11 |
| Total size | 15 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Object | Uint8 | CTF: 0/1 intels, 2/3 tents. TC: territory index |
| Team | Int8 | the owner, for TC |
| Position | Vec3 | the new cell |

CTF: moving an intel also drops it from its carrier, silently. TC: `team` is
`0`, `1` or `2` (neutral).

## Create Player
`Server -> Client`

A player spawned (or joined for the first time).

| Info | Value |
| --- | --- |
| Packet ID | 12 |
| Total size | `15 + name` (16 with an empty name) |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Weapon | Uint8 | |
| Team | Int8 | spectator = not alive |
| Position | Vec3 | the **eye** |
| Name | CP437 String | absent in a 16-byte packet |

The player is alive unless the team is spectator. It faces east for team 1
and west for team 2. Reset its block colour to `(111,111,111)` and restock it
(full health, 50 blocks, 3 grenades, full magazine and reserve).

The original Voxlap client draws the player at `position.z - 2`; modern
clients use the value as given. A 16-byte packet has no name: keep the one
you had.

## Block Action
`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 13 |
| Total size | 15 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Action | Uint8 | see below |
| X, Y, Z | Vec3i | the block cell |

| Action | Effect |
| --- | --- |
| 0 | build one block with the player's colour (a repaint when already solid) |
| 1 | destroy one block |
| 2 | spade: destroy the block and the ones above and below (1 x 1 x 3) |
| 3 | grenade: destroy 3 x 3 x 3, silently |
| 4 | destroy one block (a gun) |
| 5 | destroy one block (a spade) |

Actions 2 and 3 leave `z >= 62` alone; cells outside the map are ignored.

A server applies its rules (tool in hand, no sprinting, reach about 4
blocks, a solid neighbour for a build, the block count, editable cells) and
relays it to everyone. A client **batches edits that arrive together**: keys
by position (last wins), apply creates then destroys, then run block physics
once. Applying them one by one makes a repaint collapse a structure.

## Block Line
`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 14 |
| Total size | 26 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Start | Vec3i | |
| End | Vec3i | |

Builds a line of blocks between the two cells (a 3D DDA), skipping cells that
are already solid. It costs `|dx| + |dy| + |dz| + 1` blocks.

## State Data
`Server -> Client`

Ends the map download.

| Info | Value |
| --- | --- |
| Packet ID | 15 |
| Total size | 83 bytes (CTF), `32 + 13 * n` (TC) |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | you |
| Fog | Color | |
| Team 1 Color | Color | |
| Team 2 Color | Color | |
| Team 1 Name | 10 bytes | NUL padded, may lack the NUL |
| Team 2 Name | 10 bytes | |
| Mode | Uint8 | 0 CTF, 1 TC |

**CTF (52 bytes)**

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Team 1 Score | Uint8 | captures |
| Team 2 Score | Uint8 | |
| Capture Limit | Uint8 | |
| Intel State | Uint8 | bit 0 team 1's intel held, bit 1 team 2's |
| Team 1 Intel | 12 bytes | held: `{Uint8 carrier; 11 bytes padding}`; else `Vec3 position` |
| Team 2 Intel | 12 bytes | the same |
| Team 1 Base | Vec3 | the tent |
| Team 2 Base | Vec3 | |

**TC (`1 + 13 * n`)**

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Count | Uint8 | number of territories |
| Territories | Count x Territory | |

| Territory Field | Field Type | Notes |
| --- | --- | --- |
| Position | Vec3 | |
| Team | Uint8 | 0, 1 or 2 (neutral) |

Treat an unknown mode as TC. Read the names as 10 bytes. Load the map only
now, then open the team menu.

## Kill Action
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 16 |
| Total size | 5 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Victim | Uint8 | |
| Killer | Uint8 | equal to the victim for a suicide |
| Kind | Uint8 | see below |
| Respawn | Uint8 | seconds until the respawn |

| Kind | Meaning |
| --- | --- |
| 0 | weapon |
| 1 | headshot |
| 2 | melee |
| 3 | grenade |
| 4 | fall |
| 5 | team change |
| 6 | weapon/class change |

The killer scores unless it was a suicide. Draw the kill feed and start the
respawn countdown.

## Chat Message
`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 17 |
| Total size | `3 + text` (the NUL is included) |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | 255 = the server |
| Type | Uint8 | 0 all, 1 team, 2 system; 3-6 optional (see Message Types) |
| Text | CP437 String | or UTF-8 with the `0xFF` prefix |

Server notices use type 2 from player 255. Types 3-6 (big, info, warning,
error) are the Message Types extension. A client sends types 0 and 1 only.

A system line from player 255 sent just before a disconnect is the kick text.

## Map Start
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 18 |
| Total size | 5 bytes (0.76: 73) |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Map Size | Uint32 | length of the compressed map stream |

`[0.76]`

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Map Size | Uint32 | |
| CRC32 | Uint32 | of the compressed stream |
| Map Name | 64 bytes | NUL padded |

The stream is a **zlib-compressed VXL** map. Append every `Map Chunk` until
you have `Map Size` bytes, then inflate and load. A 0.76 server asks first:
answer `Map Cached` with 1 when you already have that CRC32, and it sends no
chunks.

## Map Chunk
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 19 |
| Total size | `1 + n` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Data | Uint8 array | a piece of the stream, in order |

Chunks may be any size. Do not start decoding before the total matches.

## Player Left
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 20 |
| Total size | 2 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |

Clear the slot.

## Territory Capture
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 21 |
| Total size | 4 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Territory | Uint8 | index |
| Winning | Uint8 | 1 ends the round |
| Team | Uint8 | 0, 1 or 2 (neutral) |

Some clients read an uninitialized pointer when `team` is not 0/1/2; a server
should only send those three.

## Progress Bar
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 22 |
| Total size | 8 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Territory | Uint8 | index |
| Team | Int8 | the capturing team |
| Rate | Int8 | progress per 50 s |
| Progress | Float | 0..1 |

A client advances `0.05 * rate` a second, like the 0.75 reference.

## Intel Capture
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 23 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Winning | Uint8 | 1 ends the round |

Ten points to the player and one to the team.

## Intel Pickup
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 24 |
| Total size | 2 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | the carrier |

## Intel Drop
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 25 |
| Total size | 14 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | who dropped it |
| Position | Vec3 | where it lies now |

## Restock
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 26 |
| Total size | 2 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |

That player gets full health, 50 blocks, 3 grenades and a full reserve. For
yourself update the HUD; for someone else only play the sound. Ignore it
while you are dead.

## Fog Color
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 27 |
| Total size | 5 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Alpha | Uint8 | unused |
| Color | Color | B, G, R |

0.75 has no fog distance: it is always 128 blocks.

## Weapon Reload
`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 28 |
| Total size | 4 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Magazine | Uint8 | rounds now in the gun |
| Reserve | Uint8 | rounds left |

A client sends it when it starts a reload. The server answers when the reload
finishes; for yourself that answer settles the ammo, for another player it
only plays the sound (and refills your view of their clip, or you stop drawing
their shots).

## Change Team
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 29 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Team | Uint8 | 0, 1 or 255 |

A team change during a match is a death (kill kind 5) followed by a respawn.

## Change Weapon
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 30 |
| Total size | 3 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Player ID | Uint8 | |
| Weapon | Uint8 | 0 rifle, 1 SMG, 2 shotgun |

A death with kill kind 6, then a respawn.

## Handshake Init
`Server -> Client`

Optional client fingerprinting.

| Info | Value |
| --- | --- |
| Packet ID | 31 |
| Total size | 5 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Challenge | Uint32 | echo it back |

The Voxlap client does not answer; a server must never require it.

## Handshake Return
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 32 |
| Total size | 5 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Challenge | Uint32 | the same four bytes |

## Version Get
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 33 |
| Total size | 1 byte, or `1 + n` for the extended form |

`Version Get` with property ids after the id asks for the extended answer
(see [Extensions](extensions.md)).

## Version Send
`Client -> Server`

| Info | Value |
| --- | --- |
| Packet ID | 34 |
| Total size | 69 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Client | Uint8 | `'B'` BetterSpades and ours, `'o'` OpenSpades |
| Major, Minor, Revision | Uint8 x 3 | |
| OS | 64 bytes | NUL padded |

## Map Cached
`Client -> Server` `[0.76]`

| Info | Value |
| --- | --- |
| Packet ID | 31 |
| Total size | 2 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Cached | Uint8 | 1 = I have the map, send no chunks |

## Ext Info
`Client <-> Server`

The extension negotiation. Its layout and the registry are in
[Extensions](extensions.md).

## Player Properties
`Server -> Client`

Optional extension `0x00`.

| Info | Value |
| --- | --- |
| Packet ID | 64 |
| Total size | 12 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 0 |
| Player ID | Uint8 | |
| Health | Uint8 | 0 for an enemy |
| Blocks | Uint8 | 0 for an enemy |
| Grenades | Uint8 | 0 for an enemy |
| Magazine | Uint8 | |
| Reserve | Uint8 | |
| Score | Uint32 | kills plus score |

Sent when a value changes, at most every 100 ms per player.

## Light
`Client <-> Server`

Optional extension `0x32` (Flashlight).

| Info | Value |
| --- | --- |
| Packet ID | 114 |
| Total size | 4 bytes |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 0 request / relay, 1 relay, 2 config, 3 state |
| Player ID | Uint8 | |
| On | Uint8 | 0 or 1 |

Send a request only after you accepted the extension, and draw the beam only
when the server relays it. A light goes off silently on `Create Player`,
`Kill Action`, `Player Left` and a new map.

# Game Rules

The wire carries state; the game still needs these rules.

## Players

Ids 0..255. Keep state for all 256: joined, alive, team, weapon, tool, score,
name, block colour, position, orientation, input. Only for yourself: health,
blocks, grenades, magazine, reserve. Out-of-range values fall back to
something safe (spectator, rifle, spade); never index a table with a value
from the wire.

## Fog

Sight is a cylinder of radius 128 around the camera. Nothing beyond is drawn,
shots stop there, the sky is the fog colour. `State Data` and `Fog Color` set
the colour. A server may send positions beyond the fog anyway; do not draw
them (that is a wallhack).

## Block Physics

When a voxel stops being solid, check its six neighbours. Each solid one
starts a flood fill through solid voxels. If the fill reaches `z >= 62`,
nothing happens; otherwise everything it reached is detached, falls and is
removed. Voxels that were **already empty** inside a spade or grenade area
must not start a check.

## Damage, Death and Respawn

The client reports hits; the server sends `Set HP`, then `Kill Action` at 0.
On death the intel is dropped (`Intel Drop`), the corpse is drawn (or a
grave), the kill feed updates and the respawn counter runs from the kill
packet. The server spawns the player with `Create Player`.

## Blocks

A player carries at most 50 blocks. A build costs one, a spade destroy gives
one back. `Restock` refills at the tent. The client counts locally for the
HUD; the server counts for the rules.

## CTF

Two scores, a capture limit, two intels (held by a player or lying at a
cell), two tents. Touching the enemy intel picks it up, carrying it into your
tent captures it, dying drops it. A capture puts the intel back at its base
(`Move Object`). A round ends at the capture limit.

## TC

A list of territories (position and owner). Standing in one moves its capture
towards your team; the server sends `Progress Bar` and then
`Territory Capture`. Owning them all wins.

# Server List

The classic public list is a JSON array at `/serverlist.json`, for example
`https://services.buildandshoot.com/serverlist.json`. Each entry has at
least:

| Key | Example | Notes |
| --- | --- | --- |
| `name` | `"Deuces Wild"` | |
| `identifier` | `"aos://16777343:32887"` | the address to join |
| `map` | `"Hallway"` | |
| `game_mode` | `"ctf"` | |
| `country` | `"US"` | ISO 3166-1 alpha-2 |
| `latency` | `120` | ms, measured by the list |
| `players_current`, `players_max` | `16`, `32` | |
| `last_updated` | `1301931731` | Unix seconds |
| `game_version` | `"0.75"` | |

Ignore keys you do not know and expect duplicates. Our master serves the same
shape plus `id`, `identifier6` and `info`; see
[Master Server](protocolmaster.md).

# Writing a Server

**Must**

1. An ENet host with the range coder, accepting connect data 3 (and 4).
2. Version, ban and capacity checks with the right disconnect reasons.
3. The map: VXL, zlib, `Map Start`, `Map Chunk`.
4. `Existing Player` for every player, then `State Data`, after the map.
5. The join (`Existing Player` or `Short Player Data`), then `Create Player`
   to everyone.
6. `World Update` at a steady rate, unreliable, zeros for unseen slots.
7. Relay and validate `Input Data`, `Weapon Input`, `Set Tool`, `Set Color`,
   `Orientation Data`, `Position Data`.
8. `Hit` -> `Set HP` -> `Kill Action` -> respawn.
9. `Block Action` and `Block Line` with validation, block physics, `Restock`.
10. Chat and `Player Left`.
11. At least CTF.

**Should**

* Server-side movement physics with corrections.
* `Create Player` for players that do not move (bots) when someone joins, or
  they stand at the map corner until the next `World Update`.
* Grenades simulated on the server.
* `Version Get` / `Handshake Init`, as optional checks.
* `0xFF` + UTF-8 for text CP437 cannot express.
* `Ext Info` and the extensions.

Spawn a player with 100 health, 50 blocks, 3 grenades, a full magazine and
reserve, the gun in hand and no input held. Send every packet reliable except
`World Update`, and one game packet per ENet packet.

# Other Resources

* [Protocol 1.0](protocol10.md) — the protocol of our server.
* [Extensions](extensions.md) — the aosprotocol extension registry.
* [Master Server](protocolmaster.md) and [Ping](protocolping.md).
* [VXL file format](http://silverspaceship.com/aosmap/aos_file_format.html).
* [BetterSpades](https://github.com/xtreme8000/BetterSpades),
  [OpenSpades](https://github.com/yvt/openspades),
  [pyspades](https://github.com/piqueserver/piqueserver) — reference
  implementations.
