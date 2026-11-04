/*
 * Golden vectors and round trips for the 1.0 codec. Build and run with
 * `make test` in this folder. Every hex string here is also in 1.0.md, so a
 * port in another language can use the same file as its first test.
 *
 * MIT licence.
 */
#include "proto10.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_bytes(const char *what, const uint8_t *got, size_t got_len, const char *hex)
{
    uint8_t want[256];
    size_t n = 0;
    for (const char *p = hex; *p; p++) {
        int hi;
        if (*p == ' ')
            continue;
        if (*p >= '0' && *p <= '9')
            hi = *p - '0';
        else if (*p >= 'A' && *p <= 'F')
            hi = *p - 'A' + 10;
        else if (*p >= 'a' && *p <= 'f')
            hi = *p - 'a' + 10;
        else {
            printf("FAIL %s: bad hex\n", what);
            failures++;
            return;
        }
        want[n / 2] = (uint8_t) (want[n / 2] << 4 | (unsigned) hi);
        n++;
    }
    n /= 2;
    if (n != got_len || memcmp(want, got, n) != 0) {
        printf("FAIL %s\n  want %s\n  got  ", what, hex);
        for (size_t i = 0; i < got_len; i++)
            printf("%02X ", got[i]);
        printf("\n");
        failures++;
        return;
    }
    printf("ok   %s\n", what);
}

static void test_packets(void)
{
    uint8_t buf[512];
    p10_writer w;

    p10_writer_init(&w, buf, sizeof buf);
    p10_build_build_block(&w, 256, 255, 62, (p10_color) {0x33, 0x22, 0x11});
    expect_bytes("BuildBlock (256,255,62)", buf, w.size, "5D 80 04 FE 03 7C 33 22 11");

    p10_writer_init(&w, buf, sizeof buf);
    p10_build_build_line(&w, 10, 20, 30, 10, 20, 25, (p10_color) {0x00, 0x00, 0xFF});
    expect_bytes("BuildLine", buf, w.size, "5E 14 28 3C 14 28 32 00 00 FF");

    p10_writer_init(&w, buf, sizeof buf);
    p10_build_chat(&w, 1, "gg");
    expect_bytes("ChatMessage team gg", buf, w.size, "46 01 02 67 67");

    /* TerrainUpdate is built by the server; feed the vectors to the decoder */
    {
        const uint8_t create[] = {0x28, 0x0A, 0x14, 0x1E, 0x64, 0xFF, 0x80, 0x00, 0x01};
        p10_reader r;
        int32_t x = -1, y = -1, z = -1;
        uint8_t hp = 0, red = 0, green = 0, blue = 0, type = 0;
        p10_reader_init(&r, create, sizeof create);
        if (!p10_decode_terrain(&r, NULL, NULL)) {
            printf("FAIL TerrainUpdate create did not decode\n");
            failures++;
        } else {
            printf("ok   TerrainUpdate create decodes\n");
        }
        (void) x;
        (void) y;
        (void) z;
        (void) hp;
        (void) red;
        (void) green;
        (void) blue;
        (void) type;

        const uint8_t destroy[] = {0x28, 0x80, 0xAC, 0x02, 0x05, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x01};
        p10_reader_init(&r, destroy, sizeof destroy);
        if (!p10_decode_terrain(&r, NULL, NULL)) {
            printf("FAIL TerrainUpdate destroy did not decode\n");
            failures++;
        } else {
            printf("ok   TerrainUpdate destroy decodes\n");
        }
        /* every truncation must fail (only the id byte is an empty update,
           which is legal) */
        for (size_t n = 2; n < sizeof create; n++) {
            p10_reader_init(&r, create, n);
            if (p10_decode_terrain(&r, NULL, NULL)) {
                printf("FAIL TerrainUpdate accepted a truncation at %zu\n", n);
                failures++;
            }
        }
    }

    /* a death without damage info: no trailing byte, and it must decode */
    {
        const uint8_t die[] = {0x20, 0x05, 0x00, 0x00};
        p10_reader r;
        uint32_t entity = 0, from = 0;
        uint8_t type = 0, dmg = 0;
        uint64_t param = 0;
        int has_damage = 1;
        p10_vec3 fire = {0, 0, 0}, hit = {0, 0, 0};
        p10_reader_init(&r, die, sizeof die);
        if (!p10_decode_entity_die(&r, &entity, &type, &param, &has_damage, &dmg, &from, &fire, &hit) ||
            has_damage || entity != 5) {
            printf("FAIL EntityDie without damage info\n");
            failures++;
        } else {
            printf("ok   EntityDie without damage info\n");
        }
    }
}

static void test_items(void)
{
    p10_entity_item in, out;
    uint8_t buf[256];
    p10_writer w;
    p10_reader r;

    memset(&in, 0, sizeof in);
    in.entity_id = 7;
    in.mask = P10_EU_CREATE | P10_EU_FLAGS | P10_EU_TRAJECTORY | P10_EU_PLAYER_INPUT | P10_EU_TOOL |
              P10_EU_BLOCK_COLOR | P10_EU_HEALTH;
    in.entity_type = P10_ENT_PLAYER;
    in.body_skin = (const uint8_t *) "class/commando";
    in.body_skin_len = strlen("class/commando");
    in.weapon_skin[0] = (const uint8_t *) "revolver";
    in.weapon_skin_len[0] = strlen("revolver");
    in.weapon[0] = (p10_weapon_params) {0, 0, 0, 0, 400, 1700, 500, 45, 6};
    in.flags = P10_EF_PLAYER_CLIP | P10_EF_JETPACK;
    in.trajectory.type = P10_TRAJ_PLAYER;
    in.trajectory.origin = (p10_vec3) {100.5f, 200.25f, 40.0f};
    in.trajectory.velocity = (p10_vec3) {0.5f, -0.5f, 0.0f};
    in.trajectory.euler = (p10_vec3) {0.1f, 0.0f, 1.2f};
    in.input.flags = P10_IN_TOOL_PRIMARY | P10_IN_SPRINT;
    in.input.stance = P10_STANCE_CROUCH;
    in.input.xmove = 10;
    in.input.ymove = -20;
    in.tool = P10_TOOL_WEAPON2;
    in.block_color = (p10_color) {1, 2, 3};
    in.health = 77;

    p10_writer_init(&w, buf, sizeof buf);
    p10_write_entity_item(&w, &in);
    if (!p10_writer_ok(&w)) {
        printf("FAIL entity item encode\n");
        failures++;
        return;
    }
    p10_reader_init(&r, buf, w.size);
    if (!p10_read_entity_item(&r, &out) || !p10_at_end(&r)) {
        printf("FAIL entity item decode\n");
        failures++;
        return;
    }
    if (out.entity_id != 7 || out.entity_type != P10_ENT_PLAYER || out.health != 77 ||
        out.trajectory.type != P10_TRAJ_PLAYER || out.trajectory.origin.y != 200.25f ||
        out.input.ymove != -20 || out.tool != P10_TOOL_WEAPON2 || out.flags != in.flags) {
        printf("FAIL entity item round trip\n");
        failures++;
        return;
    }
    printf("ok   entity item round trip\n");

    /* the compact player trajectory must round trip its fields */
    memset(&in, 0, sizeof in);
    in.entity_id = 9;
    in.mask = P10_EU_TRAJECTORY;
    in.trajectory.type = P10_TRAJ_PLAYER_COMPACT;
    in.trajectory.origin = (p10_vec3) {123.5f, 45.25f, 33.0f};
    in.trajectory.velocity = (p10_vec3) {3.5f, -2.25f, 0.5f};
    in.trajectory.euler = (p10_vec3) {-0.2f, 0.0f, 2.5f};
    p10_writer_init(&w, buf, sizeof buf);
    p10_write_entity_item(&w, &in);
    p10_reader_init(&r, buf, w.size);
    memset(&out, 0, sizeof out);
    if (!p10_read_entity_item(&r, &out) || out.trajectory.type != P10_TRAJ_PLAYER ||
        out.trajectory.origin.x != 123.5f || out.trajectory.origin.z != 33.0f ||
        out.trajectory.velocity.y != -2.25f) {
        printf("FAIL compact trajectory round trip\n");
        failures++;
        return;
    }
    printf("ok   compact trajectory round trip (%zu bytes)\n", w.size);
}

static void test_truncations(void)
{
    uint8_t buf[256];
    p10_writer w;
    p10_player_item item;
    int bad = 0;

    memset(&item, 0, sizeof item);
    item.player_id = 3;
    item.mask = P10_PU_CREATE | P10_PU_SCORE | P10_PU_FLAGS | P10_PU_TEAM | P10_PU_STATS | P10_PU_PING |
                P10_PU_PERKS;
    snprintf(item.name, sizeof item.name, "Deuce");
    item.team = P10_TEAM_2;
    p10_writer_init(&w, buf, sizeof buf);
    p10_write_player_item(&w, &item);
    for (size_t n = 1; n < w.size; n++) {
        p10_reader r;
        p10_player_item out;
        p10_reader_init(&r, buf, n);
        if (p10_read_player_item(&r, &out)) {
            printf("FAIL player item accepted a truncation at %zu\n", n);
            bad++;
        }
    }
    if (bad) {
        failures += bad;
        return;
    }
    printf("ok   player item rejects every truncation\n");
}

int main(void)
{
    test_packets();
    test_items();
    test_truncations();
    if (failures) {
        printf("\n%d failure(s)\n", failures);
        return 1;
    }
    printf("\nall codec tests passed\n");
    return 0;
}
