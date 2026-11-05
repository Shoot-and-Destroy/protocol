This page documents the networking protocols of **Ace of Spades 0.75/0.76**
and **Shoot and Destroy 1.0** (ngspades), for anyone writing a client or a
server that should play with them.

# Overview

[Ace of Spades](http://buildandshoot.com/) uses the [ENet networking
library](http://enet.bespin.org/Features.html) for all server-client
communication. The 0.75 information comes from the original
[pyspades](https://github.com/piqueserver/piqueserver) source and the clients
that speak it; 1.0 starts from OpenSpades' abandoned
[ngspades](https://github.com/yvt/openspades) branch, with its defects fixed.

## Versions

* [0.75 and 0.76](protocol075.md) — the classic protocol public servers
  speak.
* [1.0](protocol10.md) — the protocol of the Shoot and Destroy server.
* [Extensions](extensions.md) — the 0.75 extension registry.
* [Master Server](protocolmaster.md) and [Ping](protocolping.md) — the lists,
  WebRTC signalling, tokens and the raw UDP queries.
* [Examples](examples/c/README.md) — C and Godot code for the codecs and the
  connections.

## Implementers

| Project | 0.75 | 1.0 |
| --- | --- | --- |
| [Shoot and Destroy](https://github.com/ngspades) client and server | Yes | Yes |
| [BetterSpades](https://github.com/xtreme8000/BetterSpades) | Yes | No |
| [OpenSpades](https://github.com/yvt/openspades) | Yes | No |
| [piqueserver](https://github.com/piqueserver/piqueserver) | Yes | No |
| [SpadesX](https://github.com/SpadesX/SpadesX) | Yes | No |

# Other Resources

* [VXL file format](http://silverspaceship.com/aosmap/aos_file_format.html) —
  the map format both protocols send.
* [KVX file format](https://web.archive.org/web/20100102023608/http://mystaddict.tlayeh.com/Computer%20Camp/Slab6/slab6.txt) —
  the model format, based on Slab6's.
* [piqueserver](https://github.com/piqueserver/piqueserver) and
  [BetterSpades](https://github.com/xtreme8000/BetterSpades) — reference
  implementations of 0.75.
