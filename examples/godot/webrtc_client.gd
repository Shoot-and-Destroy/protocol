class_name SadWebRTC
extends Node
## A protocol 1.0 connection over WebRTC, for a Godot desktop build and for
## a web export (see protocol10.md and protocolprotocolmaster.md (WebRTC)).
##
##   var client := SadWebRTC.new()
##   add_child(client)
##   client.packet.connect(_on_packet)          # one 1.0 packet, bytes
##   client.connected.connect(_on_connected)    # channels open, "connect" sent
##   client.failed.connect(func(why): ...)
##   client.closed.connect(func(reason): ...)
##   client.join(row)                           # a row of GET /v1/servers
##
## ENet note: Godot's ENetMultiplayerPeer cannot set the 32-bit connect data
## Ace of Spades uses, so it cannot speak 0.75 or 1.0. WebRTC (this file)
## works on desktop and in the browser; for ENet on desktop, wrap a small
## C library (examples/c/enet_client.c) in a GDExtension.

signal connected
signal packet(bytes: PackedByteArray, reliable: bool)
signal failed(reason: String)
signal closed(reason: int)

const MASTER_URL := "https://master.shootanddestroy.obex.pw"

var _pc: WebRTCPeerConnection
var _reliable: WebRTCDataChannel
var _unreliable: WebRTCDataChannel
var _row: Dictionary
var _join_code := ""
var _offer_sdp := ""
var _posted := false
var _connect_sent := false
var _closed := false
var _http: HTTPRequest


func join(row: Dictionary) -> void:
	leave()
	_row = row
	_join_code = str(row.get("join_code", ""))
	_offer_sdp = ""
	_posted = false
	_connect_sent = false
	_closed = false

	_pc = WebRTCPeerConnection.new()
	var servers := []
	for ice in row.get("ice_servers", []):
		var urls: Variant = ice.get("urls", [])
		if typeof(urls) == TYPE_STRING:
			urls = [urls]
		var entry := {"urls": urls}
		if ice.has("username"):
			entry["username"] = ice["username"]
		if ice.has("credential"):
			entry["credential"] = ice["credential"]
		servers.append(entry)
	# a STUN server is not required: the server publishes its candidates and
	# most home NATs need one to learn their public address, so ours does.
	if servers.is_empty():
		servers.append({"urls": ["stun:stun.l.google.com:19302"]})
	var err := _pc.initialize({"iceServers": servers})
	if err != OK:
		failed.emit("initialize_%d" % err)
		return

	# both channels are created BEFORE the offer. Any other label is closed
	# by the server.
	_reliable = _pc.create_data_channel("reliable")
	_unreliable = _pc.create_data_channel("unreliable", {"ordered": false, "maxRetransmits": 0})
	if _reliable == null or _unreliable == null:
		failed.emit("create_channel")
		return
	_reliable.write_mode = WebRTCDataChannel.WRITE_MODE_BINARY
	_unreliable.write_mode = WebRTCDataChannel.WRITE_MODE_BINARY

	_pc.session_description_created.connect(_on_sdp)
	_pc.peer_connection_state_changed.connect(_on_state)
	if _pc.create_offer() != OK:
		failed.emit("create_offer")


## Play with a friend: the six-character code from the host.
func join_by_code(code: String) -> void:
	join({"join_code": code})


func _on_sdp(type: String, sdp: String) -> void:
	_pc.set_local_description(type, sdp)
	if type == "offer":
		_offer_sdp = sdp


func _post_offer() -> void:
	_posted = true
	var id := str(_row.get("id", ""))
	var url := ""
	if not _join_code.is_empty():
		url = "%s/v1/join/%s/offer" % [MASTER_URL, _join_code]
	elif not id.is_empty():
		url = "%s/v1/servers/%s/offer" % [MASTER_URL, id]
	else:
		failed.emit("no_id")
		return

	_http = HTTPRequest.new()
	_http.timeout = 20.0 # the master holds the offer while the server answers
	add_child(_http)
	_http.request_completed.connect(_on_answer)
	var body := JSON.stringify({"sdp": _offer_sdp})
	var err := _http.request(url, ["Content-Type: application/json", "Accept: application/json"],
		HTTPClient.METHOD_POST, body)
	if err != OK:
		failed.emit("offer_failed_%d" % err)


func _on_answer(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	if result != HTTPRequest.RESULT_SUCCESS:
		failed.emit("offer_unreachable")
		return
	if code == 429:
		failed.emit("too_many_offers")
		return
	if code == 409:
		failed.emit("no_webrtc")
		return
	if code == 503:
		var why: Variant = JSON.parse_string(body.get_string_from_utf8())
		if typeof(why) == TYPE_DICTIONARY:
			failed.emit(str(why.get("error", "refused")))
		else:
			failed.emit("refused")
		return
	if code == 504:
		failed.emit("no_answer")
		return
	if code != 200:
		failed.emit("offer_http_%d" % code)
		return
	var answer: Variant = JSON.parse_string(body.get_string_from_utf8())
	if typeof(answer) != TYPE_DICTIONARY or not answer.has("sdp"):
		failed.emit("bad_answer")
		return
	_pc.set_remote_description("answer", str(answer["sdp"]))


func _on_state(state: int) -> void:
	if state == WebRTCPeerConnection.STATE_FAILED:
		failed.emit("connection_failed")
	elif state == WebRTCPeerConnection.STATE_CLOSED:
		if not _closed:
			_closed = true
			closed.emit(0)


func _process(_delta: float) -> void:
	if _pc == null:
		return
	_pc.poll()

	# no trickle ICE: the offer is posted only once every candidate is in it
	if not _posted and not _offer_sdp.is_empty() and _pc.get_gathering_state() == WebRTCPeerConnection.GATHERING_STATE_COMPLETE:
		_post_offer()

	if _reliable == null or _unreliable == null:
		return
	if _reliable.get_ready_state() != WebRTCDataChannel.STATE_OPEN:
		return
	if _unreliable.get_ready_state() != WebRTCDataChannel.STATE_OPEN:
		return

	if not _connect_sent:
		# the server takes this text frame before any binary packet
		_reliable.write_mode = WebRTCDataChannel.WRITE_MODE_TEXT
		_reliable.put_packet(("connect %d" % P10.CONNECT_MAGIC).to_utf8_buffer())
		_reliable.write_mode = WebRTCDataChannel.WRITE_MODE_BINARY
		_connect_sent = true
		connected.emit()

	_drain(_reliable, true)
	_drain(_unreliable, false)


func _drain(channel: WebRTCDataChannel, reliable: bool) -> void:
	while channel.get_available_packet_count() > 0:
		var bytes := channel.get_packet()
		if channel.was_string_packet():
			var text := bytes.get_string_from_utf8()
			if text.begins_with("disconnect"):
				var parts := text.split(" ")
				var reason := int(parts[1]) if parts.size() > 1 and parts[1].is_valid_int() else 0
				if not _closed:
					_closed = true
					closed.emit(reason)
			continue
		packet.emit(bytes, reliable)


## Send one 1.0 packet. `reliable` must match the channel the packet belongs
## to (movement goes unreliable, everything else reliable).
func send(bytes: PackedByteArray, reliable := true) -> void:
	if not _connect_sent:
		return
	var channel: WebRTCDataChannel = _reliable if reliable else _unreliable
	if channel == null or channel.get_ready_state() != WebRTCDataChannel.STATE_OPEN:
		return
	if bytes.size() > 16384:
		push_warning("a WebRTC message over 16 KB is refused by the server")
		return
	channel.put_packet(bytes)


func leave() -> void:
	if _http != null:
		_http.queue_free()
		_http = null
	if _pc != null:
		_pc.close()
	_pc = null
	_reliable = null
	_unreliable = null
