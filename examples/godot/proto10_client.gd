class_name SadProto10Client
extends Node
## The 1.0 session on top of SadWebRTC: handshake, map download, join.
##
##   var client := SadProto10Client.new()
##   add_child(client)
##   client.joined.connect(func(id): print("we are player ", id))
##   client.join(row)     # a row of the master list
##
## This stops at "we are in the game". Add the pieces you need from 1.0.md:
## entities, terrain, chat, game mode.

signal joined(player_id: int)
signal status(text: String)
signal failed(reason: String)

var rtc: SadWebRTC
var self_id := -1

var _header := {}
var _map := PackedByteArray()
var _map_size := 0
var _in_game := false
var _body_clock := 0.0


func join(row: Dictionary) -> void:
	rtc = SadWebRTC.new()
	add_child(rtc)
	rtc.packet.connect(_on_packet)
	rtc.connected.connect(_on_transport_open)
	rtc.failed.connect(func(why: String) -> void: failed.emit(why))
	rtc.closed.connect(func(reason: int) -> void: status.emit("connection closed: %d" % reason))
	rtc.join(row)


func _on_transport_open() -> void:
	status.emit("connected to the host, waiting for the Greeting")


func _on_packet(bytes: PackedByteArray, _reliable: bool) -> void:
	if bytes.is_empty():
		return
	match bytes[0]:
		P10.GREETING:
			_on_greeting(bytes)
		P10.GAME_STATE_HEADER:
			_header = P10.read_map(bytes)
			_map_size = int(_header.get("map-bytes", "0"))
			_map = PackedByteArray()
			status.emit("map %s (%s), %d bytes" % [_header.get("map-name", "?"),
				_header.get("map-format", "?"), _map_size])
		P10.MAP_DATA:
			_on_map_data(bytes)
		P10.MAP_DATA_FINAL:
			# decode the map here (VXL is zlib + the 0.75 VXL bytes; ngmap has
			# its own codec), apply the TerrainUpdates kept meanwhile, then:
			rtc.send(PackedByteArray([P10.MAP_DATA_ACKNOWLEDGE]))
			status.emit("map received")
		P10.GAME_STATE_FINAL:
			rtc.send(P10.generic(["join", "0", "0"])) # team 1, rifle
		P10.GENERIC_COMMAND:
			_on_generic(bytes)
		P10.KICK:
			failed.emit(P10.read_kick(bytes))
		P10.CHAT_BROADCAST:
			var chat := P10.read_chat_broadcast(bytes)
			if not chat.is_empty():
				status.emit("chat: %s" % chat["text"])
		P10.ENTITY_UPDATE:
			_on_entities(bytes)
		P10.TERRAIN_UPDATE:
			pass # P10.read_terrain(bytes)
		_:
			pass # unknown ids are ignored, never a reason to disconnect


func _on_greeting(bytes: PackedByteArray) -> void:
	var nonce := P10.read_greeting(bytes)
	if nonce.is_empty():
		failed.emit("bad Greeting")
		return
	rtc.send(P10.initiate("godot-protocol-example 1.0", OS.get_name(), OS.get_locale(), "GodotPlayer",
		Crypto.new().generate_random_bytes(256), 0)) # mapQuality 0 = VXL
	# certificates with isValid = 0; our server asks for a token only from a
	# non-local UDP address, which does not apply to WebRTC
	rtc.send(P10.client_certificate())


func _on_map_data(bytes: PackedByteArray) -> void:
	var r := P10Buf.new(bytes)
	r.get_u8()
	var fragment := r.get_bytes()
	if r.error:
		return
	_map.append_array(fragment)


func _on_generic(bytes: PackedByteArray) -> void:
	var r := P10Buf.new(bytes)
	r.get_u8()
	var parts: Array[String] = []
	while r.has_more() and not r.error:
		parts.append(r.get_string())
	if parts.is_empty():
		return
	match parts[0]:
		"local-player":
			if parts.size() > 1:
				self_id = int(parts[1])
				_in_game = true
				joined.emit(self_id)
				status.emit("in the game as player %d" % self_id)
		"hit":
			status.emit("hit marker")
		"kill":
			status.emit("kill marker")
		_:
			pass


func _on_entities(bytes: PackedByteArray) -> void:
	var r := P10Buf.new(bytes)
	if r.get_u8() != P10.ENTITY_UPDATE:
		return
	var forced := r.get_u8() != 0
	while r.has_more():
		var item := P10.read_entity_item(r)
		if r.error or item.is_empty():
			break
		if item.get("type", -1) == P10.ENT_PLAYER:
			pass # feed your world: name from PlayerUpdate, body from here
		if int(item.get("id", -1)) == self_id and not forced and (int(item.get("mask", 0)) & P10.EU_CREATE) == 0:
			continue # our own body is predicted locally


## Report your body at 30 Hz, unreliable. Coordinates here are map
## coordinates (z down): use P075.pos_from_engine() if your world is Y-up.
func _process(delta: float) -> void:
	if not _in_game or rtc == null:
		return
	_body_clock += delta
	if _body_clock < 1.0 / 30.0:
		return
	_body_clock = 0.0
	var my_body := {
		"id": self_id,
		"mask": P10.EU_TRAJECTORY | P10.EU_PLAYER_INPUT | P10.EU_TOOL,
		"tool": P10.TOOL_WEAPON1,
		"input": {"flags": 0, "stance": P10.STANCE_STAND, "xmove": 0, "ymove": 0},
		"trajectory": {
			"type": P10.TRAJ_PLAYER,
			"origin": Vector3(256.0, 256.0, 40.0),
			"velocity": Vector3.ZERO,
			"euler": Vector3(0.0, 0.0, 0.0),
		},
	}
	rtc.send(P10.client_entity([my_body]), false)
