/*
 * Protocol 1.0 primitive codec and packet builders.
 *
 * Plain C99, no dependencies, no allocation: every reader works on a packet
 * you already received, every writer on a buffer you own. This is the
 * reference for sections 3 to 8 of 1.0.md.
 *
 * MIT licence (see ../README.md).
 */
#ifndef PROTO10_H
#define PROTO10_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- limits ---------------------------------------------------------- */

#define P10_PACKET_MAX 16384 /* a WebRTC message; a packet is never this big */
#define P10_BYTES_MAX (1024 * 1024)
#define P10_CHAT_MAX 255
#define P10_NAME_MAX 63
#define P10_SKIN_MAX 32

/* ---- packet ids ------------------------------------------------------ */

enum {
    P10_GREETING = 1,
    P10_INITIATE_CONNECTION = 2,
    P10_SERVER_CERTIFICATE = 3,
    P10_CLIENT_CERTIFICATE = 4,
    P10_KICK = 5,
    P10_GAME_STATE_HEADER = 6,
    P10_MAP_DATA = 7,
    P10_GAME_STATE_FINAL = 8,
    P10_GENERIC_COMMAND = 9,
    P10_MAP_DATA_ACKNOWLEDGE = 10,
    P10_MAP_DATA_FINAL = 11,
    P10_ENTITY_UPDATE = 30,
    P10_ENTITY_EVENT = 31,
    P10_ENTITY_DIE = 32,
    P10_ENTITY_REMOVE = 33,
    P10_CLIENT_SIDE_ENTITY_UPDATE = 35,
    P10_TERRAIN_UPDATE = 40,
    P10_PLAYER_UPDATE = 50,
    P10_PLAYER_REMOVE = 51,
    P10_PLAYER_ACTION = 60,
    P10_CHAT_MESSAGE = 70,
    P10_CHAT_BROADCAST = 71,
    P10_GAME_MODE_STATE = 80,
    P10_HIT_ENTITY = 90,
    P10_HIT_TERRAIN = 91,
    P10_DAMAGE = 92,
    P10_BUILD_BLOCK = 93,
    P10_BUILD_LINE = 94,
    P10_THROW_GRENADE = 95
};

/* ---- enums ----------------------------------------------------------- */

enum p10_entity_type {
    P10_ENT_PLAYER,
    P10_ENT_GRENADE,
    P10_ENT_ROCKET,
    P10_ENT_COMMAND_POST,
    P10_ENT_FLAG,
    P10_ENT_CHECKPOINT,
    P10_ENT_VEHICLE,
    P10_ENT_SENTRY,
    P10_ENT_LANDMINE,
    P10_ENT_DYNAMITE,
    P10_ENT_DRILL,
    P10_ENT_AMMO_CRATE,
    P10_ENT_HEALTH_CRATE,
    P10_ENT_COUNT
};

enum p10_tool { P10_TOOL_MELEE, P10_TOOL_BLOCK, P10_TOOL_WEAPON1, P10_TOOL_WEAPON2, P10_TOOL_WEAPON3 };
enum p10_stance { P10_STANCE_STAND, P10_STANCE_CROUCH, P10_STANCE_PRONE };
enum p10_hit { P10_HIT_UNSPECIFIED, P10_HIT_TORSO, P10_HIT_HEAD, P10_HIT_LIMB };

enum p10_damage {
    P10_DMG_UNKNOWN,
    P10_DMG_SUICIDE,
    P10_DMG_FALL,
    P10_DMG_POISON,
    P10_DMG_MELEE,
    P10_DMG_DIG,
    P10_DMG_BLOCK,
    P10_DMG_WEAPON1,
    P10_DMG_WEAPON2,
    P10_DMG_WEAPON3,
    P10_DMG_GRENADE,
    P10_DMG_VEHICLE_EXPLOSION,
    P10_DMG_VEHICLE,
    P10_DMG_SENTRY
};

enum p10_trajectory_type {
    P10_TRAJ_CONSTANT,
    P10_TRAJ_LINEAR,
    P10_TRAJ_GRAVITY,
    P10_TRAJ_PLAYER,
    P10_TRAJ_RIGID_BODY,
    P10_TRAJ_PLAYER_COMPACT,
    P10_TRAJ_COUNT
};

enum {
    P10_TEAM_1 = 0,
    P10_TEAM_2 = 1,
    P10_TEAM_SPECTATOR = 255
};

#define P10_NO_OWNER 0xFFFFFFFFu

/* entity update mask */
enum {
    P10_EU_CREATE = 1 << 0,
    P10_EU_FLAGS = 1 << 1,
    P10_EU_TRAJECTORY = 1 << 2,
    P10_EU_PLAYER_INPUT = 1 << 3,
    P10_EU_TOOL = 1 << 4,
    P10_EU_BLOCK_COLOR = 1 << 5,
    P10_EU_HEALTH = 1 << 6
};

/* entity flags */
enum {
    P10_EF_PLAYER_CLIP = 1 << 0,
    P10_EF_WEAPON_CLIP = 1 << 1,
    P10_EF_FLY = 1 << 2,
    P10_EF_JETPACK = 1 << 3
};

/* input flags */
enum {
    P10_IN_TOOL_PRIMARY = 1 << 0,
    P10_IN_TOOL_SECONDARY = 1 << 1,
    P10_IN_CHAT = 1 << 2,
    P10_IN_SPRINT = 1 << 3,
    P10_IN_JETPACK = 1 << 4
};

/* player update mask (note: Score is bit 1 and Flags bit 2) */
enum {
    P10_PU_CREATE = 1 << 0,
    P10_PU_SCORE = 1 << 1,
    P10_PU_FLAGS = 1 << 2,
    P10_PU_TEAM = 1 << 3,
    P10_PU_STATS = 1 << 4,
    P10_PU_PING = 1 << 5,
    P10_PU_PERKS = 1 << 6
};

/* ---- primitives ------------------------------------------------------ */

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
    int error; /* sticky: once set, every read returns 0 */
} p10_reader;

typedef struct {
    uint8_t *data;
    size_t cap;
    size_t size;
    int error; /* sticky: set when cap is exceeded */
} p10_writer;

typedef struct { float x, y, z; } p10_vec3;
typedef struct { uint8_t b, g, r; } p10_color;

void p10_reader_init(p10_reader *r, const void *data, size_t size);
void p10_writer_init(p10_writer *w, void *buf, size_t cap);
int p10_reader_ok(const p10_reader *r);
int p10_at_end(const p10_reader *r);
int p10_writer_ok(const p10_writer *w);

int p10_has_more(const p10_reader *r);

uint8_t p10_read_u8(p10_reader *r);
uint16_t p10_read_u16(p10_reader *r);
uint32_t p10_read_u32(p10_reader *r);
uint32_t p10_read_varint32(p10_reader *r);
uint64_t p10_read_varint(p10_reader *r);
int64_t p10_read_svarint(p10_reader *r);
float p10_read_f32(p10_reader *r);
p10_vec3 p10_read_vec3(p10_reader *r);
p10_color p10_read_color(p10_reader *r);
/* returns a pointer into the packet; *len is set. NULL on a decode error. */
const uint8_t *p10_read_bytes(p10_reader *r, size_t *len);
/* copies into out (NUL terminated); rejects NUL inside and overlong text */
void p10_read_string(p10_reader *r, char *out, size_t out_size);

void p10_write_raw(p10_writer *w, const void *data, size_t n);
void p10_write_u8(p10_writer *w, uint8_t v);
void p10_write_u16(p10_writer *w, uint16_t v);
void p10_write_u32(p10_writer *w, uint32_t v);
void p10_write_varint(p10_writer *w, uint64_t v);
void p10_write_svarint(p10_writer *w, int64_t v);
void p10_write_f32(p10_writer *w, float f);
void p10_write_vec3(p10_writer *w, p10_vec3 v);
void p10_write_color(p10_writer *w, p10_color c);
void p10_write_bytes(p10_writer *w, const void *data, size_t n);
void p10_write_string(p10_writer *w, const char *s);

/* ---- weapons and trajectories --------------------------------------- */

typedef struct {
    uint8_t full_auto, slow_reload, penetrative, type;
    uint16_t fire_interval_ms, reload_ms, raise_ms;
    uint8_t max_damage, magazine;
} p10_weapon_params;

typedef struct {
    uint8_t type;    /* P10_TRAJ_*, never COMPACT after decoding */
    uint8_t has_parent;
    uint32_t parent;
    p10_vec3 origin, velocity;
    p10_vec3 rotation;         /* compact quaternion (x,y,z), w >= 0 */
    p10_vec3 angular_velocity; /* non-player types */
    p10_vec3 euler;            /* player: pitch, roll, yaw */
} p10_trajectory;

typedef struct {
    uint8_t flags; /* P10_IN_* */
    uint8_t stance;
    int8_t xmove, ymove;
} p10_input;

void p10_write_weapon(p10_writer *w, const p10_weapon_params *p);
int p10_read_weapon(p10_reader *r, p10_weapon_params *p);
void p10_write_trajectory(p10_writer *w, const p10_trajectory *t);
int p10_read_trajectory(p10_reader *r, p10_trajectory *t);
void p10_write_input(p10_writer *w, const p10_input *in);
int p10_read_input(p10_reader *r, p10_input *in);

/* ---- entity and player items ---------------------------------------- */

typedef struct {
    uint32_t entity_id;
    uint8_t mask;
    uint8_t entity_type;
    uint32_t owner;
    uint8_t team;
    /* skins: pointers into the packet, not NUL terminated */
    const uint8_t *body_skin;
    size_t body_skin_len;
    const uint8_t *weapon_skin[3];
    size_t weapon_skin_len[3];
    p10_weapon_params weapon[3];
    uint8_t flags;
    p10_trajectory trajectory;
    p10_input input;
    uint8_t tool;
    p10_color block_color;
    uint8_t health;
} p10_entity_item;

/* the writer variant takes the same struct; skins are C strings */
void p10_write_entity_item(p10_writer *w, const p10_entity_item *it);
int p10_read_entity_item(p10_reader *r, p10_entity_item *it);

typedef struct {
    uint32_t player_id;
    uint8_t mask;
    char name[P10_NAME_MAX + 1];
    uint8_t flags; /* bit 0 isAdmin */
    uint32_t score;
    uint8_t team;
    uint32_t kills, deaths;
    uint32_t ping_ms;
    uint8_t perk_points, perk_inventory, perk_next;
} p10_player_item;

void p10_write_player_item(p10_writer *w, const p10_player_item *it);
int p10_read_player_item(p10_reader *r, p10_player_item *it);

/* ---- packet builders (client -> server) ------------------------------ */

void p10_build_initiate(p10_writer *w, const char *package, const char *environment, const char *locale,
                        const char *player_name, const uint8_t nonce[256], uint8_t map_quality);
void p10_build_client_certificate(p10_writer *w, int valid, const void *cert, size_t cert_len,
                                  const void *sig, size_t sig_len);
void p10_build_generic(p10_writer *w, int argc, const char *const *argv);
void p10_build_client_entity(p10_writer *w, p10_entity_item *items, size_t count);
void p10_build_hit_entity(p10_writer *w, uint32_t target, uint8_t hit_type, uint8_t damage_type,
                          p10_vec3 fire, p10_vec3 hit);
void p10_build_hit_terrain(p10_writer *w, int32_t x, int32_t y, int32_t z, uint8_t damage_type,
                           p10_vec3 fire, p10_vec3 hit);
void p10_build_build_block(p10_writer *w, int32_t x, int32_t y, int32_t z, p10_color color);
void p10_build_build_line(p10_writer *w, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                          int32_t z1, p10_color color);
void p10_build_throw_grenade(p10_writer *w, p10_vec3 origin, p10_vec3 velocity, float fuse);
void p10_build_chat(p10_writer *w, uint8_t channel, const char *utf8);
void p10_build_player_action(p10_writer *w, uint8_t event_type, uint64_t param);

/* ---- decoders (server -> client) ------------------------------------- */

int p10_decode_greeting(p10_reader *r, uint8_t nonce[128]);
/* GameStateHeader / GameModeState / GameStateFinal's property map */
typedef void (*p10_map_fn)(void *ctx, const char *key, const char *value);
int p10_decode_map(p10_reader *r, p10_map_fn fn, void *ctx);
int p10_decode_kick(p10_reader *r, char *reason, size_t reason_size);
int p10_decode_chat_broadcast(p10_reader *r, uint32_t *from, uint8_t *channel, char *text, size_t text_size);
int p10_decode_entity_die(p10_reader *r, uint32_t *entity, uint8_t *death_type, uint64_t *param,
                          int *has_damage, uint8_t *damage_type, uint32_t *from_entity, p10_vec3 *fire,
                          p10_vec3 *hit);
int p10_decode_entity_event(p10_reader *r, uint32_t *entity, uint8_t *event_type, uint64_t *param);

/* TerrainUpdate: called once per edit, in order. Return 0 to stop. */
typedef int (*p10_edit_fn)(void *ctx, int32_t x, int32_t y, int32_t z, uint8_t health, uint8_t r, uint8_t g,
                           uint8_t b, uint8_t type);
int p10_decode_terrain(p10_reader *r, p10_edit_fn fn, void *ctx);

/* ---- helpers --------------------------------------------------------- */

/* a 0.75/1.0 orientation vector from Euler angles (pitch positive = down) */
p10_vec3 p10_euler_to_front(p10_vec3 euler);
/* CRC32 (zlib's polynomial), for the map cache key */
uint32_t p10_crc32(uint32_t crc, const void *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* PROTO10_H */
