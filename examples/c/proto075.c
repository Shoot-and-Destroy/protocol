/*
 * Ace of Spades 0.75 / 0.76 helpers. See proto075.h and 0.75.md.
 *
 * MIT licence.
 */
#include "proto075.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- helpers --------------------------------------------------------- */

static int put(uint8_t *buf, size_t cap, size_t *at, const void *data, size_t n)
{
    if (*at > cap || cap - *at < n)
        return 0;
    memcpy(buf + *at, data, n);
    *at += n;
    return 1;
}

int p075_word_to_ipv4(uint32_t word, uint8_t out[4])
{
    /* the word is the address in little-endian byte order */
    out[0] = (uint8_t) (word & 0xFF);
    out[1] = (uint8_t) ((word >> 8) & 0xFF);
    out[2] = (uint8_t) ((word >> 16) & 0xFF);
    out[3] = (uint8_t) ((word >> 24) & 0xFF);
    return 1;
}

int p075_parse_address(const char *text, uint8_t out[4], uint16_t *port)
{
    char tmp[256];
    const char *p = text;
    size_t len;

    *port = P075_PORT_DEFAULT;
    if (strncmp(p, "aos://", 6) == 0)
        p += 6;
    if (p[0] == '[') /* IPv6: not a 0.75 address; the caller handles it */
        return 0;
    len = strlen(p);
    if (len == 0 || len >= sizeof tmp)
        return 0;
    memcpy(tmp, p, len + 1);

    /* a trailing :port, when what follows is numeric */
    char *colon = strrchr(tmp, ':');
    if (colon != NULL && colon[1] != '\0') {
        int numeric = 1;
        for (const char *q = colon + 1; *q; q++) {
            if (*q < '0' || *q > '9') {
                numeric = 0;
                break;
            }
        }
        if (numeric) {
            long v = strtol(colon + 1, NULL, 10);
            if (v <= 0 || v > 65535)
                return 0;
            *port = (uint16_t) v;
            *colon = '\0';
        }
    }

    /* dotted quad */
    unsigned a, b, c, d;
    if (sscanf(tmp, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        if (a > 255 || b > 255 || c > 255 || d > 255)
            return 0;
        out[0] = (uint8_t) a;
        out[1] = (uint8_t) b;
        out[2] = (uint8_t) c;
        out[3] = (uint8_t) d;
        return 1;
    }

    /* the classic word: all digits */
    for (const char *q = tmp; *q; q++) {
        if (*q < '0' || *q > '9')
            return 0; /* a hostname: resolve it with getaddrinfo yourself */
    }
    if (tmp[0] == '\0')
        return 0;
    return p075_word_to_ipv4((uint32_t) strtoul(tmp, NULL, 10), out);
}

p075_vec3 p075_pos_to_engine(p075_vec3 wire)
{
    p075_vec3 v;
    v.x = wire.x;
    v.y = 63.0f - wire.z;
    v.z = wire.y;
    return v;
}

p075_vec3 p075_pos_from_engine(p075_vec3 engine)
{
    p075_vec3 v;
    v.x = engine.x;
    v.y = engine.z;
    v.z = 63.0f - engine.y;
    return v;
}

p075_vec3 p075_dir_to_engine(p075_vec3 wire)
{
    p075_vec3 v;
    v.x = wire.x;
    v.y = -wire.z;
    v.z = wire.y;
    return v;
}

p075_vec3 p075_dir_from_engine(p075_vec3 engine)
{
    p075_vec3 v;
    v.x = engine.x;
    v.y = engine.z;
    v.z = -engine.y;
    return v;
}

/* ---- CP437 ----------------------------------------------------------- */

static const uint16_t cp437_high[128] = {
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
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

static size_t utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char) cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char) (0xC0 | (cp >> 6));
        out[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char) (0xE0 | (cp >> 12));
    out[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char) (0x80 | (cp & 0x3F));
    return 3;
}

size_t p075_cp437_to_utf8(const uint8_t *in, size_t in_len, char *out)
{
    size_t at = 0;
    for (size_t i = 0; i < in_len; i++) {
        uint8_t c = in[i];
        if (c == 0)
            break;
        if (c < 0x80)
            out[at++] = (char) c;
        else
            at += utf8_encode(cp437_high[c - 0x80], out + at);
    }
    out[at] = '\0';
    return at;
}

static int utf8_valid(const uint8_t *p, size_t n)
{
    size_t i = 0;
    while (i < n) {
        uint8_t c = p[i];
        size_t need;
        uint32_t cp;
        if (c < 0x80) {
            i++;
            continue;
        }
        if ((c & 0xE0) == 0xC0) {
            need = 1;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            need = 2;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            need = 3;
            cp = c & 0x07;
        } else {
            return 0;
        }
        if (i + need >= n)
            return 0;
        for (size_t k = 1; k <= need; k++) {
            if ((p[i + k] & 0xC0) != 0x80)
                return 0;
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        if ((need == 1 && cp < 0x80) || (need == 2 && cp < 0x800) || (need == 3 && cp < 0x10000))
            return 0;
        i += need + 1;
    }
    return 1;
}

int p075_text_to_utf8(const uint8_t *in, size_t in_len, char *out, size_t out_size)
{
    if (in_len >= 2 && in[0] == 0xFF && utf8_valid(in + 1, in_len - 1)) {
        size_t n = in_len - 1;
        if (n >= out_size)
            n = out_size - 1;
        memcpy(out, in + 1, n);
        out[n] = '\0';
        return 1;
    }
    if (utf8_valid(in, in_len)) { /* a server that sends UTF-8 without 0xFF */
        size_t n = in_len;
        if (n >= out_size)
            n = out_size - 1;
        memcpy(out, in, n);
        out[n] = '\0';
        return 1;
    }
    p075_cp437_to_utf8(in, in_len, out);
    return 0;
}

/* ---- builders -------------------------------------------------------- */

size_t p075_build_position(uint8_t *buf, size_t cap, p075_vec3 pos)
{
    if (cap < 1 + sizeof pos)
        return 0;
    buf[0] = P075_POSITION;
    memcpy(buf + 1, &pos, sizeof pos);
    return 1 + sizeof pos;
}

size_t p075_build_orientation(uint8_t *buf, size_t cap, p075_vec3 ori)
{
    if (cap < sizeof(p075_orientation))
        return 0;
    buf[0] = P075_ORIENTATION;
    memcpy(buf + 1, &ori, sizeof ori);
    return 1 + sizeof ori;
}

size_t p075_build_input(uint8_t *buf, size_t cap, uint8_t player, uint8_t keys)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_INPUT;
    buf[1] = player;
    buf[2] = keys;
    return 3;
}

size_t p075_build_weapon_input(uint8_t *buf, size_t cap, uint8_t player, int primary, int secondary)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_WEAPONINPUT;
    buf[1] = player;
    buf[2] = (uint8_t) ((primary ? 1 : 0) | (secondary ? 2 : 0));
    return 3;
}

size_t p075_build_hit(uint8_t *buf, size_t cap, uint8_t victim, uint8_t hit_type)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_HIT; /* id 5; the direction decides Hit vs SetHP */
    buf[1] = victim;
    buf[2] = hit_type;
    return 3;
}

size_t p075_build_grenade(uint8_t *buf, size_t cap, uint8_t player, float fuse, p075_vec3 pos, p075_vec3 vel)
{
    p075_grenade g;
    size_t at = 0;
    memset(&g, 0, sizeof g);
    g.id = P075_GRENADE;
    g.player_id = player;
    g.fuse = fuse;
    g.pos = pos;
    g.vel = vel;
    if (!put(buf, cap, &at, &g, sizeof g))
        return 0;
    return at;
}

size_t p075_build_set_tool(uint8_t *buf, size_t cap, uint8_t player, uint8_t tool)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_SETTOOL;
    buf[1] = player;
    buf[2] = tool;
    return 3;
}

size_t p075_build_set_color(uint8_t *buf, size_t cap, uint8_t player, p075_color color)
{
    if (cap < 5)
        return 0;
    buf[0] = P075_SETCOLOR;
    buf[1] = player;
    buf[2] = color.b;
    buf[3] = color.g;
    buf[4] = color.r;
    return 5;
}

size_t p075_build_join(uint8_t *buf, size_t cap, uint8_t player, int8_t team, uint8_t weapon, uint8_t tool,
                       uint32_t kills, p075_color color, const char *name)
{
    size_t at = 0;
    size_t n = strlen(name);
    if (n > 15)
        n = 15; /* the classic limit */
    if (!put(buf, cap, &at, &(uint8_t) {P075_EXISTINGPLAYER}, 1) ||
        !put(buf, cap, &at, &player, 1) || !put(buf, cap, &at, &team, 1) ||
        !put(buf, cap, &at, &weapon, 1) || !put(buf, cap, &at, &tool, 1) ||
        !put(buf, cap, &at, &kills, 4) || !put(buf, cap, &at, &color, 3) ||
        !put(buf, cap, &at, name, n) || !put(buf, cap, &at, "", 1))
        return 0;
    return at;
}

size_t p075_build_block_action(uint8_t *buf, size_t cap, uint8_t player, uint8_t type, int32_t x, int32_t y,
                               int32_t z)
{
    p075_blockaction a;
    size_t at = 0;
    memset(&a, 0, sizeof a);
    a.id = P075_BLOCKACTION;
    a.player_id = player;
    a.type = type;
    a.pos.x = x;
    a.pos.y = y;
    a.pos.z = z;
    if (!put(buf, cap, &at, &a, sizeof a))
        return 0;
    return at;
}

size_t p075_build_block_line(uint8_t *buf, size_t cap, uint8_t player, p075_vec3i from, p075_vec3i to)
{
    p075_blockline l;
    size_t at = 0;
    memset(&l, 0, sizeof l);
    l.id = P075_BLOCKLINE;
    l.player_id = player;
    l.start = from;
    l.end = to;
    if (!put(buf, cap, &at, &l, sizeof l))
        return 0;
    return at;
}

size_t p075_build_chat(uint8_t *buf, size_t cap, uint8_t player, uint8_t type, const char *text,
                       size_t text_len)
{
    size_t at = 0;
    if (text_len > 254)
        text_len = 254; /* keep room for the NUL */
    if (!put(buf, cap, &at, &(uint8_t) {P075_CHATMESSAGE}, 1) ||
        !put(buf, cap, &at, &player, 1) || !put(buf, cap, &at, &type, 1) ||
        !put(buf, cap, &at, text, text_len) || !put(buf, cap, &at, "", 1))
        return 0;
    return at;
}

size_t p075_build_weapon_reload(uint8_t *buf, size_t cap, uint8_t player, uint8_t ammo, uint8_t reserve)
{
    if (cap < 4)
        return 0;
    buf[0] = P075_WEAPONRELOAD;
    buf[1] = player;
    buf[2] = ammo;
    buf[3] = reserve;
    return 4;
}

size_t p075_build_change_team(uint8_t *buf, size_t cap, uint8_t player, int8_t team)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_CHANGETEAM;
    buf[1] = player;
    buf[2] = (uint8_t) team;
    return 3;
}

size_t p075_build_change_weapon(uint8_t *buf, size_t cap, uint8_t player, uint8_t weapon)
{
    if (cap < 3)
        return 0;
    buf[0] = P075_CHANGEWEAPON;
    buf[1] = player;
    buf[2] = weapon;
    return 3;
}

size_t p075_build_map_cached(uint8_t *buf, size_t cap, int cached)
{
    if (cap < 2)
        return 0;
    buf[0] = P075_MAPCACHED;
    buf[1] = (uint8_t) (cached ? 1 : 0);
    return 2;
}

size_t p075_build_handshake_return(uint8_t *buf, size_t cap, int32_t challenge)
{
    if (cap < 5)
        return 0;
    buf[0] = P075_HANDSHAKERETURN;
    memcpy(buf + 1, &challenge, 4);
    return 5;
}

size_t p075_build_version_send(uint8_t *buf, size_t cap, char client, uint8_t major, uint8_t minor,
                               uint8_t revision, const char *os)
{
    size_t at = 0;
    uint8_t zero[64] = {0};
    size_t n = strlen(os);
    if (n > sizeof zero)
        n = sizeof zero;
    memcpy(zero, os, n);
    if (!put(buf, cap, &at, &(uint8_t) {P075_VERSIONSEND}, 1) || !put(buf, cap, &at, &client, 1) ||
        !put(buf, cap, &at, &major, 1) || !put(buf, cap, &at, &minor, 1) ||
        !put(buf, cap, &at, &revision, 1) || !put(buf, cap, &at, zero, sizeof zero))
        return 0;
    return at;
}

size_t p075_build_extinfo(uint8_t *buf, size_t cap, const uint8_t *pairs, size_t pair_count)
{
    size_t at = 0;
    uint8_t count = (uint8_t) pair_count;
    if (!put(buf, cap, &at, &(uint8_t) {P075_EXTINFO}, 1) || !put(buf, cap, &at, &count, 1) ||
        !put(buf, cap, &at, pairs, pair_count * 2))
        return 0;
    return at;
}

size_t p075_build_flashlight(uint8_t *buf, size_t cap, uint8_t player, int on)
{
    if (cap < 4)
        return 0;
    buf[0] = P075_FLASHLIGHT;
    buf[1] = 0; /* sub 0 = Light */
    buf[2] = player;
    buf[3] = (uint8_t) (on ? 1 : 0);
    return 4;
}
