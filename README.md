# Shoot and Destroy Protocol

Documentation of the Ace of Spades **0.75/0.76** wire protocol and of
**protocol 1.0**, the protocol of the Shoot and Destroy server, with C and
Godot examples.

Start at [index.md](index.md), or go straight to the protocol you need:

| Document | Covers |
| --- | --- |
| [protocol075.md](protocol075.md) | 0.75 and 0.76: connection, every packet, game rules, the server list, how to write a server |
| [protocol10.md](protocol10.md) | 1.0: connection over ENet and WebRTC, every packet, entities, trajectories, game mode state, a minimum client |
| [extensions.md](extensions.md) | the 0.75 extension negotiation and registry |
| [protocolmaster.md](protocolmaster.md) | the classic master and our HTTP master: list, announcing, WebRTC signalling, join codes, connect tokens |
| [protocolping.md](protocolping.md) | `HELLO`, `HELLOLAN`, `HELLOINFO`, map previews |
| [examples/c](examples/c/README.md), [examples/godot](examples/godot/README.md) | C and GDScript codecs and connections |

## Why should I care?

If you want your game, client or server to play with Ace of Spades, this is
the whole wire: what the server sends, what it expects, and what a client has
to send back. The 0.75 part is what public servers speak; 1.0 is what ours
speaks, and the only way a browser can play.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). A wire change is a protocol change:
open an issue first.

## Licence

The text is CC BY 4.0; the example code is MIT. See [LICENSE](LICENSE).
