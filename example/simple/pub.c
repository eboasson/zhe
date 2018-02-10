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

#include "zhe-config-deriv.h" /* for N_OUT_CONDUITS, ZTIME_TO_SECu32 */
#include "zhe-util.h"

uint16_t port = 7447;
const char *scoutaddrstr = "239.255.0.1";

void init_rnd() {
#ifdef __APPLE__
    srandomdev();
#else
    srandom(time(NULL) + getpid());
#endif

}
int main(int argc, char* argv[]) {
    unsigned char ownid[16];
    zhe_paysize_t ownidsize;
    struct zhe_config cfg;

    unsigned cid = 0;

    init_rnd();

#if N_OUT_MCONDUITS == 0
    char *mcgroups_join_str = "";
    char *mconduit_dstaddrs_str = "";
#elif N_OUT_MCONDUITS == 1
    char *mcgroups_join_str = "239.255.0.2"; /* in addition to scout */
    char *mconduit_dstaddrs_str = "239.255.0.2";
#elif N_OUT_MCONDUITS == 2
    char *mcgroups_join_str = "239.255.0.2,239.255.0.3"; /* in addition to scout */
    char *mconduit_dstaddrs_str = "239.255.0.2,239.255.0.3";
#elif N_OUT_MCONDUITS == 3
    char *mcgroups_join_str = "239.255.0.2,239.255.0.3,239.255.0.4"; /* in addition to scout */
    char *mconduit_dstaddrs_str = "239.255.0.2,239.255.0.3";
#endif

#if ENABLE_TRACING
    zhe_trace_cats = ZTCAT_PEERDISC | ZTCAT_PUBSUB;
#endif

    ownidsize = getrandomid(ownid, sizeof(ownid));

    memset(&cfg, 0, sizeof(cfg));
    cfg.id = ownid;
    cfg.idlen = ownidsize;

    struct zhe_platform * const platform = zhe_platform_new(port, 0);

    cfg_handle_addrs(&cfg, platform, scoutaddrstr, mcgroups_join_str, mconduit_dstaddrs_str);

    if (zhe_init(&cfg, platform, zhe_platform_time()) < 0) {
        fprintf(stderr, "init failed\n");
        exit(1);
    }
    zhe_start(zhe_platform_time());
    zhe_pubidx_t p;
    p = zhe_publish(1, cid, 1);
    uint64_t  delay = 1000000000;
    zhe_once(platform, delay);

    uint64_t count = 0;

    while (true) {
        printf(">> Writing count %llu\n", count);
        zhe_write(p, &count, 100, zhe_platform_time());
        zhe_flush();
        count += 1;
        zhe_once(platform, delay);
//        sleep(1);
    }
}