class_name SadMaster
extends Node
## The server list (see protocolmaster.md).
##
##   var master := SadMaster.new()
##   add_child(master)
##   master.servers_ready.connect(_on_servers)
##   master.list_failed.connect(func(why): print("list: ", why))
##   master.fetch()

signal servers_ready(rows: Array)
signal list_failed(reason: String)

const DEFAULT_URL := "https://master.shootanddestroy.obex.pw/v1/servers"
const CLASSIC_URL := "https://services.buildandshoot.com/serverlist.json"

var url := DEFAULT_URL
var _http: HTTPRequest


func fetch(target := "") -> void:
	if target.is_empty():
		target = url
	url = target
	if _http != null:
		_http.queue_free()
	_http = HTTPRequest.new()
	_http.timeout = 10.0
	add_child(_http)
	_http.request_completed.connect(_on_done)
	var err := _http.request(url, ["Accept: application/json"], HTTPClient.METHOD_GET)
	if err != OK:
		list_failed.emit("request_failed_%d" % err)


func _on_done(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	if result != HTTPRequest.RESULT_SUCCESS:
		list_failed.emit("unreachable")
		return
	if code != 200:
		list_failed.emit("http_%d" % code)
		return
	var parsed: Variant = JSON.parse_string(body.get_string_from_utf8())
	if typeof(parsed) != TYPE_ARRAY:
		list_failed.emit("not_a_list") # a captive portal or a parked domain
		return
	servers_ready.emit(parsed)


## Merge our list with a classic 0.75 list: a server in both appears once,
## as ours. Rows are matched by address.
static func merge(ours: Array, classic: Array) -> Array:
	var seen := {}
	var out := []
	for row in ours:
		var key := str(row.get("identifier", ""))
		if key.is_empty():
			continue
		seen[key] = true
		out.append(row)
	for row in classic:
		var key := str(row.get("identifier", ""))
		if key.is_empty() or seen.has(key):
			continue
		seen[key] = true
		out.append(row)
	return out


## True when this row has a slot for a player and is not full.
static func has_room(row: Dictionary) -> bool:
	return int(row.get("players_current", 0)) < int(row.get("players_max", 0))


## The address to dial for a row, or "" when the row has none.
static func address_of(row: Dictionary) -> String:
	var a := str(row.get("identifier", ""))
	if a.is_empty():
		a = str(row.get("identifier6", ""))
	return a
