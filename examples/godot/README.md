# Godot examples

Godot 4 GDScript. Two things are provided: the **codecs** (fully usable in
any Godot project) and a **connection** over WebRTC that works in a desktop
build and in a web export.

| File | What it is |
| --- | --- |
| `buf.gd` (`P10Buf`) | byte reader/writer: u8/u16/u32/f32/varint/svarint/bytes/string/vec3/color |
| `proto10.gd` (`P10`) | protocol 1.0 constants, packet builders and decoders |
| `proto075.gd` (`P075`) | 0.75/0.76 constants, CP437 text, `aos://` addresses, packet builders |
| `master.gd` (`SadMaster`) | the server list over HTTPS |
| `webrtc_client.gd` (`SadWebRTC`) | WebRTC signalling through our master, two DataChannels, the `connect` text |
| `proto10_client.gd` (`SadProto10Client`) | handshake, map download, join, body updates |

Copy the files into your project (any folder). `class_name` registers them
globally, so no scene setup is needed for the codecs.

## Quick start

```gdscript
extends Node

var master: SadMaster
var client: SadProto10Client
var rows: Array = []

func _ready() -> void:
    master = SadMaster.new()
    add_child(master)
    master.servers_ready.connect(_on_servers)
    master.list_failed.connect(func(why): push_error("list: " + why))
    master.fetch()

func _on_servers(list: Array) -> void:
    rows = list
    for row in rows:
        print(row["name"], "  ", row["identifier"], "  ", row["players_current"], "/", row["players_max"])
    # join the first 1.0 server that takes browsers
    for row in rows:
        if row.get("game_version", "") == "1.0" and row.get("webrtc", false):
            client = SadProto10Client.new()
            add_child(client)
            client.status.connect(func(t): print("[sad] ", t))
            client.joined.connect(func(id): print("joined as ", id))
            client.join(row)
            return

func _process(_delta: float) -> void:
    # the client polls the peer connection; nothing else is needed
    pass
```

Run the project and watch the Output panel: the list, the Greeting, the map
header, `joined as N`.

## Why WebRTC and not ENet

- **Godot has no usable ENet.** `ENetMultiplayerPeer` is Godot's own
  multiplayer protocol on top of ENet: it cannot set the 32-bit **connect
  data** (`3`, `4` or `0x08101919`) that Ace of Spades decides the protocol
  with. The server refuses the connection before a single game packet.
- **WebRTC is available everywhere Godot runs**: `WebRTCPeerConnection` is
  built into desktop builds (libdatachannel) and into web exports (the
  browser's own WebRTC). It is also the only way a browser can play at all.
- **Only protocol 1.0** goes over WebRTC. 0.75 needs raw UDP: a Godot game
  that wants to join public 0.75 servers needs a GDExtension around a native
  ENet client. `examples/c/enet_client.c` is the C side of such a plugin:
  wrap `enet_host_create`, `enet_host_connect(host, &addr, 2, 0x08101919)`
  and the `enet_host_service` loop in a `RefCounted` and expose
  `connect(host, port, proto)`, `send(bytes, reliable)` and a `packet(bytes)`
  signal. (ENet's connect data is the only reason Godot's own peer cannot be
  used.)

Check that the module is there before shipping:

```gdscript
assert(ClassDB.class_exists("WebRTCPeerConnection"))
```

Some custom Godot builds compile the `webrtc` module out.

## The WebRTC details that matter

1. **Both DataChannels before the offer.** `reliable` (defaults) and
   `unreliable` (`{"ordered": false, "maxRetransmits": 0}`). Any other label
   is closed by the server. Keep a reference to both or Godot closes them.
2. **No trickle ICE.** `SadWebRTC` posts the offer only when
   `get_gathering_state() == GATHERING_STATE_COMPLETE`. The master answers
   with the server's complete SDP.
3. **Text before binary.** After both channels are open, the first message is
   the **text** frame `connect 135272729`. Godot sends text by setting
   `channel.write_mode = WebRTCDataChannel.WRITE_MODE_TEXT` for that one
   `put_packet`; a binary message before it is dropped and the handshake
   times out. `SadWebRTC` does this for you and emits `connected` afterwards.
4. **16 KB per binary message.** One 1.0 packet each; the map fragments are
   8 KB.
5. **The server closes with the text `disconnect <reason>`.** `SadWebRTC`
   turns it into its `closed(reason)` signal.
6. **Use the row's `ice_servers`.** A home NAT needs STUN; a symmetric NAT
   needs the TURN server the server published. `SadWebRTC` falls back to a
   public STUN server only when the row has none.

## Web export

- Serve the page over **HTTPS** (or `localhost`): the master is HTTPS and
  the browser blocks mixed content. It is a static export, no server code.
- The master sends `Access-Control-Allow-Origin: *`, so `HTTPRequest` can
  read the list and post the offer from the page.
- The WebRTC module in a web export is the browser's; `write_mode` becomes
  the `send(string)` / `send(ArrayBuffer)` distinction, which is exactly what
  the server expects.

## Testing the codecs without a server

The golden vectors in `../../protocol10.md` and `../c/test_codec.c` are the test:

```gdscript
func _ready() -> void:
    var pkt := P10.build_block(Vector3i(256, 255, 62), Color8(0x11, 0x22, 0x33))
    var want := PackedByteArray([0x5D, 0x80, 0x04, 0xFE, 0x03, 0x7C, 0x33, 0x22, 0x11])
    assert(pkt == want)

    var chat := P10.chat(1, "gg")
    assert(chat == PackedByteArray([0x46, 0x01, 0x02, 0x67, 0x67]))
```

If those pass, the byte order, the varints and the colour order are right.

## Coverage

The examples stop where the protocol stops being generic: rendering the map
(VXL / ngmap), the game's own physics, and the HUD are yours. `P10.read_*`
covers every server packet a minimal client needs; `P075` covers the 0.75
packets and the `StateData` layout. Both follow the documents, so the tables
there are the reference when a field is missing here.

## Licence

MIT. See `../../LICENSE-CODE`.
