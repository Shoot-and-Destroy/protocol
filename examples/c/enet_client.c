/*
 * A minimal client over ENet: connect data, range coder, both protocols, and
 * a 1.0 handshake that gets as far as a body in the world.
 *
 * This is an example, not a game: it prints what it receives and sends the
 * least a server needs. Build with `make enet` (needs ENet 1.3).
 *
 * MIT licence.
 */
#include <enet/enet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto075.h"
#include "proto10.h"

#define CONNECT_075 3
#define CONNECT_10 0x08101919u

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
    int proto10;

    /* map download */
    uint8_t *map;
    size_t map_size;
    size_t map_got;

    uint32_t self;
    int in_game;
} client_t;

static void send_packet(client_t *c, int channel, int reliable, const void *data, size_t len)
{
    if (c->peer == NULL || len == 0)
        return;
    ENetPacket *p = enet_packet_create(data, len, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (enet_peer_send(c->peer, (enet_uint8) channel, p) != 0)
        enet_packet_destroy(p);
}

static void send_generic(client_t *c, int argc, const char *const *argv)
{
    uint8_t buf[512];
    p10_writer w;
    p10_writer_init(&w, buf, sizeof buf);
    p10_build_generic(&w, argc, argv);
    if (p10_writer_ok(&w))
        send_packet(c, 0, 1, buf, w.size);
}

/* ---- protocol 1.0 ---------------------------------------------------- */

static void p10_start(client_t *c)
{
    uint8_t nonce[256];
    uint8_t buf[1024];
    p10_writer w;

    for (size_t i = 0; i < sizeof nonce; i++)
        nonce[i] = (uint8_t) rand();

    p10_writer_init(&w, buf, sizeof buf);
    p10_build_initiate(&w, "protocol-example 1.0", "example", "en_US", "enet_example", nonce,
                       0 /* 0 = ask for VXL, 80 = ngmap q80 */);
    if (!p10_writer_ok(&w)) {
        fprintf(stderr, "InitiateConnection does not fit\n");
        return;
    }
    send_packet(c, 0, 1, buf, w.size);

    /* nothing is validated: an empty certificate is what everyone sends */
    p10_writer_init(&w, buf, sizeof buf);
    p10_build_client_certificate(&w, 0, NULL, 0, NULL, 0);
    send_packet(c, 0, 1, buf, w.size);
}

static void p10_handle(client_t *c, const uint8_t *data, size_t len, int reliable)
{
    (void) reliable;
    uint8_t id = data[0];

    switch (id) {
    case P10_GREETING: {
        p10_reader r;
        uint8_t nonce[128];
        p10_reader_init(&r, data, len);
        if (p10_decode_greeting(&r, nonce)) {
            printf("1.0: Greeting\n");
            p10_start(c);
        } else {
            printf("1.0: bad Greeting\n");
        }
        break;
    }
    case P10_SERVER_CERTIFICATE: /* we already sent ours */
        break;
    case P10_GAME_STATE_HEADER: {
        p10_reader r;
        char key[128], value[512];
        p10_reader_init(&r, data, len);
        p10_read_u8(&r); /* the id */
        while (p10_reader_ok(&r)) {
            p10_read_string(&r, key, sizeof key);
            if (key[0] == '\0')
                break;
            p10_read_string(&r, value, sizeof value);
            printf("1.0: header %s = %s\n", key, value);
            if (strcmp(key, "map-bytes") == 0) {
                c->map_size = (size_t) strtoul(value, NULL, 10);
                free(c->map);
                c->map = c->map_size ? malloc(c->map_size) : NULL;
                c->map_got = 0;
            }
        }
        break;
    }
    case P10_MAP_DATA: {
        p10_reader r;
        size_t n;
        const uint8_t *p;
        p10_reader_init(&r, data, len);
        p10_read_u8(&r);
        p = p10_read_bytes(&r, &n);
        if (p != NULL && c->map != NULL && c->map_got + n <= c->map_size) {
            memcpy(c->map + c->map_got, p, n);
            c->map_got += n;
        }
        break;
    }
    case P10_MAP_DATA_FINAL: {
        uint8_t ack = P10_MAP_DATA_ACKNOWLEDGE;
        printf("1.0: map final, %zu of %zu bytes\n", c->map_got, c->map_size);
        /* decode the map here, apply the TerrainUpdates kept meanwhile, then
           acknowledge. The download is zlib: map-format says VXL or ngmap. */
        send_packet(c, 0, 1, &ack, 1);
        break;
    }
    case P10_GAME_STATE_FINAL: {
        printf("1.0: GameStateFinal\n");
        const char *argv[] = {"join", "0", "0"}; /* team 1, rifle */
        send_generic(c, 3, argv);
        break;
    }
    case P10_GENERIC_COMMAND: {
        p10_reader r;
        char part[256];
        p10_reader_init(&r, data, len);
        p10_read_u8(&r);
        while (p10_reader_ok(&r) && p10_has_more(&r)) {
            p10_read_string(&r, part, sizeof part);
            if (strcmp(part, "local-player") == 0) {
                p10_read_string(&r, part, sizeof part);
                c->self = (uint32_t) strtoul(part, NULL, 10);
                c->in_game = 1;
                printf("1.0: we are player %u\n", c->self);
                break;
            }
            printf("1.0: command %s\n", part);
        }
        break;
    }
    case P10_KICK: {
        p10_reader r;
        char reason[256];
        p10_reader_init(&r, data, len);
        if (p10_decode_kick(&r, reason, sizeof reason))
            printf("1.0: kicked: %s\n", reason);
        break;
    }
    case P10_ENTITY_UPDATE:
    case P10_PLAYER_UPDATE:
    case P10_TERRAIN_UPDATE:
    case P10_DAMAGE:
    case P10_ENTITY_DIE:
    case P10_ENTITY_EVENT:
    case P10_CHAT_BROADCAST:
    case P10_GAME_MODE_STATE:
        /* decode these with the functions in proto10.h */
        break;
    default:
        printf("1.0: packet id %u (%zu bytes), ignored\n", id, len);
        break;
    }
}

/* your body, 30 times a second, and reliably right before an action */
static void p10_send_body(client_t *c, float x, float y, float z, float pitch)
{
    uint8_t buf[128];
    p10_writer w;
    p10_entity_item it;

    memset(&it, 0, sizeof it);
    it.entity_id = c->self;
    it.mask = P10_EU_TRAJECTORY | P10_EU_PLAYER_INPUT | P10_EU_TOOL;
    it.trajectory.type = P10_TRAJ_PLAYER;
    it.trajectory.origin = (p10_vec3) {x, y, z};
    it.trajectory.velocity = (p10_vec3) {0, 0, 0};
    it.trajectory.euler = (p10_vec3) {pitch, 0, 0};
    it.input.flags = 0;
    it.input.stance = P10_STANCE_STAND;
    it.tool = P10_TOOL_WEAPON1;

    p10_writer_init(&w, buf, sizeof buf);
    p10_build_client_entity(&w, &it, 1);
    if (p10_writer_ok(&w))
        send_packet(c, 1, 0, buf, w.size); /* channel 1, unreliable */
}

/* ---- protocol 0.75 --------------------------------------------------- */

static void p075_handle(client_t *c, const uint8_t *data, size_t len)
{
    switch (data[0]) {
    case P075_MAPSTART: {
        if (len < (int) sizeof(p075_mapstart))
            break;
        const p075_mapstart *m = (const p075_mapstart *) data;
        c->map_size = m->map_size;
        free(c->map);
        c->map = c->map_size ? malloc(c->map_size) : NULL;
        c->map_got = 0;
        printf("0.75: map start, %u bytes compressed\n", (unsigned) m->map_size);
        break;
    }
    case P075_MAPCHUNK:
        if (c->map != NULL && c->map_got + (len - 1) <= c->map_size) {
            memcpy(c->map + c->map_got, data + 1, len - 1);
            c->map_got += len - 1;
        }
        break;
    case P075_STATEDATA: {
        const p075_statedata *s = (const p075_statedata *) data;
        uint8_t buf[128];
        size_t n;
        printf("0.75: state data, %zu of %zu map bytes, mode %u\n", c->map_got, c->map_size, s->mode);
        c->self = s->player_id;
        n = p075_build_join(buf, sizeof buf, c->self, P075_TEAM_1, P075_RIFLE, P075_TOOL_GUN, 0,
                            (p075_color) {111, 111, 111}, "enet_example");
        send_packet(c, 0, 1, buf, n);
        break;
    }
    case P075_CREATEPLAYER: {
        const p075_createplayer *p = (const p075_createplayer *) data;
        printf("0.75: player %u spawned at %.1f %.1f %.1f\n", p->player_id, p->pos.x, p->pos.y, p->pos.z);
        break;
    }
    case P075_HANDSHAKEINIT: {
        uint8_t buf[8];
        int32_t challenge;
        size_t n;
        memcpy(&challenge, data + 1, 4);
        n = p075_build_handshake_return(buf, sizeof buf, challenge);
        send_packet(c, 0, 1, buf, n);
        break;
    }
    case P075_VERSIONGET: {
        uint8_t buf[128];
        size_t n = p075_build_version_send(buf, sizeof buf, 'B', 0, 1, 5, "example");
        send_packet(c, 0, 1, buf, n);
        break;
    }
    case P075_KILLACTION:
    case P075_CHATMESSAGE:
    case P075_BLOCKACTION:
    case P075_WORLDUPDATE:
        break;
    default:
        printf("0.75: packet id %u (%zu bytes), ignored\n", data[0], len);
        break;
    }
}

/* ---- main ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    const char *hostname = argc > 1 ? argv[1] : "127.0.0.1";
    enet_uint16 port = argc > 2 ? (enet_uint16) atoi(argv[2]) : 32887;
    int proto10 = argc > 3 ? atoi(argv[3]) : 1;

    if (enet_initialize() != 0) {
        fprintf(stderr, "enet_initialize failed\n");
        return 1;
    }

    client_t c;
    memset(&c, 0, sizeof c);
    c.proto10 = proto10;

    c.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (c.host == NULL) {
        fprintf(stderr, "enet_host_create failed\n");
        return 1;
    }
    enet_host_compress_with_range_coder(c.host); /* mandatory for AoS */

    ENetAddress address;
    if (enet_address_set_host(&address, hostname) != 0) {
        fprintf(stderr, "cannot resolve %s\n", hostname);
        return 1;
    }
    address.port = port;
    c.peer = enet_host_connect(c.host, &address, 2, proto10 ? CONNECT_10 : CONNECT_075);
    if (c.peer == NULL) {
        fprintf(stderr, "enet_host_connect failed\n");
        return 1;
    }

    uint32_t last_send = 0;
    for (;;) {
        ENetEvent e;
        int r = enet_host_service(c.host, &e, 10);
        if (r < 0)
            break;
        if (r == 0) {
            /* a 1.0 body update at 30 Hz */
            if (c.proto10 && c.in_game && enet_time_get() - last_send >= 33) {
                last_send = enet_time_get();
                p10_send_body(&c, 256.0f, 256.0f, 40.0f, 0.0f);
            }
            continue;
        }
        switch (e.type) {
        case ENET_EVENT_TYPE_CONNECT:
            printf("connected (%s)\n", proto10 ? "1.0" : "0.75");
            if (!proto10) {
                /* nothing to do: the server starts sending the map */
            }
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            if (proto10)
                p10_handle(&c, e.packet->data, e.packet->dataLength, 1);
            else
                p075_handle(&c, e.packet->data, e.packet->dataLength);
            enet_packet_destroy(e.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            printf("disconnected: %u\n", (unsigned) e.data);
            enet_host_destroy(c.host);
            enet_deinitialize();
            free(c.map);
            return 0;
        default:
            break;
        }
    }

    enet_host_destroy(c.host);
    enet_deinitialize();
    free(c.map);
    return 0;
}
