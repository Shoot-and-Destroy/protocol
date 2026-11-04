class_name P10
extends RefCounted
## Protocol 1.0: constants, packet builders and decoders (see 1.0.md).
##
## Everything here works on PackedByteArray, so it is identical in a desktop
## build and a web export. The transport (ENet or WebRTC) is separate.

const CONNECT_MAGIC := 135272729 # 0x08101919
const PROTOCOL_NAME := "WorldOfSpades 1.0"
const PROTOCOL_MINOR := 3

# packet ids
const GREETING := 1
const INITIATE_CONNECTION := 2
const SERVER_CERTIFICATE := 3
const CLIENT_CERTIFICATE := 4
const KICK := 5
const GAME_STATE_HEADER := 6
const MAP_DATA := 7
const GAME_STATE_FINAL := 8
const GENERIC_COMMAND := 9
const MAP_DATA_ACKNOWLEDGE := 10
const MAP_DATA_FINAL := 11
const ENTITY_UPDATE := 30
const ENTITY_EVENT := 31
const ENTITY_DIE := 32
const ENTITY_REMOVE := 33
const CLIENT_SIDE_ENTITY_UPDATE := 35
const TERRAIN_UPDATE := 40
const PLAYER_UPDATE := 50
const PLAYER_REMOVE := 51
const PLAYER_ACTION := 60
const CHAT_MESSAGE := 70
const CHAT_BROADCAST := 71
const GAME_MODE_STATE := 80
const HIT_ENTITY := 90
const HIT_TERRAIN := 91
const DAMAGE := 92
const BUILD_BLOCK := 93
const BUILD_LINE := 94
const THROW_GRENADE := 95

# entity types
const ENT_PLAYER := 0
const ENT_GRENADE := 1
const ENT_ROCKET := 2
const ENT_COMMAND_POST := 3
const ENT_FLAG := 4
const ENT_CHECKPOINT := 5
const ENT_VEHICLE := 6
const ENT_SENTRY := 7
const ENT_LANDMINE := 8
const ENT_DYNAMITE := 9
const ENT_DRILL := 10
const ENT_AMMO_CRATE := 11
const ENT_HEALTH_CRATE := 12
const ENT_COUNT := 13

# entity update mask
const EU_CREATE := 1 << 0
const EU_FLAGS := 1 << 1
const EU_TRAJECTORY := 1 << 2
const EU_PLAYER_INPUT := 1 << 3
const EU_TOOL := 1 << 4
const EU_BLOCK_COLOR := 1 << 5
const EU_HEALTH := 1 << 6
const EU_KNOWN := 0x7F

# player update mask (Score is bit 1, Flags bit 2)
const PU_CREATE := 1 << 0
const PU_SCORE := 1 << 1
const PU_FLAGS := 1 << 2
const PU_TEAM := 1 << 3
const PU_STATS := 1 << 4
const PU_PING := 1 << 5
const PU_PERKS := 1 << 6
const PU_KNOWN := 0x7F

# entity flags
const EF_PLAYER_CLIP := 1 << 0
const EF_WEAPON_CLIP := 1 << 1
const EF_FLY := 1 << 2
const EF_JETPACK := 1 << 3

# input flags
const IN_PRIMARY := 1 << 0
const IN_SECONDARY := 1 << 1
const IN_CHAT := 1 << 2
const IN_SPRINT := 1 << 3
const IN_JETPACK := 1 << 4
const STANCE_STAND := 0
const STANCE_CROUCH := 1
const STANCE_PRONE := 2

# tools
const TOOL_MELEE := 0
const TOOL_BLOCK := 1
const TOOL_WEAPON1 := 2
const TOOL_WEAPON2 := 3
const TOOL_WEAPON3 := 4

# trajectory types
const TRAJ_CONSTANT := 0
const TRAJ_LINEAR := 1
const TRAJ_GRAVITY := 2
const TRAJ_PLAYER := 3
const TRAJ_RIGID_BODY := 4
const TRAJ_PLAYER_COMPACT := 5

# damage types
const DMG_UNKNOWN := 0
const DMG_SUICIDE := 1
const DMG_FALL := 2
const DMG_POISON := 3
const DMG_MELEE := 4
const DMG_DIG := 5
const DMG_BLOCK := 6
const DMG_WEAPON1 := 7
const DMG_WEAPON2 := 8
const DMG_WEAPON3 := 9
const DMG_GRENADE := 10

const TEAM_1 := 0
const TEAM_2 := 1
const TEAM_SPECTATOR := 255
const NO_OWNER := 0xFFFFFFFF

# entity events
const EV_JUMP := 0
const EV_RELOAD_WEAPON := 1
const EV_SUICIDE := 2
const EV_EXPLODE := 3
const EV_FIRE := 4
const EV_USE_PERK := 5


# ---------------------------------------------------------------- builders

static func initiate(package: String, environment: String, locale: String, player_name: String,
		nonce: PackedByteArray, map_quality: int) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(INITIATE_CONNECTION)
	w.put_string(PROTOCOL_NAME)
	w.put_u16(1)
	w.put_u16(PROTOCOL_MINOR)
	w.put_u16(0)
	w.put_string(package)
	w.put_string(environment)
	w.put_string(locale)
	w.put_string(player_name)
	w.put_bytes(nonce)
	w.put_u8(map_quality) # 0 = VXL, 1..100 = ngmap
	return w.data


static func client_certificate(cert := PackedByteArray(), signature := PackedByteArray()) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(CLIENT_CERTIFICATE)
	if cert.is_empty():
		w.put_u8(0)
	else:
		w.put_u8(1)
		w.put_bytes(cert)
		w.put_bytes(signature)
	return w.data


static func generic(parts: Array) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(GENERIC_COMMAND)
	for part in parts:
		w.put_string(str(part))
	return w.data


static func client_entity(items: Array) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(CLIENT_SIDE_ENTITY_UPDATE)
	for item in items:
		_write_entity_item(w, item)
	return w.data


static func _write_entity_item(w: P10Buf, item: Dictionary) -> void:
	var mask: int = item.get("mask", 0) & ~EU_CREATE # a client never creates
	w.put_varint(int(item.get("id", 0)))
	w.put_u8(mask)
	if mask & EU_FLAGS:
		w.put_u8(int(item.get("flags", 0)))
	if mask & EU_TRAJECTORY:
		_write_trajectory(w, item.get("trajectory", {}))
	if mask & EU_PLAYER_INPUT:
		_write_input(w, item.get("input", {}))
	if mask & EU_TOOL:
		w.put_u8(int(item.get("tool", TOOL_MELEE)))
	if mask & EU_BLOCK_COLOR:
		var c: Color = item.get("block_color", Color(7.0 / 15.0, 7.0 / 15.0, 7.0 / 15.0))
		w.put_color(int(c.r8), int(c.g8), int(c.b8))
	if mask & EU_HEALTH:
		w.put_u8(int(item.get("health", 100)))


static func _write_input(w: P10Buf, input: Dictionary) -> void:
	var flags: int = int(input.get("flags", 0)) & 0x1F
	var stance: int = int(input.get("stance", STANCE_STAND)) & 3
	w.put_u8(flags | (stance << 6))
	w.put_u8(int(input.get("xmove", 0)) & 0xFF)
	w.put_u8(int(input.get("ymove", 0)) & 0xFF)


static func _write_weapon(w: P10Buf, p: Dictionary) -> void:
	var flags := 0
	if p.get("full_auto", false):
		flags |= 1
	if p.get("slow_reload", false):
		flags |= 2
	if p.get("penetrative", false):
		flags |= 4
	flags |= (int(p.get("type", 0)) & 3) << 3
	w.put_u8(flags)
	w.put_u16(int(p.get("fire_interval_ms", 500)))
	w.put_u16(int(p.get("reload_ms", 2500)))
	w.put_u16(int(p.get("raise_ms", 500)))
	w.put_u8(int(p.get("max_damage", 49)))
	w.put_u8(int(p.get("magazine", 10)))


static func _write_trajectory(w: P10Buf, t: Dictionary) -> void:
	var type: int = int(t.get("type", TRAJ_CONSTANT))
	var parent: int = int(t.get("parent", -1))
	var has_parent := parent >= 0
	w.put_u8((type & 0x7F) | (0x80 if has_parent else 0))
	var origin: Vector3 = t.get("origin", Vector3.ZERO)
	var velocity: Vector3 = t.get("velocity", Vector3.ZERO)
	w.put_vec3(origin)
	w.put_vec3(velocity)
	if type == TRAJ_PLAYER:
		w.put_vec3(t.get("euler", Vector3.ZERO))
	else:
		w.put_vec3(t.get("rotation", Vector3.ZERO))
		w.put_vec3(t.get("angular", Vector3.ZERO))
	if has_parent:
		w.put_varint(parent)


static func hit_entity(target: int, hit_type: int, damage_type: int, fire: Vector3,
		hit: Vector3) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(HIT_ENTITY)
	w.put_varint(target)
	w.put_u8(hit_type)
	w.put_u8(damage_type)
	w.put_vec3(fire)
	w.put_vec3(hit)
	return w.data


static func hit_terrain(cell: Vector3i, damage_type: int, fire: Vector3,
		hit: Vector3) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(HIT_TERRAIN)
	w.put_svarint(cell.x)
	w.put_svarint(cell.y)
	w.put_svarint(cell.z)
	w.put_u8(damage_type)
	w.put_vec3(fire)
	w.put_vec3(hit)
	return w.data


static func build_block(cell: Vector3i, color: Color) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(BUILD_BLOCK)
	w.put_svarint(cell.x)
	w.put_svarint(cell.y)
	w.put_svarint(cell.z)
	w.put_color(int(color.r8), int(color.g8), int(color.b8))
	return w.data


static func build_line(from: Vector3i, to: Vector3i, color: Color) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(BUILD_LINE)
	w.put_svarint(from.x)
	w.put_svarint(from.y)
	w.put_svarint(from.z)
	w.put_svarint(to.x)
	w.put_svarint(to.y)
	w.put_svarint(to.z)
	w.put_color(int(color.r8), int(color.g8), int(color.b8))
	return w.data


static func throw_grenade(origin: Vector3, velocity: Vector3, fuse: float) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(THROW_GRENADE)
	w.put_vec3(origin)
	w.put_vec3(velocity)
	w.put_f32(fuse)
	return w.data


static func chat(channel: int, text: String) -> PackedByteArray:
	var w := P10Buf.new()
	var bytes := text.to_utf8_buffer()
	if bytes.size() > 255:
		bytes = bytes.slice(0, 255)
	w.put_u8(CHAT_MESSAGE)
	w.put_u8(channel)
	w.put_bytes(bytes)
	return w.data


static func player_action(event_type: int, param := 0) -> PackedByteArray:
	var w := P10Buf.new()
	w.put_u8(PLAYER_ACTION)
	w.put_u8(event_type)
	w.put_varint(param)
	return w.data


# ---------------------------------------------------------------- decoders

static func read_greeting(bytes: PackedByteArray) -> PackedByteArray:
	var r := P10Buf.new(bytes)
	if r.get_u8() != GREETING:
		return PackedByteArray()
	if r.get_string() != "Hello":
		return PackedByteArray()
	var nonce := r.get_bytes()
	if r.error:
		return PackedByteArray()
	return nonce


static func read_map(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	r.get_u8() # the packet id
	var out := {}
	while not r.error:
		var key := r.get_string()
		if key.is_empty():
			break
		var value := r.get_string()
		if r.error:
			break
		out[key] = value
	return out


static func read_kick(bytes: PackedByteArray) -> String:
	var r := P10Buf.new(bytes)
	if r.get_u8() != KICK:
		return ""
	return r.get_string()


static func read_chat_broadcast(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != CHAT_BROADCAST:
		return {}
	var from := r.get_varint()
	var channel := r.get_u8()
	var text := r.get_string()
	if r.error or channel > 2:
		return {}
	return {"from": from, "channel": channel, "text": text}


static func read_entity_event(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != ENTITY_EVENT:
		return {}
	var entity := r.get_varint()
	var event_type := r.get_u8()
	var param := r.get_varint()
	if r.error:
		return {}
	return {"entity": entity, "event": event_type, "param": param}


static func read_entity_die(bytes: PackedByteArray) -> Dictionary:
	var r := P10Buf.new(bytes)
	if r.get_u8() != ENTITY_DIE:
		return {}
	var entity := r.get_varint()
	var death_type := r.get_u8()
	var param := r.get_varint()
	var out := {"entity": entity, "death_type": death_type, "param": param,
		"reason": param & 0xFF, "respawn": param >> 8, "has_damage": false}
	if r.has_more():
		var dt := r.get_u8()
		out["has_damage"] = true
		out["damage_type"] = dt & 0x7F
		out["fire"] = r.get_vec3()
		out["hit"] = r.get_vec3()
		out["from"] = r.get_varint() if (dt & 0x80) else 0
	if r.error:
		return {}
	return out


static func read_player_item(r: P10Buf) -> Dictionary:
	var out := {"mask": 0}
	out["id"] = r.get_varint()
	var mask := r.get_u8()
	out["mask"] = mask
	if mask & ~PU_KNOWN:
		r.error = true
		return {}
	if mask & PU_CREATE:
		out["name"] = r.get_string()
	if mask & PU_SCORE:
		out["score"] = r.get_varint()
	if mask & PU_FLAGS:
		out["flags"] = r.get_u8()
	if mask & PU_TEAM:
		out["team"] = r.get_u8()
	if mask & PU_STATS:
		out["kills"] = r.get_varint()
		out["deaths"] = r.get_varint()
	if mask & PU_PING:
		out["ping"] = r.get_varint()
	if mask & PU_PERKS:
		out["perk_points"] = r.get_u8()
		out["perk_inventory"] = r.get_u8()
		out["perk_next"] = r.get_u8()
	if r.error:
		return {}
	return out


static func _i16(v: int) -> int:
	return v - 65536 if v >= 32768 else v


static func _i8(v: int) -> int:
	return v - 256 if v >= 128 else v


static func read_trajectory(r: P10Buf) -> Dictionary:
	var b := r.get_u8()
	var type := b & 0x7F
	var has_parent := (b & 0x80) != 0
	if type >= TRAJ_PLAYER_COMPACT + 1:
		r.error = true
		return {}
	if type == TRAJ_PLAYER_COMPACT:
		var origin := Vector3(r.get_u16() / 64.0, r.get_u16() / 64.0, _i16(r.get_u16()) / 64.0)
		var velocity := Vector3(_i16(r.get_u16()) / 1024.0,
			_i16(r.get_u16()) / 1024.0, _i16(r.get_u16()) / 1024.0)
		var pitch := _i16(r.get_u16()) / (32767.0 / (PI / 2.0))
		var yaw := _i16(r.get_u16()) / (32767.0 / PI)
		var t := {"type": TRAJ_PLAYER, "origin": origin, "velocity": velocity,
			"euler": Vector3(pitch, 0.0, yaw), "parent": r.get_varint() if has_parent else -1}
		if r.error:
			return {}
		return t
	var origin := r.get_vec3()
	var velocity := r.get_vec3()
	var t := {"type": type, "origin": origin, "velocity": velocity}
	if type == TRAJ_PLAYER:
		t["euler"] = r.get_vec3()
	else:
		t["rotation"] = r.get_vec3()
		t["angular"] = r.get_vec3()
	t["parent"] = r.get_varint() if has_parent else -1
	if r.error:
		return {}
	return t


static func read_entity_item(r: P10Buf) -> Dictionary:
	var out := {}
	out["id"] = r.get_varint()
	var mask := r.get_u8()
	if mask & ~EU_KNOWN:
		r.error = true
		return {}
	out["mask"] = mask
	if mask & EU_CREATE:
		var type := r.get_u8()
		if type >= ENT_COUNT:
			r.error = true
			return {}
		out["type"] = type
		if type == ENT_PLAYER:
			out["body_skin"] = r.get_bytes().get_string_from_utf8()
			var skins := []
			for i in 3:
				skins.append(r.get_bytes().get_string_from_utf8())
			out["weapon_skin"] = skins
			var weapons := []
			for i in 3:
				weapons.append(read_weapon(r))
			out["weapon"] = weapons
		if type == ENT_COMMAND_POST or type == ENT_FLAG or type == ENT_CHECKPOINT or type >= ENT_SENTRY:
			out["owner"] = r.get_varint()
			out["team"] = r.get_u8()
	if mask & EU_FLAGS:
		out["flags"] = r.get_u8()
	if mask & EU_TRAJECTORY:
		out["trajectory"] = read_trajectory(r)
	if mask & EU_PLAYER_INPUT:
		var b := r.get_u8()
		out["input"] = {
			"flags": b & 0x1F,
			"stance": (b >> 6) & 3,
			"xmove": _i8(r.get_u8()),
			"ymove": _i8(r.get_u8()),
		}
		if (b & 0x20) or ((b >> 6) & 3) > STANCE_PRONE:
			r.error = true
	if mask & EU_TOOL:
		out["tool"] = r.get_u8()
	if mask & EU_BLOCK_COLOR:
		out["block_color"] = r.get_color()
	if mask & EU_HEALTH:
		out["health"] = r.get_u8()
	if r.error:
		return {}
	return out


static func read_weapon(r: P10Buf) -> Dictionary:
	var f := r.get_u8()
	var out := {
		"full_auto": (f & 1) != 0,
		"slow_reload": (f & 2) != 0,
		"penetrative": (f & 4) != 0,
		"type": (f >> 3) & 3,
	}
	out["fire_interval_ms"] = r.get_u16()
	out["reload_ms"] = r.get_u16()
	out["raise_ms"] = r.get_u16()
	out["max_damage"] = r.get_u8()
	out["magazine"] = r.get_u8()
	if f & 0xE0:
		r.error = true
	return out


## TerrainUpdate: an Array of {cell: Vector3i, health: int, color: Color, type: int}.
## health 0 means destroyed (color is black).
static func read_terrain(bytes: PackedByteArray) -> Array:
	var r := P10Buf.new(bytes)
	if r.get_u8() != TERRAIN_UPDATE:
		return []
	var out := []
	var cursor := Vector3i.ZERO
	while r.has_more():
		var b := r.get_u8()
		var dx := b if b < 128 else b - 256
		if b == 0x80:
			cursor = Vector3i(r.get_varint(), r.get_varint(), r.get_varint())
			var nb := r.get_u8()
			dx = nb if nb < 128 else nb - 256
		var db := r.get_u8()
		var dy := db if db < 128 else db - 256
		var zb := r.get_u8()
		var dz := zb if zb < 128 else zb - 256
		var cell := Vector3i(cursor.x + dx, cursor.y + dy, cursor.z + dz)
		var health := r.get_u8()
		if health != 0:
			var red := r.get_u8()
			var green := r.get_u8()
			var blue := r.get_u8() # R, G, B here
			var ctype := r.get_u8()
			out.append({"cell": cell, "health": health, "color": Color8(red, green, blue), "type": ctype})
		else:
			var dtype := r.get_u8()
			out.append({"cell": cell, "health": 0, "color": Color.BLACK, "type": dtype})
		if r.error:
			return []
	return out


# ---------------------------------------------------------------- helpers

static func euler_to_front(euler: Vector3) -> Vector3:
	var cp := cos(euler.x)
	return Vector3(-cp * sin(euler.z), cp * cos(euler.z), sin(euler.x))
