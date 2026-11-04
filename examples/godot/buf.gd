class_name P10Buf
extends RefCounted
## Byte reader/writer for protocol 1.0 (see 1.0.md section 3).
##
## The reader keeps a sticky `error`, like the C codec: check it once after
## decoding a whole packet, or use `at_end()` for packets whose length is
## implied by the end of the packet.
##
## GDScript ints are 64-bit; that is enough for every varint the protocol
## uses (a hostile 10-byte varint above 2^63 is clamped by the shift).

var data := PackedByteArray()
var pos := 0
var error := false


func _init(bytes := PackedByteArray()) -> void:
	data = bytes


# ---------------------------------------------------------------- writer

func put_u8(v: int) -> void:
	data.append(v & 0xFF)


func put_u16(v: int) -> void:
	put_u8(v)
	put_u8(v >> 8)


func put_u32(v: int) -> void:
	put_u8(v)
	put_u8(v >> 8)
	put_u8(v >> 16)
	put_u8(v >> 24)


func put_f32(v: float) -> void:
	var at := data.size()
	data.resize(at + 4)
	data.encode_float(at, v)


func put_varint(v: int) -> void:
	if v < 0:
		error = true
		return
	while true:
		var b := v & 0x7F
		v >>= 7
		if v != 0:
			put_u8(b | 0x80)
		else:
			put_u8(b)
			return


func put_svarint(v: int) -> void:
	if v < 0:
		put_varint(((-1 - v) << 1) | 1)
	else:
		put_varint(v << 1)


func put_raw(bytes: PackedByteArray) -> void:
	data.append_array(bytes)


func put_bytes(bytes: PackedByteArray) -> void:
	put_varint(bytes.size())
	put_raw(bytes)


func put_string(s: String) -> void:
	put_bytes(s.to_utf8_buffer())


func put_vec3(v: Vector3) -> void:
	put_f32(v.x)
	put_f32(v.y)
	put_f32(v.z)


## The `color` primitive is B, G, R (TerrainUpdate is the exception).
func put_color(r: int, g: int, b: int) -> void:
	put_u8(b)
	put_u8(g)
	put_u8(r)


# ---------------------------------------------------------------- reader

func _need(n: int) -> bool:
	if error or data.size() - pos < n:
		error = true
		return false
	return true


func has_more() -> bool:
	return not error and pos < data.size()


func at_end() -> bool:
	return not error and pos == data.size()


func get_u8() -> int:
	if not _need(1):
		return 0
	var v := data[pos]
	pos += 1
	return v


func get_u16() -> int:
	if not _need(2):
		return 0
	var v := data[pos] | (data[pos + 1] << 8)
	pos += 2
	return v


func get_u32() -> int:
	if not _need(4):
		return 0
	var v := data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24)
	pos += 4
	return v


func get_f32() -> float:
	if not _need(4):
		return 0.0
	var v := data.decode_float(pos)
	pos += 4
	return v


func get_varint() -> int:
	var v := 0
	var shift := 0
	while shift < 70:
		var b := get_u8()
		if error:
			return 0
		v |= (b & 0x7F) << shift
		if (b & 0x80) == 0:
			return v
		shift += 7
	error = true
	return 0


func get_svarint() -> int:
	var u := get_varint()
	if u & 1:
		return -1 - (u >> 1)
	return u >> 1


func get_bytes() -> PackedByteArray:
	var n := get_varint()
	if n < 0 or n > 1048576 or not _need(n):
		error = true
		return PackedByteArray()
	var out := data.slice(pos, pos + n)
	pos += n
	return out


func get_string() -> String:
	var b := get_bytes()
	if error:
		return ""
	if b.find(0) != -1:
		error = true
		return ""
	return b.get_string_from_utf8()


func get_vec3() -> Vector3:
	var x := get_f32()
	var y := get_f32()
	var z := get_f32()
	return Vector3(x, y, z)


## raw n bytes, not length-prefixed (0.75 structs, fixed-size fields)
func get_raw(n: int) -> PackedByteArray:
	if not _need(n):
		return PackedByteArray()
	var out := data.slice(pos, pos + n)
	pos += n
	return out


func get_color() -> Color:
	var b := get_u8()
	var g := get_u8()
	var r := get_u8()
	return Color8(r, g, b)
