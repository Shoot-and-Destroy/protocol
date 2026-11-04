class_name P075
extends RefCounted
## Ace of Spades 0.75/0.76: constants, text, addresses and packet builders
## (see 0.75.md). Uses P10Buf as a plain little-endian byte buffer.

const CONNECT := 3
const CONNECT_076 := 4
const PORT_DEFAULT := 32887

const POSITION := 0
const ORIENTATION := 1
const WORLDUPDATE := 2
const INPUT := 3
const WEAPONINPUT := 4
const SETHP := 5
const HIT := 5
const GRENADE := 6
const SETTOOL := 7
const SETCOLOR := 8
const EXISTINGPLAYER := 9
const SHORTPLAYER := 10
const MOVEOBJECT := 11
const CREATEPLAYER := 12
const BLOCKACTION := 13
const BLOCKLINE := 14
const STATEDATA := 15
const KILLACTION := 16
const CHATMESSAGE := 17
const MAPSTART := 18
const MAPCHUNK := 19
const PLAYERLEFT := 20
const TERRITORYCAPTURE := 21
const PROGRESSBAR := 22
const INTELCAPTURE := 23
const INTELPICKUP := 24
const INTELDROP := 25
const RESTOCK := 26
const FOGCOLOR := 27
const WEAPONRELOAD := 28
const CHANGETEAM := 29
const CHANGEWEAPON := 30
const MAPCACHED := 31
const HANDSHAKEINIT := 31
const HANDSHAKERETURN := 32
const VERSIONGET := 33
const VERSIONSEND := 34
const EXTINFO := 60
const PLAYERPROPERTIES := 64
const FLASHLIGHT := 114

const TEAM_1 := 0
const TEAM_2 := 1
const TEAM_SPECTATOR := 255

const RIFLE := 0
const SMG := 1
const SHOTGUN := 2

const TOOL_SPADE := 0
const TOOL_BLOCK := 1
const TOOL_GUN := 2
const TOOL_GRENADE := 3

const BUILD := 0
const DESTROY := 1
const SPADE := 2
const GRENADE_ACTION := 3
const GUN_DESTROY := 4
const SPADE_ONE := 5

const HIT_TORSO := 0
const HIT_HEAD := 1
const HIT_ARMS := 2
const HIT_LEGS := 3
const HIT_MELEE := 4

const CHAT_ALL := 0
const CHAT_TEAM := 1
const CHAT_SYSTEM := 2


# ---------------------------------------------------------------- text

# The upper half of code page 437, U+0080..U+00FF.
const CP437_HIGH := [
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF,
	0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA,
	0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557,
	0x255D, 0x255C, 0x255B, 0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559,
	0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4,
	0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
	0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
]


static func cp437_to_utf8(bytes: PackedByteArray) -> String:
	var out := ""
	for v in bytes:
		if v == 0:
			break
		if v < 0x80:
			out += String.chr(v)
		else:
			out += String.chr(CP437_HIGH[v - 0x80])
	return out


static func utf8_to_cp437(text: String) -> PackedByteArray:
	var out := PackedByteArray()
	for i in text.length():
		var cp := text.unicode_at(i)
		if cp < 0x80:
			out.append(cp)
			continue
		var found := CP437_HIGH.find(cp)
		out.append(0x3F if found == -1 else 0x80 + found) # '?' when it does not fit
	return out


## A 0.75 text: `0xFF` + valid UTF-8 is UTF-8, anything else CP437.
static func text_to_utf8(bytes: PackedByteArray) -> String:
	if bytes.size() >= 2 and bytes[0] == 0xFF:
		return bytes.slice(1).get_string_from_utf8()
	# a server that sends UTF-8 without the prefix: Godot decodes invalid
	# UTF-8 to U+FFFD, so compare the round trip
	var as_utf8 := bytes.get_string_from_utf8()
	if as_utf8.to_utf8_buffer() == bytes:
		return as_utf8
	return cp437_to_utf8(bytes)


# ---------------------------------------------------------------- address

## aos://<word>[:port], aos://a.b.c.d:port, host:port.
## Returns {"ip": String, "port": int} or {} when it cannot be read.
## (A hostname needs DNS, which is the caller's job: use IP.resolve.)
static func parse_address(text: String) -> Dictionary:
	var s := text.strip_edges()
	if s.begins_with("aos://"):
		s = s.substr(6)
	if s.begins_with("["):
		return {} # IPv6 is not a 0.75 address; 1.0 handles it
	var port := PORT_DEFAULT
	var colon := s.rfind(":")
	if colon != -1:
		var tail := s.substr(colon + 1)
		if tail.is_valid_int():
			port = int(tail)
			s = s.substr(0, colon)
	if s.is_valid_int():
		var word := int(s)
		return {
			"ip": "%d.%d.%d.%d" % [word & 0xFF, (word >> 8) & 0xFF, (word >> 16) & 0xFF, (word >> 24) & 0xFF],
			"port": port,
		}
	var parts := s.split(".")
	if parts.size() == 4:
		for p in parts:
			if not p.is_valid_int() or int(p) > 255:
				return {}
		return {"ip": s, "port": port}
	return {}


# ---------------------------------------------------------------- coords

## wire (z down) -> Godot (Y up): (x, 63 - z, y)
static func pos_to_engine(v: Vector3) -> Vector3:
	return Vector3(v.x, 63.0 - v.z, v.y)


static func pos_from_engine(v: Vector3) -> Vector3:
	return Vector3(v.x, v.z, 63.0 - v.y)


static func dir_to_engine(v: Vector3) -> Vector3:
	return Vector3(v.x, -v.z, v.y)


static func dir_from_engine(v: Vector3) -> Vector3:
	return Vector3(v.x, v.z, -v.y)


# ---------------------------------------------------------------- builders

static func position(v: Vector3) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(POSITION)
	w.put_vec3(v)
	return w.data


static func orientation(v: Vector3) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(ORIENTATION)
	w.put_vec3(v)
	return w.data


static func input(player: int, keys: int) -> PackedByteArray:
	return PackedByteArray([INPUT, player & 0xFF, keys & 0xFF])


static func weapon_input(player: int, primary: bool, secondary: bool) -> PackedByteArray:
	var buttons := 0
	if primary:
		buttons |= 1
	if secondary:
		buttons |= 2
	return PackedByteArray([WEAPONINPUT, player & 0xFF, buttons])


static func hit(victim: int, hit_type: int) -> PackedByteArray:
	return PackedByteArray([HIT, victim & 0xFF, hit_type & 0xFF])


static func grenade(player: int, fuse: float, origin: Vector3, velocity: Vector3) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(GRENADE)
	w.put_u8(player)
	w.put_f32(fuse)
	w.put_vec3(origin)
	w.put_vec3(velocity) # blocks per 1/32 s
	return w.data


static func set_tool(player: int, tool: int) -> PackedByteArray:
	return PackedByteArray([SETTOOL, player & 0xFF, tool & 0xFF])


static func set_color(player: int, color: Color) -> PackedByteArray:
	return PackedByteArray([SETCOLOR, player & 0xFF, color.b8, color.g8, color.r8])


static func join(player: int, team: int, weapon: int, tool: int, kills: int, color: Color,
		name: String) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(EXISTINGPLAYER)
	w.put_u8(player)
	w.put_u8(team & 0xFF)
	w.put_u8(weapon)
	w.put_u8(tool)
	w.put_u32(kills)
	w.put_u8(color.b8)
	w.put_u8(color.g8)
	w.put_u8(color.r8)
	w.put_raw(utf8_to_cp437(name.substr(0, 15)))
	w.put_u8(0)
	return w.data


static func block_action(player: int, type: int, cell: Vector3i) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(BLOCKACTION)
	w.put_u8(player)
	w.put_u8(type)
	w.put_u32(cell.x)
	w.put_u32(cell.y)
	w.put_u32(cell.z)
	return w.data


static func block_line(player: int, from: Vector3i, to: Vector3i) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(BLOCKLINE)
	w.put_u8(player)
	w.put_u32(from.x)
	w.put_u32(from.y)
	w.put_u32(from.z)
	w.put_u32(to.x)
	w.put_u32(to.y)
	w.put_u32(to.z)
	return w.data


## text is CP437 bytes; pass 0xFF + UTF-8 to send UTF-8 to a server that
## agreed to it.
static func chat(player: int, channel: int, text: PackedByteArray) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(CHATMESSAGE)
	w.put_u8(player)
	w.put_u8(channel)
	w.put_raw(text)
	w.put_u8(0)
	return w.data


static func weapon_reload(player: int, ammo: int, reserve: int) -> PackedByteArray:
	return PackedByteArray([WEAPONRELOAD, player & 0xFF, ammo & 0xFF, reserve & 0xFF])


static func change_team(player: int, team: int) -> PackedByteArray:
	return PackedByteArray([CHANGETEAM, player & 0xFF, team & 0xFF])


static func change_weapon(player: int, weapon: int) -> PackedByteArray:
	return PackedByteArray([CHANGEWEAPON, player & 0xFF, weapon & 0xFF])


static func map_cached(cached: bool) -> PackedByteArray:
	return PackedByteArray([MAPCACHED, 1 if cached else 0])


static func handshake_return(challenge: int) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(HANDSHAKERETURN)
	w.put_u32(challenge)
	return w.data


static func version_send(client: String, major: int, minor: int, revision: int,
		os: String) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(VERSIONSEND)
	w.put_u8(client.unicode_at(0))
	w.put_u8(major)
	w.put_u8(minor)
	w.put_u8(revision)
	var fixed := os.to_utf8_buffer()
	fixed.resize(64)
	w.put_raw(fixed)
	return w.data


## pairs: [[id, version], ...] you accept
static func extinfo(pairs: Array) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(EXTINFO)
	w.put_u8(pairs.size())
	for pair in pairs:
		w.put_u8(int(pair[0]))
		w.put_u8(int(pair[1]))
	return w.data


static func flashlight(player: int, on: bool) -> PackedByteArray:
	return PackedByteArray([FLASHLIGHT, 0, player & 0xFF, 1 if on else 0])


# ---------------------------------------------------------------- decoding

static func read_create_player(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != CREATEPLAYER:
		return {}
	var out := {"id": r.get_u8(), "weapon": r.get_u8(), "team": r.get_u8()}
	out["pos"] = r.get_vec3()
	var name: PackedByteArray = r.get_raw(bytes.size() - r.pos)
	out["name"] = text_to_utf8(name)
	return out


static func read_existing_player(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != EXISTINGPLAYER:
		return {}
	var out := {"id": r.get_u8(), "team": r.get_u8(), "weapon": r.get_u8(), "tool": r.get_u8()}
	if out["team"] >= 128:
		out["team"] -= 256
	out["kills"] = r.get_u32()
	out["color"] = r.get_color()
	out["name"] = text_to_utf8(r.get_raw(bytes.size() - r.pos))
	return out


## CTF: scores, limit, two intels ({held, carrier} or {pos}), two tents.
## TC: a list of {pos, team}.
static func read_state_data(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != STATEDATA:
		return {}
	var out := {}
	out["player_id"] = r.get_u8()
	out["fog"] = r.get_color()
	out["team1_color"] = r.get_color()
	out["team2_color"] = r.get_color()
	out["team1_name"] = text_to_utf8(r.get_raw(10)).strip_edges()
	out["team2_name"] = text_to_utf8(r.get_raw(10)).strip_edges()
	var mode := r.get_u8()
	out["mode"] = mode
	if mode == 0:
		out["team1_score"] = r.get_u8()
		out["team2_score"] = r.get_u8()
		out["capture_limit"] = r.get_u8()
		var bits := r.get_u8()
		var intels := []
		for i in 2:
			var raw := r.get_raw(12)
			if bits & (1 << i):
				intels.append({"held": true, "carrier": raw[0]})
			else:
				intels.append({"held": false, "pos": Vector3(raw.decode_float(0), raw.decode_float(4),
					raw.decode_float(8))})
		out["intels"] = intels
		out["team1_base"] = r.get_vec3()
		out["team2_base"] = r.get_vec3()
	else:
		var count := r.get_u8()
		var territories := []
		for i in count:
			territories.append({"pos": r.get_vec3(), "team": r.get_u8()})
		out["territories"] = territories
	if r.error:
		return {}
	return out


## one entry of a WorldUpdate: {"pos", "ori"} (0.76 also has "id")
static func world_entry(bytes: PackedByteArray, index: int, v076 := false) -> Dictionary:
	var size: int = 25 if v076 else 24
	var at := 1 + size * index
	if at + size > bytes.size():
		return {}
	var r := P10Buf.new(bytes)
	r.pos = at
	var out := {}
	if v076:
		out["id"] = r.get_u8()
	out["pos"] = r.get_vec3()
	out["ori"] = r.get_vec3()
	return out
