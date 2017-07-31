#ifndef ZENO_INT_H
#define ZENO_INT_H

#include "zeno-config-int.h"

struct out_conduit;
struct in_conduit;

void xrce_panic(uint16_t line, uint16_t code);
uint8_t popcnt8(uint8_t x);
void bitset_set(uint8_t *s, uint8_t idx);
void bitset_clear(uint8_t *s, uint8_t idx);
int bitset_test(const uint8_t *s, uint8_t idx);
uint8_t bitset_count(const uint8_t *s, uint8_t size);
int bitset_findfirst(const uint8_t *s, uint8_t size);
void oc_setup(void);
void ic_setup(void);
uint16_t xmitw_pos_add(uint16_t p, uint16_t a);
uint16_t xmitw_bytesused(const struct out_conduit *c);
void msend(void);
void pack_reserve(zeno_address_t *dst, zpsize_t cnt);
void pack1(uint8_t x);
void pack2(uint8_t x, uint8_t y);
void pack_u16(uint16_t x);
void pack_vec(zpsize_t n, const uint8_t *buf);
zmsize_t oc_pack_payload_msgprep(seq_t *s, struct out_conduit *c, int relflag, zpsize_t sz);
void oc_pack_copyrel(struct out_conduit *c, zmsize_t from);
void oc_pack_payload(struct out_conduit *c, int relflag, zpsize_t sz, const void *vdata);
void oc_pack_payload_done(struct out_conduit *c, int relflag);
void rsub_register(rid_t rid, uint8_t submode);
uint8_t rsub_precommit(rid_t *err_rid);
void rsub_commit(void);
int rsub_exists(uint8_t pubidx);
void rsub_precommit_curpkt_abort(void);
void rsub_precommit_curpkt_done(void);
void rsub_clear(void);
void xrce_setup(void);
int read_serial(void);
int unpack_skip(zmsize_t *sz, const uint8_t **data, zmsize_t n);
int unpack_byte(zmsize_t *sz, const uint8_t **data, zmsize_t *u);
int unpack_u16(zmsize_t *sz, const uint8_t **data, uint16_t *u);
int unpack_vle16(zmsize_t *sz, const uint8_t **data, uint16_t *u);
int unpack_vle32(zmsize_t *sz, const uint8_t **data, uint32_t *u);
int unpack_vle64(zmsize_t *sz, const uint8_t **data, uint64_t *u);
int unpack_seq(zmsize_t *sz, const uint8_t **data, seq_t *u);
int unpack_rid(zmsize_t *sz, const uint8_t **data, rid_t *u);
int unpack_vec(zmsize_t *sz, const uint8_t **data, size_t lim, zpsize_t *u, uint8_t *v);
int unpack_locs(zmsize_t *sz, const uint8_t **data);
int unpack_props(zmsize_t *sz, const uint8_t **data);
zmsize_t handle_mscout(zmsize_t sz, const uint8_t *data);
zmsize_t handle_mhello(zmsize_t sz, const uint8_t *data);
zmsize_t handle_mopen(zmsize_t sz, const uint8_t *data);
zmsize_t handle_maccept(zmsize_t sz, const uint8_t *data);
void close_connection_and_scout(void);
zmsize_t handle_mclose(zmsize_t sz, const uint8_t *data);
zmsize_t handle_dresource(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dpub(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dsub(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dselection(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dbindid(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dcommit(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_dresult(zmsize_t sz, const uint8_t *data, int interpret);
zmsize_t handle_ddeleteres(zmsize_t sz, const uint8_t *data, int interpret);
int seq_lt(seq_t a, seq_t b);
int seq_le(seq_t a, seq_t b);
int ic_may_deliver_seq(struct in_conduit *ic, uint8_t hdr, seq_t seq);
void ic_update_seq (struct in_conduit *ic, uint8_t hdr, seq_t seq);
void acknack_if_needed(uint8_t cid, int wantsack);
zmsize_t handle_mdeclare(zmsize_t sz, const uint8_t *data, uint8_t cid);
zmsize_t handle_msynch(zmsize_t sz, const uint8_t *data, uint8_t cid);
zmsize_t handle_msdata(zmsize_t sz, const uint8_t *data, uint8_t cid);
void remove_acked_messages(struct out_conduit *c, seq_t seq);
zmsize_t handle_macknack(zmsize_t sz, const uint8_t *data, uint8_t cid);
zmsize_t handle_mping(zmsize_t sz, const uint8_t *data);
zmsize_t handle_mpong(zmsize_t sz, const uint8_t *data);
zmsize_t handle_mkeepalive(zmsize_t sz, const uint8_t *data);
zmsize_t handle_mconduit(zmsize_t sz, const uint8_t *data, uint8_t *cid);
void reset_pubs_to_declare(void);
void reset_subs_to_declare(void);
pubidx_t publish(rid_t rid, int reliable);
subidx_t subscribe(rid_t rid, zpsize_t xmitneed, subhandler_t handler, void *arg);
int zeno_write(pubidx_t pubidx, zpsize_t sz, const void *data);
void flush_output(ztime_t tnow);
void send_msynch(ztime_t tnow);
void send_declares(ztime_t tnow);
void xrce_loop_init(void);
void xrce_loop(void);

#endif
