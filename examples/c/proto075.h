/*
 * Ace of Spades 0.75 / 0.76: packed packet layouts and small helpers.
 *
 * The structs below are exactly the wire bytes (pragma pack, little-endian).
 * For variable-length packets (names, chat) use the builders at the bottom;
 * the fixed structs are safe to read over a received buffer once you checked
 * the length.
 *
 * MIT licence.
 */
#ifndef PROTO075_H
#define PROTO075_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- connect and addressing ----------------------------------------- */

#define P075_CONNECT 3
#define P075_CONNECT_076 4
#define P075_PORT_DEFAULT 32887

/* parse aos://<word>[:port], aos://a.b.c.d:port or host:port.
   out[0..3] is the IPv4 address in network order, *port is set.
   Returns 1 on success. */
int p075_parse_address(const char *text, uint8_t out[4], uint16_t *port);
/* the same, for an address the master gave as aos://<word>:port */
int p075_word_to_ipv4(uint32_t word, uint8_t out[4]);

/* ---- ids ------------------------------------------------------------- */

enum {
    P075_POSITION = 0,
    P075_ORIENTATION = 1,
    P075_WORLDUPDATE = 2,
    P075_INPUT = 3,
    P075_WEAPONINPUT = 4,
    P075_SETHP = 5,
    P075_HIT = 5, /* C->S; the direction decides which */
    P075_GRENADE = 6,
    P075_SETTOOL = 7,
    P075_SETCOLOR = 8,
    P075_EXISTINGPLAYER = 9,
    P075_SHORTPLAYER = 10,
    P075_MOVEOBJECT = 11,
    P075_CREATEPLAYER = 12,
    P075_BLOCKACTION = 13,
    P075_BLOCKLINE = 14,
    P075_STATEDATA = 15,
    P075_KILLACTION = 16,
    P075_CHATMESSAGE = 17,
    P075_MAPSTART = 18,
    P075_MAPCHUNK = 19,
    P075_PLAYERLEFT = 20,
    P075_TERRITORYCAPTURE = 21,
    P075_PROGRESSBAR = 22,
    P075_INTELCAPTURE = 23,
    P075_INTELPICKUP = 24,
    P075_INTELDROP = 25,
    P075_RESTOCK = 26,
    P075_FOGCOLOR = 27,
    P075_WEAPONRELOAD = 28,
    P075_CHANGETEAM = 29,
    P075_CHANGEWEAPON = 30,
    P075_MAPCACHED = 31, /* C->S, 0.76 */
    P075_HANDSHAKEINIT = 31,
    P075_HANDSHAKERETURN = 32,
    P075_VERSIONGET = 33,
    P075_VERSIONSEND = 34,
    P075_EXTINFO = 60,
    P075_PLAYERPROPERTIES = 64,
    P075_FLASHLIGHT = 114
};

enum { P075_TEAM_1 = 0, P075_TEAM_2 = 1, P075_TEAM_SPECTATOR = -1 };
enum { P075_RIFLE = 0, P075_SMG = 1, P075_SHOTGUN = 2 };
enum { P075_TOOL_SPADE = 0, P075_TOOL_BLOCK = 1, P075_TOOL_GUN = 2, P075_TOOL_GRENADE = 3 };

enum {
    P075_ACTION_BUILD = 0,
    P075_ACTION_DESTROY = 1,
    P075_ACTION_SPADE = 2,
    P075_ACTION_GRENADE = 3,
    P075_ACTION_GUN = 4,
    P075_ACTION_SPADE_ONE = 5
};

enum { P075_HIT_TORSO = 0, P075_HIT_HEAD = 1, P075_HIT_ARMS = 2, P075_HIT_LEGS = 3, P075_HIT_MELEE = 4 };

enum { P075_CHAT_ALL = 0, P075_CHAT_TEAM = 1, P075_CHAT_SYSTEM = 2, P075_CHAT_BIG = 3 };

/* ---- packed wire types ---------------------------------------------- */

#pragma pack(push, 1)
typedef struct { float x, y, z; } p075_vec3;
typedef struct { int32_t x, y, z; } p075_vec3i;
typedef struct { uint8_t b, g, r; } p075_color;

typedef struct { p075_vec3 pos; } p075_position;                  /* 13 with id */
typedef struct { p075_vec3 ori; } p075_orientation;               /* 13 */
typedef struct { p075_vec3 pos, ori; } p075_world_entry;          /* 24 */
typedef struct { uint8_t id, player_id, keys; } p075_input;       /* 3 */
typedef struct { uint8_t id, player_id, buttons; } p075_weaponinput; /* bit0 primary, bit1 secondary */
typedef struct { uint8_t id, hp, type; p075_vec3 source; } p075_sethp;
typedef struct { uint8_t id, player_id; float fuse; p075_vec3 pos, vel; } p075_grenade;
typedef struct { uint8_t id, player_id, tool; } p075_settool;
typedef struct { uint8_t id, player_id; p075_color color; } p075_setcolor;
typedef struct { uint8_t id, player_id, team, weapon, tool; uint32_t kills; p075_color color; } p075_existing;
typedef struct { uint8_t id, player_id, team, weapon; } p075_shortplayer;
typedef struct { uint8_t id, object, team; p075_vec3 pos; } p075_moveobject;
typedef struct { uint8_t id, player_id, weapon, team; p075_vec3 pos; } p075_createplayer;
typedef struct { uint8_t id, player_id, type; p075_vec3i pos; } p075_blockaction;
typedef struct { uint8_t id, player_id; p075_vec3i start, end; } p075_blockline;
typedef struct { uint8_t id, victim, killer, kind, respawn; } p075_killaction;
typedef struct { uint8_t id, player_id, chat_type; } p075_chat_head;
typedef struct { uint8_t id; uint32_t map_size; } p075_mapstart;
typedef struct { uint8_t id; uint32_t map_size, crc32; char map_name[64]; } p075_mapstart076;
typedef struct { uint8_t id, player_id; } p075_playerleft;
typedef struct { uint8_t id, index, winning, team; } p075_territorycapture;
typedef struct { uint8_t id, index, team; int8_t rate; float progress; } p075_progressbar;
typedef struct { uint8_t id, player_id, winning; } p075_intelcapture;
typedef struct { uint8_t id, player_id; } p075_intelpickup;
typedef struct { uint8_t id, player_id; p075_vec3 pos; } p075_inteldrop;
typedef struct { uint8_t id, player_id; } p075_restock;
typedef struct { uint8_t id, alpha; p075_color color; } p075_fogcolor;
typedef struct { uint8_t id, player_id, ammo, reserved; } p075_weaponreload;
typedef struct { uint8_t id, player_id, team; } p075_changeteam;
typedef struct { uint8_t id, player_id, weapon; } p075_changeweapon;
typedef struct { uint8_t id, cached; } p075_mapcached;
typedef struct { uint8_t id; int32_t challenge; } p075_handshake;
typedef struct { uint8_t id, client, major, minor, revision; char os[64]; } p075_versionsend;
typedef struct { uint8_t id, player_id, on; } p075_light; /* sub 0 is the request */

/* StateData: 32 fixed bytes + a 52 byte CTF part or 1 + 13n TC part */
typedef struct {
    uint8_t player_id;
    p075_color fog, team1_color, team2_color;
    char team1_name[10], team2_name[10]; /* NUL padded, may lack the NUL */
    uint8_t mode;                        /* 0 CTF, 1 TC */
    union {
        struct {
            uint8_t team1_score, team2_score, capture_limit;
            uint8_t intel_state; /* bit 0: team 1's intel held, bit 1: team 2's */
            /* 12 bytes each: held = { u8 carrier; u8 pad[11]; } else vec3 pos */
            uint8_t intel1[12], intel2[12];
            p075_vec3 team1_base, team2_base;
        } ctf;
        struct {
            uint8_t count;
            struct { p075_vec3 pos; uint8_t team; } territories[16];
        } tc;
    } state;
} p075_statedata;
#pragma pack(pop)

/* Player Properties (extension 0x00, packet 64) */
#pragma pack(push, 1)
typedef struct {
    uint8_t id, sub, player_id;
    uint8_t health, blocks, grenades, clip, reserve;
    uint32_t score;
} p075_playerproperties;
#pragma pack(pop)

#define P075_STATEDATA_BASE 31u
#define P075_STATEDATA_CTF (P075_STATEDATA_BASE + 52u)
#define P075_STATEDATA_TC(n) (P075_STATEDATA_BASE + 1u + 13u * (n))

/* ---- coordinates ---------------------------------------------------- */

/* wire (map) -> engine Y-up: (x, 63 - z, y) */
p075_vec3 p075_pos_to_engine(p075_vec3 wire);
p075_vec3 p075_pos_from_engine(p075_vec3 engine);
/* wire direction (z down) -> engine Y-up: (x, -z, y) */
p075_vec3 p075_dir_to_engine(p075_vec3 wire);
p075_vec3 p075_dir_from_engine(p075_vec3 engine);

/* ---- text ----------------------------------------------------------- */

/* CP437 bytes -> UTF-8; out must hold 3 * in_len + 1 bytes.
   Returns the number of bytes written (without the NUL). */
size_t p075_cp437_to_utf8(const uint8_t *in, size_t in_len, char *out);
/* A 0.75 text: `0xFF` + valid UTF-8 is UTF-8, anything else CP437.
   Returns 1 if it was UTF-8. */
int p075_text_to_utf8(const uint8_t *in, size_t in_len, char *out, size_t out_size);

/* ---- builders (client -> server) ------------------------------------ */

/* All return the packet length, or 0 when the buffer is too small.
   Every builder writes the id byte first. */

size_t p075_build_position(uint8_t *buf, size_t cap, p075_vec3 pos);
size_t p075_build_orientation(uint8_t *buf, size_t cap, p075_vec3 ori);
size_t p075_build_input(uint8_t *buf, size_t cap, uint8_t player, uint8_t keys);
size_t p075_build_weapon_input(uint8_t *buf, size_t cap, uint8_t player, int primary, int secondary);
size_t p075_build_hit(uint8_t *buf, size_t cap, uint8_t victim, uint8_t hit_type);
size_t p075_build_grenade(uint8_t *buf, size_t cap, uint8_t player, float fuse, p075_vec3 pos, p075_vec3 vel);
size_t p075_build_set_tool(uint8_t *buf, size_t cap, uint8_t player, uint8_t tool);
size_t p075_build_set_color(uint8_t *buf, size_t cap, uint8_t player, p075_color color);
size_t p075_build_join(uint8_t *buf, size_t cap, uint8_t player, int8_t team, uint8_t weapon, uint8_t tool,
                       uint32_t kills, p075_color color, const char *name);
size_t p075_build_block_action(uint8_t *buf, size_t cap, uint8_t player, uint8_t type, int32_t x, int32_t y,
                               int32_t z);
size_t p075_build_block_line(uint8_t *buf, size_t cap, uint8_t player, p075_vec3i from, p075_vec3i to);
size_t p075_build_chat(uint8_t *buf, size_t cap, uint8_t player, uint8_t type, const char *text,
                       size_t text_len);
size_t p075_build_weapon_reload(uint8_t *buf, size_t cap, uint8_t player, uint8_t ammo, uint8_t reserve);
size_t p075_build_change_team(uint8_t *buf, size_t cap, uint8_t player, int8_t team);
size_t p075_build_change_weapon(uint8_t *buf, size_t cap, uint8_t player, uint8_t weapon);
size_t p075_build_map_cached(uint8_t *buf, size_t cap, int cached);
size_t p075_build_handshake_return(uint8_t *buf, size_t cap, int32_t challenge);
size_t p075_build_version_send(uint8_t *buf, size_t cap, char client, uint8_t major, uint8_t minor,
                               uint8_t revision, const char *os);
size_t p075_build_extinfo(uint8_t *buf, size_t cap, const uint8_t *pairs, size_t pair_count);
size_t p075_build_flashlight(uint8_t *buf, size_t cap, uint8_t player, int on);

#ifdef __cplusplus
}
#endif

#endif /* PROTO075_H */
