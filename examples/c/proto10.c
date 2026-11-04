/*
 * Protocol 1.0 primitive codec. See proto10.h and 1.0.md.
 *
 * MIT licence.
 */
#include "proto10.h"

#include <math.h>
#include <string.h>

/* ---- reader ---------------------------------------------------------- */

void p10_reader_init(p10_reader *r, const void *data, size_t size)
{
    r->data = (const uint8_t *) data;
    r->size = size;
    r->pos = 0;
    r->error = 0;
}

int p10_reader_ok(const p10_reader *r) { return !r->error; }
int p10_at_end(const p10_reader *r) { return !r->error && r->pos == r->size; }
int p10_has_more(const p10_reader *r) { return !r->error && r->pos < r->size; }

static int need(p10_reader *r, size_t n)
{
    if (r->error || r->size - r->pos < n) {
        r->error = 1;
        return 0;
    }
    return 1;
}

uint8_t p10_read_u8(p10_reader *r) { return need(r, 1) ? r->data[r->pos++] : 0; }

uint16_t p10_read_u16(p10_reader *r)
{
    if (!need(r, 2))
        return 0;
    uint16_t v = (uint16_t) (r->data[r->pos] | ((uint16_t) r->data[r->pos + 1] << 8));
    r->pos += 2;
    return v;
}

uint32_t p10_read_u32(p10_reader *r)
{
    if (!need(r, 4))
        return 0;
    const uint8_t *p = r->data + r->pos;
    r->pos += 4;
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

/* LEB128, at most 10 bytes; unlike the ngspades reference this keeps 64 bits
   and rejects an endless run of 0x80 (1.0.md section 3.1) */
uint64_t p10_read_varint(p10_reader *r)
{
    uint64_t v = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        uint8_t b = p10_read_u8(r);
        if (r->error)
            return 0;
        v |= (uint64_t) (b & 0x7f) << shift;
        if (!(b & 0x80))
            return v;
    }
    r->error = 1;
    return 0;
}

uint32_t p10_read_varint32(p10_reader *r)
{
    uint64_t v = p10_read_varint(r);
    if (v > 0xFFFFFFFFu) {
        r->error = 1;
        return 0;
    }
    return (uint32_t) v;
}

int64_t p10_read_svarint(p10_reader *r)
{
    uint64_t u = p10_read_varint(r);
    return (u & 1) ? -1 - (int64_t) (u >> 1) : (int64_t) (u >> 1);
}

float p10_read_f32(p10_reader *r)
{
    uint32_t u = p10_read_u32(r);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

p10_vec3 p10_read_vec3(p10_reader *r)
{
    p10_vec3 v;
    v.x = p10_read_f32(r);
    v.y = p10_read_f32(r);
    v.z = p10_read_f32(r);
    return v;
}

p10_color p10_read_color(p10_reader *r)
{
    p10_color c;
    c.b = p10_read_u8(r);
    c.g = p10_read_u8(r);
    c.r = p10_read_u8(r);
    return c;
}

const uint8_t *p10_read_bytes(p10_reader *r, size_t *len)
{
    uint64_t n = p10_read_varint(r);
    *len = 0;
    if (n > P10_BYTES_MAX || !need(r, (size_t) n)) {
        r->error = 1;
        return NULL;
    }
    const uint8_t *p = r->data + r->pos;
    r->pos += (size_t) n;
    *len = (size_t) n;
    return p;
}

void p10_read_string(p10_reader *r, char *out, size_t out_size)
{
    size_t len;
    const uint8_t *p = p10_read_bytes(r, &len);
    out[0] = '\0';
    if (p == NULL || memchr(p, 0, len) != NULL || len >= out_size) {
        r->error = 1;
        return;
    }
    memcpy(out, p, len);
    out[len] = '\0';
}

/* ---- writer ---------------------------------------------------------- */

void p10_writer_init(p10_writer *w, void *buf, size_t cap)
{
    w->data = (uint8_t *) buf;
    w->cap = cap;
    w->size = 0;
    w->error = 0;
}

int p10_writer_ok(const p10_writer *w) { return !w->error; }

void p10_write_raw(p10_writer *w, const void *data, size_t n)
{
    if (w->error || w->cap - w->size < n) {
        w->error = 1;
        return;
    }
    if (n)
        memcpy(w->data + w->size, data, n);
    w->size += n;
}

void p10_write_u8(p10_writer *w, uint8_t v) { p10_write_raw(w, &v, 1); }

void p10_write_u16(p10_writer *w, uint16_t v)
{
    uint8_t b[2] = {(uint8_t) v, (uint8_t) (v >> 8)};
    p10_write_raw(w, b, 2);
}

void p10_write_u32(p10_writer *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24)};
    p10_write_raw(w, b, 4);
}

void p10_write_varint(p10_writer *w, uint64_t v)
{
    do {
        uint8_t b = (uint8_t) (v & 0x7f);
        v >>= 7;
        p10_write_u8(w, v ? (uint8_t) (b | 0x80) : b);
    } while (v && !w->error);
}

void p10_write_svarint(p10_writer *w, int64_t v)
{
    p10_write_varint(w, v < 0 ? (((uint64_t) (-1 - v)) << 1) | 1 : ((uint64_t) v) << 1);
}

void p10_write_f32(p10_writer *w, float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    p10_write_u32(w, u);
}

void p10_write_vec3(p10_writer *w, p10_vec3 v)
{
    p10_write_f32(w, v.x);
    p10_write_f32(w, v.y);
    p10_write_f32(w, v.z);
}

void p10_write_color(p10_writer *w, p10_color c)
{
    p10_write_u8(w, c.b);
    p10_write_u8(w, c.g);
    p10_write_u8(w, c.r);
}

void p10_write_bytes(p10_writer *w, const void *data, size_t n)
{
    p10_write_varint(w, n);
    p10_write_raw(w, data, n);
}

void p10_write_string(p10_writer *w, const char *s)
{
    if (s == NULL)
        s = "";
    p10_write_bytes(w, s, strlen(s));
}

/* ---- weapons, trajectories, input ----------------------------------- */

void p10_write_weapon(p10_writer *w, const p10_weapon_params *p)
{
    p10_write_u8(w, (uint8_t) ((p->full_auto ? 1 : 0) | (p->slow_reload ? 2 : 0) | (p->penetrative ? 4 : 0) |
                               ((p->type & 3) << 3)));
    p10_write_u16(w, p->fire_interval_ms);
    p10_write_u16(w, p->reload_ms); /* wire order differs from the struct: */
    p10_write_u16(w, p->raise_ms);  /* reload and raise come before damage */
    p10_write_u8(w, p->max_damage);
    p10_write_u8(w, p->magazine);
}

int p10_read_weapon(p10_reader *r, p10_weapon_params *p)
{
    uint8_t f = p10_read_u8(r);
    if (f & 0xe0) {
        r->error = 1;
        return 0;
    }
    p->full_auto = f & 1;
    p->slow_reload = (f >> 1) & 1;
    p->penetrative = (f >> 2) & 1;
    p->type = (f >> 3) & 3;
    p->fire_interval_ms = p10_read_u16(r);
    p->reload_ms = p10_read_u16(r);
    p->raise_ms = p10_read_u16(r);
    p->max_damage = p10_read_u8(r);
    p->magazine = p10_read_u8(r);
    return p10_reader_ok(r);
}

/* compact trajectory scales (1.0.md section 6.5) */
#define CPOS 64.0f
#define CVEL 1024.0f
#define CPITCH (32767.0f / 1.57079632679f)
#define CYAW (32767.0f / 3.14159265359f)

void p10_write_trajectory(p10_writer *w, const p10_trajectory *t)
{
    if (t->type == P10_TRAJ_PLAYER_COMPACT) {
        p10_write_u8(w, (uint8_t) (P10_TRAJ_PLAYER_COMPACT | (t->has_parent ? 0x80 : 0)));
        p10_write_u16(w, (uint16_t) lroundf(t->origin.x * CPOS));
        p10_write_u16(w, (uint16_t) lroundf(t->origin.y * CPOS));
        /* z rounds up (toward the sky): a standing body must never arrive
           inside its floor, or the client's step-up bobs it */
        p10_write_u16(w, (uint16_t) (int16_t) floorf(t->origin.z * CPOS));
        p10_write_u16(w, (uint16_t) (int16_t) lroundf(t->velocity.x * CVEL));
        p10_write_u16(w, (uint16_t) (int16_t) lroundf(t->velocity.y * CVEL));
        p10_write_u16(w, (uint16_t) (int16_t) lroundf(t->velocity.z * CVEL));
        p10_write_u16(w, (uint16_t) (int16_t) lroundf(t->euler.x * CPITCH));
        p10_write_u16(w, (uint16_t) (int16_t) lroundf(t->euler.z * CYAW));
        if (t->has_parent)
            p10_write_varint(w, t->parent);
        return;
    }

    p10_write_u8(w, (uint8_t) ((t->type & 0x7f) | (t->has_parent ? 0x80 : 0)));
    p10_write_vec3(w, t->origin);
    p10_write_vec3(w, t->velocity);
    if (t->type == P10_TRAJ_PLAYER) {
        p10_write_vec3(w, t->euler);
    } else {
        p10_write_vec3(w, t->rotation);
        p10_write_vec3(w, t->angular_velocity);
    }
    if (t->has_parent)
        p10_write_varint(w, t->parent);
}

int p10_read_trajectory(p10_reader *r, p10_trajectory *t)
{
    uint8_t b = p10_read_u8(r);
    memset(t, 0, sizeof *t);
    t->type = b & 0x7f;
    t->has_parent = (b & 0x80) != 0;
    if (t->type >= P10_TRAJ_COUNT) { /* the layout depends on the type */
        r->error = 1;
        return 0;
    }
    if (t->type == P10_TRAJ_PLAYER_COMPACT) {
        t->type = P10_TRAJ_PLAYER;
        t->origin.x = p10_read_u16(r) / CPOS;
        t->origin.y = p10_read_u16(r) / CPOS;
        t->origin.z = (int16_t) p10_read_u16(r) / CPOS;
        t->velocity.x = (int16_t) p10_read_u16(r) / CVEL;
        t->velocity.y = (int16_t) p10_read_u16(r) / CVEL;
        t->velocity.z = (int16_t) p10_read_u16(r) / CVEL;
        t->euler.x = (int16_t) p10_read_u16(r) / CPITCH;
        t->euler.z = (int16_t) p10_read_u16(r) / CYAW;
        if (t->has_parent)
            t->parent = p10_read_varint32(r);
        return p10_reader_ok(r);
    }
    t->origin = p10_read_vec3(r);
    t->velocity = p10_read_vec3(r);
    if (t->type == P10_TRAJ_PLAYER) {
        t->euler = p10_read_vec3(r);
    } else {
        t->rotation = p10_read_vec3(r);
        t->angular_velocity = p10_read_vec3(r);
    }
    if (t->has_parent)
        t->parent = p10_read_varint32(r);
    return p10_reader_ok(r);
}

void p10_write_input(p10_writer *w, const p10_input *in)
{
    p10_write_u8(w, (uint8_t) ((in->flags & 0x1f) | ((in->stance & 3) << 6)));
    p10_write_u8(w, (uint8_t) in->xmove);
    p10_write_u8(w, (uint8_t) in->ymove);
}

int p10_read_input(p10_reader *r, p10_input *in)
{
    uint8_t b = p10_read_u8(r);
    in->flags = b & 0x1f;
    in->stance = (uint8_t) ((b >> 6) & 3);
    in->xmove = (int8_t) p10_read_u8(r);
    in->ymove = (int8_t) p10_read_u8(r);
    if ((b & 0x20) || in->stance > P10_STANCE_PRONE)
        r->error = 1;
    return p10_reader_ok(r);
}

/* ---- entity and player items ---------------------------------------- */

static int skin_ok(const uint8_t *p, size_t n)
{
    if (n > P10_SKIN_MAX)
        return 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = p[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '/' || c == '-'))
            return 0;
    }
    return 1;
}

static int has_owner(uint8_t type)
{
    return type == P10_ENT_COMMAND_POST || type == P10_ENT_FLAG || type == P10_ENT_CHECKPOINT ||
           type >= P10_ENT_SENTRY;
}

void p10_write_entity_item(p10_writer *w, const p10_entity_item *it)
{
    p10_write_varint(w, it->entity_id);
    p10_write_u8(w, it->mask);
    if (it->mask & P10_EU_CREATE) {
        p10_write_u8(w, it->entity_type);
        if (it->entity_type == P10_ENT_PLAYER) {
            p10_write_bytes(w, it->body_skin, it->body_skin_len);
            for (int i = 0; i < 3; i++)
                p10_write_bytes(w, it->weapon_skin[i], it->weapon_skin_len[i]);
            for (int i = 0; i < 3; i++)
                p10_write_weapon(w, &it->weapon[i]);
        }
        if (has_owner(it->entity_type)) {
            p10_write_varint(w, it->owner);
            p10_write_u8(w, it->team);
        }
    }
    if (it->mask & P10_EU_FLAGS)
        p10_write_u8(w, it->flags);
    if (it->mask & P10_EU_TRAJECTORY)
        p10_write_trajectory(w, &it->trajectory);
    if (it->mask & P10_EU_PLAYER_INPUT)
        p10_write_input(w, &it->input);
    if (it->mask & P10_EU_TOOL)
        p10_write_u8(w, it->tool);
    if (it->mask & P10_EU_BLOCK_COLOR)
        p10_write_color(w, it->block_color);
    if (it->mask & P10_EU_HEALTH)
        p10_write_u8(w, it->health);
}

int p10_read_entity_item(p10_reader *r, p10_entity_item *it)
{
    memset(it, 0, sizeof *it);
    it->entity_id = p10_read_varint32(r);
    it->mask = p10_read_u8(r);
    if (it->mask & ~(uint8_t) (P10_EU_CREATE | P10_EU_FLAGS | P10_EU_TRAJECTORY | P10_EU_PLAYER_INPUT |
                              P10_EU_TOOL | P10_EU_BLOCK_COLOR | P10_EU_HEALTH)) {
        r->error = 1; /* an unknown bit's payload cannot be measured */
        return 0;
    }
    if (it->mask & P10_EU_CREATE) {
        it->entity_type = p10_read_u8(r);
        if (it->entity_type >= P10_ENT_COUNT) {
            r->error = 1;
            return 0;
        }
        if (it->entity_type == P10_ENT_PLAYER) {
            it->body_skin = p10_read_bytes(r, &it->body_skin_len);
            if (it->body_skin == NULL || !skin_ok(it->body_skin, it->body_skin_len))
                r->error = 1;
            for (int i = 0; i < 3; i++) {
                it->weapon_skin[i] = p10_read_bytes(r, &it->weapon_skin_len[i]);
                if (it->weapon_skin[i] == NULL)
                    r->error = 1;
                else if (!skin_ok(it->weapon_skin[i], it->weapon_skin_len[i]))
                    r->error = 1;
            }
            for (int i = 0; i < 3; i++) {
                if (!p10_read_weapon(r, &it->weapon[i]))
                    return 0;
            }
        }
        if (has_owner(it->entity_type)) {
            it->owner = p10_read_varint32(r);
            it->team = p10_read_u8(r);
            if (it->team != P10_TEAM_1 && it->team != P10_TEAM_2 && it->team != P10_TEAM_SPECTATOR)
                r->error = 1;
        }
    }
    if (it->mask & P10_EU_FLAGS) {
        it->flags = p10_read_u8(r);
        if (it->flags & ~(P10_EF_PLAYER_CLIP | P10_EF_WEAPON_CLIP | P10_EF_FLY | P10_EF_JETPACK))
            r->error = 1;
    }
    if (it->mask & P10_EU_TRAJECTORY) {
        if (!p10_read_trajectory(r, &it->trajectory))
            return 0;
    }
    if (it->mask & P10_EU_PLAYER_INPUT) {
        if (!p10_read_input(r, &it->input))
            return 0;
    }
    if (it->mask & P10_EU_TOOL) {
        it->tool = p10_read_u8(r);
        if (it->tool > P10_TOOL_WEAPON3)
            r->error = 1;
    }
    if (it->mask & P10_EU_BLOCK_COLOR)
        it->block_color = p10_read_color(r);
    if (it->mask & P10_EU_HEALTH)
        it->health = p10_read_u8(r);
    return p10_reader_ok(r);
}

void p10_write_player_item(p10_writer *w, const p10_player_item *it)
{
    p10_write_varint(w, it->player_id);
    p10_write_u8(w, it->mask);
    if (it->mask & P10_PU_CREATE) /* order: Create, Score, Flags, Team, ... */
        p10_write_string(w, it->name);
    if (it->mask & P10_PU_SCORE)
        p10_write_varint(w, it->score);
    if (it->mask & P10_PU_FLAGS)
        p10_write_u8(w, it->flags);
    if (it->mask & P10_PU_TEAM)
        p10_write_u8(w, it->team);
    if (it->mask & P10_PU_STATS) {
        p10_write_varint(w, it->kills);
        p10_write_varint(w, it->deaths);
    }
    if (it->mask & P10_PU_PING)
        p10_write_varint(w, it->ping_ms);
    if (it->mask & P10_PU_PERKS) {
        p10_write_u8(w, it->perk_points);
        p10_write_u8(w, it->perk_inventory);
        p10_write_u8(w, it->perk_next);
    }
}

int p10_read_player_item(p10_reader *r, p10_player_item *it)
{
    memset(it, 0, sizeof *it);
    it->player_id = p10_read_varint32(r);
    it->mask = p10_read_u8(r);
    if (it->mask & ~(uint8_t) (P10_PU_CREATE | P10_PU_SCORE | P10_PU_FLAGS | P10_PU_TEAM | P10_PU_STATS |
                              P10_PU_PING | P10_PU_PERKS)) {
        r->error = 1;
        return 0;
    }
    if (it->mask & P10_PU_CREATE)
        p10_read_string(r, it->name, sizeof it->name);
    if (it->mask & P10_PU_SCORE)
        it->score = p10_read_varint32(r);
    if (it->mask & P10_PU_FLAGS)
        it->flags = p10_read_u8(r);
    if (it->mask & P10_PU_TEAM) {
        it->team = p10_read_u8(r);
        if (it->team != P10_TEAM_1 && it->team != P10_TEAM_2 && it->team != P10_TEAM_SPECTATOR)
            r->error = 1;
    }
    if (it->mask & P10_PU_STATS) {
        it->kills = p10_read_varint32(r);
        it->deaths = p10_read_varint32(r);
    }
    if (it->mask & P10_PU_PING)
        it->ping_ms = p10_read_varint32(r);
    if (it->mask & P10_PU_PERKS) {
        it->perk_points = p10_read_u8(r);
        it->perk_inventory = p10_read_u8(r);
        it->perk_next = p10_read_u8(r);
    }
    return p10_reader_ok(r);
}

/* ---- packet builders ------------------------------------------------- */

void p10_build_initiate(p10_writer *w, const char *package, const char *environment, const char *locale,
                        const char *player_name, const uint8_t nonce[256], uint8_t map_quality)
{
    p10_write_u8(w, P10_INITIATE_CONNECTION);
    p10_write_string(w, "WorldOfSpades 1.0");
    p10_write_u16(w, 1); /* major */
    p10_write_u16(w, 3); /* minor = protocol revision 1.3 */
    p10_write_u16(w, 0); /* revision */
    p10_write_string(w, package);
    p10_write_string(w, environment);
    p10_write_string(w, locale);
    p10_write_string(w, player_name);
    p10_write_bytes(w, nonce, 256);
    p10_write_u8(w, map_quality); /* 0 = VXL, 1..100 = ngmap */
}

void p10_build_client_certificate(p10_writer *w, int valid, const void *cert, size_t cert_len,
                                  const void *sig, size_t sig_len)
{
    p10_write_u8(w, P10_CLIENT_CERTIFICATE);
    p10_write_u8(w, valid ? 1 : 0);
    if (valid) {
        p10_write_bytes(w, cert, cert_len);
        p10_write_bytes(w, sig, sig_len);
    }
}

void p10_build_generic(p10_writer *w, int argc, const char *const *argv)
{
    p10_write_u8(w, P10_GENERIC_COMMAND);
    for (int i = 0; i < argc; i++)
        p10_write_string(w, argv[i]);
}

void p10_build_client_entity(p10_writer *w, p10_entity_item *items, size_t count)
{
    p10_write_u8(w, P10_CLIENT_SIDE_ENTITY_UPDATE);
    for (size_t i = 0; i < count; i++) {
        items[i].mask &= (uint8_t) ~P10_EU_CREATE; /* never Create from a client */
        p10_write_entity_item(w, &items[i]);
    }
}

void p10_build_hit_entity(p10_writer *w, uint32_t target, uint8_t hit_type, uint8_t damage_type,
                          p10_vec3 fire, p10_vec3 hit)
{
    p10_write_u8(w, P10_HIT_ENTITY);
    p10_write_varint(w, target);
    p10_write_u8(w, hit_type);
    p10_write_u8(w, damage_type);
    p10_write_vec3(w, fire);
    p10_write_vec3(w, hit);
}

void p10_build_hit_terrain(p10_writer *w, int32_t x, int32_t y, int32_t z, uint8_t damage_type,
                           p10_vec3 fire, p10_vec3 hit)
{
    p10_write_u8(w, P10_HIT_TERRAIN);
    p10_write_svarint(w, x);
    p10_write_svarint(w, y);
    p10_write_svarint(w, z);
    p10_write_u8(w, damage_type);
    p10_write_vec3(w, fire);
    p10_write_vec3(w, hit);
}

void p10_build_build_block(p10_writer *w, int32_t x, int32_t y, int32_t z, p10_color color)
{
    p10_write_u8(w, P10_BUILD_BLOCK);
    p10_write_svarint(w, x);
    p10_write_svarint(w, y);
    p10_write_svarint(w, z);
    p10_write_color(w, color); /* B, G, R */
}

void p10_build_build_line(p10_writer *w, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                          int32_t z1, p10_color color)
{
    p10_write_u8(w, P10_BUILD_LINE);
    p10_write_svarint(w, x0);
    p10_write_svarint(w, y0);
    p10_write_svarint(w, z0);
    p10_write_svarint(w, x1);
    p10_write_svarint(w, y1);
    p10_write_svarint(w, z1);
    p10_write_color(w, color);
}

void p10_build_throw_grenade(p10_writer *w, p10_vec3 origin, p10_vec3 velocity, float fuse)
{
    p10_write_u8(w, P10_THROW_GRENADE);
    p10_write_vec3(w, origin);
    p10_write_vec3(w, velocity);
    p10_write_f32(w, fuse);
}

void p10_build_chat(p10_writer *w, uint8_t channel, const char *utf8)
{
    p10_write_u8(w, P10_CHAT_MESSAGE);
    p10_write_u8(w, channel);
    p10_write_string(w, utf8);
}

void p10_build_player_action(p10_writer *w, uint8_t event_type, uint64_t param)
{
    p10_write_u8(w, P10_PLAYER_ACTION);
    p10_write_u8(w, event_type);
    p10_write_varint(w, param);
}

/* ---- decoders -------------------------------------------------------- */

/* every decode starts at the packet's id byte */
static int begin(p10_reader *r, uint8_t id)
{
    uint8_t got = p10_read_u8(r);
    if (got != id) {
        r->error = 1;
        return 0;
    }
    return 1;
}

int p10_decode_greeting(p10_reader *r, uint8_t nonce[128])
{
    char magic[8];
    size_t len;
    const uint8_t *p;
    if (!begin(r, P10_GREETING))
        return 0;
    p10_read_string(r, magic, sizeof magic);
    if (strcmp(magic, "Hello") != 0) {
        r->error = 1;
        return 0;
    }
    p = p10_read_bytes(r, &len);
    if (p == NULL || len != 128) {
        r->error = 1;
        return 0;
    }
    memcpy(nonce, p, 128);
    return p10_at_end(r);
}

int p10_decode_map(p10_reader *r, p10_map_fn fn, void *ctx)
{
    char key[128];
    char value[512];
    for (;;) {
        p10_read_string(r, key, sizeof key);
        if (!p10_reader_ok(r))
            return 0;
        if (key[0] == '\0')
            return 1;
        p10_read_string(r, value, sizeof value);
        if (!p10_reader_ok(r))
            return 0;
        if (fn != NULL)
            fn(ctx, key, value);
    }
}

int p10_decode_kick(p10_reader *r, char *reason, size_t reason_size)
{
    if (!begin(r, P10_KICK))
        return 0;
    p10_read_string(r, reason, reason_size);
    return p10_at_end(r);
}

int p10_decode_chat_broadcast(p10_reader *r, uint32_t *from, uint8_t *channel, char *text, size_t text_size)
{
    if (!begin(r, P10_CHAT_BROADCAST))
        return 0;
    *from = p10_read_varint32(r);
    *channel = p10_read_u8(r);
    if (*channel > 2) {
        r->error = 1;
        return 0;
    }
    p10_read_string(r, text, text_size);
    return p10_at_end(r);
}

int p10_decode_entity_event(p10_reader *r, uint32_t *entity, uint8_t *event_type, uint64_t *param)
{
    if (!begin(r, P10_ENTITY_EVENT))
        return 0;
    *entity = p10_read_varint32(r);
    *event_type = p10_read_u8(r);
    *param = p10_read_varint(r);
    return p10_at_end(r);
}

int p10_decode_entity_die(p10_reader *r, uint32_t *entity, uint8_t *death_type, uint64_t *param,
                          int *has_damage, uint8_t *damage_type, uint32_t *from_entity, p10_vec3 *fire,
                          p10_vec3 *hit)
{
    uint8_t dt;
    if (!begin(r, P10_ENTITY_DIE))
        return 0;
    *entity = p10_read_varint32(r);
    *death_type = p10_read_u8(r);
    *param = p10_read_varint(r);
    *has_damage = 0;
    *from_entity = 0;
    /* check for the end BEFORE reading the damage type: a death without
       damage info has no such byte (ngspades defect D3) */
    if (!p10_has_more(r))
        return p10_reader_ok(r);
    dt = p10_read_u8(r);
    *has_damage = 1;
    *from_entity = 0;
    *damage_type = (uint8_t) (dt & 0x7f);
    *fire = p10_read_vec3(r);
    *hit = p10_read_vec3(r);
    if (dt & 0x80)
        *from_entity = p10_read_varint32(r);
    return p10_at_end(r);
}

int p10_decode_terrain(p10_reader *r, p10_edit_fn fn, void *ctx)
{
    int64_t cx = 0, cy = 0, cz = 0;
    if (!begin(r, P10_TERRAIN_UPDATE))
        return 0;
    while (p10_has_more(r)) {
        uint8_t b = p10_read_u8(r);
        int8_t dx = (int8_t) b;
        if (b == 0x80) { /* cursor moved to an absolute position */
            cx = (int64_t) p10_read_varint(r);
            cy = (int64_t) p10_read_varint(r);
            cz = (int64_t) p10_read_varint(r);
            dx = (int8_t) p10_read_u8(r);
        }
        int8_t dy = (int8_t) p10_read_u8(r);
        int8_t dz = (int8_t) p10_read_u8(r);
        int64_t x = cx + dx, y = cy + dy, z = cz + dz;
        uint8_t health = p10_read_u8(r);
        if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX || z < INT32_MIN ||
            z > INT32_MAX) {
            r->error = 1;
            return 0;
        }
        if (health != 0) {
            uint8_t red = p10_read_u8(r);
            uint8_t green = p10_read_u8(r);
            uint8_t blue = p10_read_u8(r); /* TerrainUpdate is R, G, B */
            uint8_t type = p10_read_u8(r);
            if (fn != NULL && !fn(ctx, (int32_t) x, (int32_t) y, (int32_t) z, health, red, green, blue, type))
                return 0;
        } else {
            uint8_t type = p10_read_u8(r);
            if (fn != NULL && !fn(ctx, (int32_t) x, (int32_t) y, (int32_t) z, 0, 0, 0, 0, type))
                return 0;
        }
        if (r->error)
            return 0;
    }
    return p10_reader_ok(r);
}

/* ---- helpers --------------------------------------------------------- */

p10_vec3 p10_euler_to_front(p10_vec3 euler)
{
    /* front = (-cos(pitch) sin(yaw), cos(pitch) cos(yaw), sin(pitch)) */
    float cp = cosf(euler.x), sp = sinf(euler.x);
    float sy = sinf(euler.z), cy = cosf(euler.z);
    p10_vec3 v;
    v.x = -cp * sy;
    v.y = cp * cy;
    v.z = sp;
    return v;
}

uint32_t p10_crc32(uint32_t crc, const void *data, size_t n)
{
    static uint32_t table[256];
    static int ready;
    const uint8_t *p = (const uint8_t *) data;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = 1;
    }
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
