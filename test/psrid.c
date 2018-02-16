#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include "platform-udp.h"
#include "zhe.h"
#include "zhe-tracing.h"
#include "zhe-assert.h"

#include "zhe-config-deriv.h" /* for ZTIME_TO_SECu32, LATENCY_BUDGET */

#include "zhe-util.h"

#define MAX_KEY 9u

struct ping { uint32_t dst, src, seq; };
struct pong { uint32_t dst, src, seq; };

struct peerinfo {
    /* transmit: */
    uint32_t seq;
    zhe_time_t tping;
    /* receive: */
    uint32_t firstseq;
    uint32_t lastseq;
    unsigned lastseq_init: 1;
    /* print: */
    uint32_t pseq;
};

static uint32_t ownkey;
static struct peerinfo peerinfo[MAX_KEY+1];
static const zhe_rid_t rid_ping = 1;
static const zhe_rid_t rid_pong = 2;
static zhe_pubidx_t pub_ping;
static zhe_pubidx_t pub_pong;
static zhe_subidx_t sub_ping;
static zhe_subidx_t sub_pong;

static void print_state(zhe_time_t tnow)
{
    printf("%"PRIu32".%03"PRIu32" *** [%"PRIu32"]", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey);
    for (uint32_t k = 0; k <= MAX_KEY; k++) {
        if (peerinfo[k].lastseq_init) {
            uint32_t d = peerinfo[k].seq - peerinfo[k].pseq;
            if (d >= 10000) {
                printf(" %1"PRIu32".%1"PRIu32"k", (d/1000)%10, (d/100)%10);
            } else {
                printf(" %4"PRIu32, d % 10000);
            }
        } else {
            printf(" ....");
        }
        peerinfo[k].pseq = peerinfo[k].seq;
    }
    printf("\n");
}

static bool send_next_ping(unsigned to, zhe_time_t tnow)
{
    const struct ping ping = { .src = ownkey, .dst = to, .seq = ++peerinfo[to].seq };
    const int ok = zhe_write(pub_ping, &ping, sizeof(ping), tnow);
#if LATENCY_BUDGET != 0 /* don't want to wait */
    zhe_flush();
#endif
    if (ok) peerinfo[to].tping = tnow;
    return ok >= 0;
}

static void ping_handler(zhe_rid_t rid, const void *payload, zhe_paysize_t size, void *arg)
{
    const zhe_time_t tnow = zhe_platform_time();
    const struct ping * const d = payload;
    assert(size == sizeof(*d));
    if (d->src > MAX_KEY) {
        printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] ping_handler: ping from %"PRIu32" out of range\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, d->src);
        abort();
    }
    if (d->dst != ownkey) {
        /* don't care about pings going to others */
        return;
    }
    if (peerinfo[d->src].lastseq_init) {
        if (d->seq != peerinfo[d->src].lastseq+1) {
            printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] ping_handler: ping from %"PRIu32" out of sequence\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, d->src);
            abort();
        }
        peerinfo[d->src].lastseq = d->seq;
    } else {
        peerinfo[d->src].firstseq = peerinfo[d->src].lastseq = d->seq;
        peerinfo[d->src].lastseq_init = 1;
        printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] ping_handler: %"PRIu32" is alive\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, d->src);
    }
    const struct pong pong = { .src = ownkey, .dst = d->src, .seq = d->seq };
    if (!zhe_write(pub_pong, &pong, sizeof(pong), tnow)) {
        printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] ping_handler: write pong failed\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey);
        abort();
    }
#if LATENCY_BUDGET != 0 /* don't want to wait */
    zhe_flush();
#endif
}

static void pong_handler(zhe_rid_t rid, const void *payload, zhe_paysize_t size, void *arg)
{
    const zhe_time_t tnow = zhe_platform_time();
    const struct pong * const d = payload;
    assert(size == sizeof(*d));
    if (d->src > MAX_KEY) {
        printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] pong_handler: pong from %"PRIu32" out of range\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, d->src);
        abort();
    }
    if (d->dst != ownkey) {
        /* don't care about pongs going to others */
        return;
    }
    if (d->seq == peerinfo[d->src].seq) {
        if (!send_next_ping(d->src, tnow)) {
            printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] pong_handler: write pong failed\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey);
            abort();
        }
    } else {
        printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] pong_handler: pong from %"PRIu32" out of sequence\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, d->src);
        abort();
    }
}

int main(int argc, char * const *argv)
{
    unsigned char ownid[16];
    zhe_paysize_t ownidsize;
    int opt;
    struct zhe_config cfg;
    int drop_pct = 0;
    zhe_time_t duration = (zhe_time_t)~0;

#ifdef __APPLE__
    srandomdev();
#else
    srandom(time(NULL) + getpid());
#endif
    ownidsize = getrandomid(ownid, sizeof(ownid));
#if ENABLE_TRACING
    zhe_trace_cats = ZTCAT_PEERDISC;// | ZTCAT_PUBSUB;
#endif

    while((opt = getopt(argc, argv, "h:X:D:")) != EOF) {
        switch(opt) {
            case 'h': ownidsize = getidfromarg(ownid, sizeof(ownid), optarg); break;
            case 'X': drop_pct = atoi(optarg); break;
            case 'D': duration = (zhe_time_t)atoi(optarg); break;
            default: fprintf(stderr, "invalid options given\n"); exit(1); break;
        }
    }
    if (optind + 1 != argc) {
        fprintf(stderr, "usage: psrid [options] id\n");
        exit(1);
    }
    ownkey = atoi(argv[optind]);
    if (ownkey > MAX_KEY) {
        fprintf(stderr, "key %"PRIu32" out of range\n", ownkey);
        exit(1);
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.id = ownid;
    cfg.idlen = ownidsize;
    struct zhe_platform * const platform = zhe_platform_new(7447, drop_pct);
    cfg_handle_addrs(&cfg, platform, "239.255.0.1", "", "");
    if (zhe_init(&cfg, platform, zhe_platform_time()) < 0) {
        fprintf(stderr, "init failed\n");
        exit(1);
    }
    zhe_time_t tstart = zhe_platform_time();
    zhe_time_t tprint = tstart;
    for (uint32_t k = 0; k <= MAX_KEY; k++) {
        peerinfo[k].tping = tstart - 1000;
    }
    zhe_start(tstart);
    pub_ping = zhe_publish(rid_ping, 0, true);
    pub_pong = zhe_publish(rid_pong, 0, true);
    sub_ping = zhe_subscribe(rid_ping, 12 + sizeof(struct pong), 0, ping_handler, NULL);
    sub_pong = zhe_subscribe(rid_pong, 12 + sizeof(struct ping), 0, pong_handler, NULL);
    for (zhe_time_t tnow = tstart; ZTIME_TO_SECu32(tnow - tstart) <= duration; tnow = zhe_platform_time()) {
        char inbuf[TRANSPORT_MTU];
        zhe_address_t insrc;
        int recvret;
        for (uint32_t k = 0; k <= MAX_KEY; k++) {
            if (peerinfo[k].lastseq_init && ZTIME_TO_SECu32(tnow - peerinfo[k].tping) > 3) {
                printf("%4"PRIu32".%03"PRIu32" *** [%"PRIu32"] no response after 3s from %"PRIu32" (%"PRIu32".%03"PRIu32")\n", ZTIME_TO_SECu32(tnow), ZTIME_TO_MSECu32(tnow), ownkey, k, ZTIME_TO_SECu32(peerinfo[k].tping), ZTIME_TO_MSECu32(peerinfo[k].tping));
                abort();
            }
            if (ZTIME_TO_SECu32(tnow - peerinfo[k].tping) > 1) {
                (void)send_next_ping(k, tnow);
            }
        }
        if (ZTIME_TO_SECu32(tnow - tprint) > 1) {
            print_state(tnow);
            tprint = tnow;
        }
        zhe_housekeeping(tnow);
        while ((recvret = zhe_platform_recv(platform, inbuf, sizeof(inbuf), &insrc)) > 0) {
            zhe_input(inbuf, (size_t)recvret, &insrc, tnow);
        }
        zhe_platform_wait(platform, 10);
    }
    print_state(zhe_platform_time());
    return 0;
}