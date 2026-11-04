This page documents the packet extensions of the 0.75 protocol: how a server
and a client agree on one, the id registry, and the extensions our server
speaks. The reference specification is
[zerospades/aosprotocol](https://github.com/zerospades/aosprotocol).

# Overview

Every extension has a unique id, decided when it is first registered. There
are two kinds:

| Type | Purpose | Extension id range |
| --- | --- | --- |
| `HAS_PACKETS` | introduces new packets | 0-191 |
| `PACKETLESS` | changes existing packets or is only announced | 192-255 |

An extension with packets is reached by one legacy packet id equal to
`64 + extension id`. `PACKETLESS` extensions have no packet of their own.

Each extension packet carries one extra byte after the packet id: a **sub
packet id**, used to have several packets per extension. It is always there,
even when the extension needs only one packet.

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Packet ID | Uint8 | `64 + extension id` |
| Sub Packet ID | Uint8 | 0-255 |
| Data | Uint8 array | the sub packet's data |

# Ext Info

`Client <-> Server`

| Info | Value |
| --- | --- |
| Packet ID | 60 |
| Total size | `2 + 2 * n` |

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Count | Uint8 | the number of entries that follow |
| Entries | Ext Info Entry x count | |

An entry:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Extension ID | Uint8 | see the registry |
| Version | Uint8 | starting at 1 |

The aosprotocol page words `Count` as "length, `length+1` entries will
follow". Our server writes and our client reads the **entry count**, with no
`+1`. A client reading a server it does not know should treat the field as a
count; if the first entry is not a known extension id and `count - 1` entries
parse cleanly to the end of the packet, the other wording was meant.

## Protocol Flow

The server **must** send an `Ext Info` on connect (our server sends it right
after `State Data`). The client stores the list and **must** reply with an
`Ext Info` naming the extensions it supports.

The reply is the intersection: never name an extension the server did not
offer, and never one you do not implement. An extension's packets go only to
clients that agreed to it. A server sends nothing of a `PACKETLESS`
extension; the agreement alone changes how it writes existing packets.

# Registry

| ID | Name | Kind | Packet | What it does |
| --- | --- | --- | --- | --- |
| `0x00` | Player Properties | `HAS_PACKETS` | 64 | exact health, blocks, grenades, ammo and score of players |
| `0x20` | Damage Markers | `HAS_PACKETS` | 96 | hit markers and floating damage numbers |
| `0x30` | Teamplay | `HAS_PACKETS` | 112 | config, pings, ESP marks, teammate names |
| `0x31` | Pie Menu | `HAS_PACKETS` | 113 | server-defined quick-chat rings |
| `0x32` | Flashlight | `HAS_PACKETS` | 114 | per-player lights |
| `0x33` | Daytime and Weather | `HAS_PACKETS` | 115 | time of day and weather |
| `0x35` | Player Outfit | `HAS_PACKETS` | 117 | how players look (a draft) |
| `0xC0` | Player Limit | `PACKETLESS` | - | how many players the server accepts |
| `0xC1` | Message Types | `PACKETLESS` | - | chat types 3..6 |
| `0xC2` | Kick Reason | `PACKETLESS` | - | the kick text as a system line |

# Player Properties
`Server -> Client` — extension `0x00`, packet 64, sub 0

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

Sent when a value changes, at most every 100 ms per player. A client keeps
the ammo and score of everyone (the ammo decides whether another player's
shot is drawn) but the health, blocks and grenades of its own player only.

# Damage Markers
`Server -> Client` — extension `0x20`, packet 96

Sub 0 is a damage marker: the attacker is told the victim and the amount;
the client draws a hit marker, and a number rises over the victim. The exact
fields are in the aosprotocol specification.

# Teamplay
`Client <-> Server` — extension `0x30`, packet 112

| Sub | Name | Direction | Purpose |
| --- | --- | --- | --- |
| 0 | Config | `Server -> Client` | what the server allows |
| 1 | Ping | both | a mark for the team |
| 2 | ESP Mark | `Server -> Client` | a spot shown through walls |

Config:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 0 |
| Flags | Uint8 | bit 0 teammate ESP, bit 1 pings, bit 2 compass |
| North X | Float | the map direction the client calls north |
| North Y | Float | a zero or NaN means the map's top |

A ping carries its position, life in seconds, colour, label and surfaces
(world, map, compass); an ESP mark carries a target, life, whether to show
the name, colour and label. The layouts are in the specification. Our server
sends the server's own pings as player 255.

# Pie Menu
`Server -> Client` — extension `0x31`, packet 113

The server defines the quick-chat rings: a menu id, the contexts it applies
to (block, teammate, enemy, sky, spectator, dead) and the slices with their
text. A menu is parsed whole or refused. The layout is in the specification.

# Flashlight
`Client <-> Server` — extension `0x32`, packet 114

| Sub | Name | Direction | Purpose |
| --- | --- | --- | --- |
| 0 | Light | both | a player's light on or off |
| 1 | Light State | `Server -> Client` | one bit per player id |
| 2 | Light Config | `Server -> Client` | the beam |

Light:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 0 |
| Player ID | Uint8 | |
| On | Uint8 | 0 or 1 |

A client sends Light to ask; the server relays the accepted request to every
client that agreed, the owner included (a client draws only a light the
server switched on). A light goes off **silently** on `Create Player`,
`Kill Action`, `Player Left` and a new map: no packet, every client clears
it itself.

Light State is `[114, 1, bitmask...]`, one bit per player id, low bit first.

Light Config:

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 2 |
| Player ID | Uint8 | 255 = every player without one |
| Reach | Uint8 | |
| Cone | Uint8 | degrees |
| Red, Green, Blue | Uint8 x 3 | the beam's colour |
| Flicker | Uint8 | |

# Daytime and Weather
`Server -> Client` — extension `0x33`, packet 115

| Field Name | Field Type | Notes |
| --- | --- | --- |
| Sub | Uint8 | 0 |
| Time | Uint16 | 0 night, 720 noon; other values are the day |
| Reserved | Uint8 x 4 | the weather in the draft; version 1 has a day and a night |

The server sends it after the agreement and after every map. A client that
agreed and got no Sky yet draws the night: with `Time` 0 the fog and the sky
are black, and the flashlights are the only light. A new server starts by day.

# Player Outfit
`Server -> Client` — extension `0x35`, packet 117

A draft of the aosprotocol specification:

| Sub | Name | Fields |
| --- | --- | --- |
| 0 | Set Outfit | player, outfit, colour (0 team 1, 1 team 2, 255 its own) |
| 1 | Team Outfits | team, outfit pairs |

Outfits: 0 soldier, 7 miner, 64 zombie, 255 the team's outfit. Cosmetic only.
A player's outfit is reset when it leaves and at every map.

# Player Limit
`PACKETLESS` — extension `0xC0`

Announced through `Ext Info` only: the number of players the server accepts
(the World Update's slot count). Nothing else is sent.

# Message Types
`PACKETLESS` — extension `0xC1`

A client that agreed may receive `Chat Message` types 3 (big), 4 (info),
5 (warning) and 6 (error); the others get type 2. Show 3..6 as popups.

# Kick Reason
`PACKETLESS` — extension `0xC2`

A system `Chat Message` from player 255 sent just before a disconnect is the
kick text. Let it leave the ENet queue before disconnecting
(`enet_peer_disconnect_later`, not `enet_peer_disconnect`).

# UTF-8 Chat

Not an extension: a chat line or name beginning with byte `0xFF` and valid
UTF-8 after it is UTF-8. A client announces that it reads it through the
extended version answer's feature bit 0 (below), and only then sends `0xFF`
lines itself.

# Extra Packets

After a plain `Version Send` (34) a server may ask for more, once a
connection:

## Handshake Init
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 31 |
| Total size | 5 bytes |

Answer with the same four bytes (packet 32).

## Extended Version Get
`Server -> Client`

| Info | Value |
| --- | --- |
| Packet ID | 33 |
| Total size | `1 + n` |

The bytes after the id are the property ids the server asks for. Answer with
`Existing Player` (9) whose **first byte is `'x'`** (0x78), then per property
`id, length, payload`, one packet per property, each repeating the ones
before.

| Id | Meaning | Payload |
| --- | --- | --- |
| 0 | application | e.g. `BetterSpades 0.1.5` |
| 1 | language | e.g. `en_us`, `pt_br` |
| 2 | feature flags | bit 0: reads UTF-8 chat |

An unknown id gets an empty payload.

# Other Resources

* [aosprotocol specification](https://github.com/zerospades/aosprotocol).
* [Protocol 0.75](protocol075.md).
