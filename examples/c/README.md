# C examples

| File | What it is |
| --- | --- |
| `proto10.h`, `proto10.c` | the complete 1.0 primitive codec, plus encoders/decoders for the packets a client needs |
| `test_codec.c` | golden vectors from `protocol10.md` and round trips; `make test` |
| `proto075.h`, `proto075.c` | the 0.75/0.76 packed structs, address parsing, CP437 text and packet builders |
| `enet_client.c` | a real connection: range coder, connect data, both protocols, map download, join, body updates |
| `master_http.c` | `GET /v1/servers` with libcurl and a minimal JSON row reader |

```sh
make test        # codec only, no dependencies
make enet        # needs ENet 1.3 (pkg-config enet)
make master      # needs libcurl
./enet_client 203.0.113.5 32887 1     # host port 1 = 1.0, 0 = 0.75
```

## The codec

The codec is deliberately boring: one `p10_reader` with a sticky error flag,
one `p10_writer` with a fixed buffer, and functions that return the decoded
value. The rules implemented (and required) are in `protocol10.md`, Data Types and decoder notes:

* little-endian fixed-width values, LEB128 varints (max 10 bytes), zigzag
  signed varints;
* length-prefixed bytes and strings, at most 1 MiB, no NUL inside;
* masks are validated: an unknown bit is an error, because its payload
  cannot be measured;
* `EntityDie` checks for the end of the packet before reading damage info;
* compact trajectories decode into ordinary player trajectories;
* `TerrainUpdate` uses R, G, B while every other colour is B, G, R.

Copy the files into your project and keep the same function names if you can
— every example and test in this repository uses them.

## ENet notes

`enet_client.c` is written against **ENet 1.3**, the one every Ace of Spades
program uses. Our fork (github.com/ngspades/enet, ENet 2's IPv6 API)
changes three calls:

```c
enet_host_create(ENET_ADDRESS_TYPE_ANY, 1, 2, 0, 0);           /* not NULL */
enet_address_set_host(&address, ENET_ADDRESS_TYPE_IPV4, name); /* type first */
/* ENetAddress.host is a union: host.v4, host.host32 */
```

Everything else is the same. Our servers are dual-stack: one `ANY` host
accepts IPv4 and IPv6, and an IPv4 peer arrives as `::ffff:a.b.c.d` (turn it
back before you log or ban it).

The range coder is not optional: `enet_host_compress_with_range_coder` on
both sides, and keep decompression on, or the map and every packet after it
are garbage.

## WebRTC in C

We do not ship a C WebRTC example because the browser side is the one that
matters and a native client can use ENet. If you need WebRTC from C, use
[libdatachannel](https://github.com/paullouisageneau/libdatachannel) (the one
our server uses) and follow `protocolprotocolmaster.md` (WebRTC) and `protocol10.md` (WebRTC):
two DataChannels (`reliable`, `unreliable`), one SDP offer/answer through the
master, the text `connect 135272729`, then one 1.0 packet per binary message.

## Licence

MIT. See `../../LICENSE-CODE`.
