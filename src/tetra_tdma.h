#ifndef TETRA_TDMA_H
#define TETRA_TDMA_H

#include <stdint.h>
#include <math.h>
#include <phy/tetra_burst_sync.h>

enum tdma_master_slave_link_flag {
	DM_LINK_SLAVE,
	DM_LINK_MASTER
};

struct tetra_tdma_time {
	uint16_t hn;    /* hyperframe number (1 ... 65535) */
	uint32_t sn;	/* symbol number (1 ... 255) */
	uint32_t tn;	/* timeslot number (1 .. 4) */
	uint32_t fn;	/* frame number (1 .. 18) */
	uint32_t mn;	/* multiframe number (1 .. 60) */
	enum tdma_master_slave_link_flag link;	/* master/slave link (0..1) */
};

void tetra_tdma_time_add_sym(struct tetra_tdma_time *tm, uint32_t sym_count);
void tetra_tdma_time_add_tn(struct tetra_tdma_time *tm, uint32_t tn_count);
void tetra_tdma_time_add_fn(struct tetra_tdma_time *tm, uint32_t fn_count);
char *tetra_tdma_time_dump(const struct tetra_tdma_time *tm);

uint32_t tetra_tdma_time2fn(struct tetra_tdma_time *tm);
void tetra_tdma_time_add_burst_delta(struct tetra_tdma_time *tm, struct tetra_rx_state *trs);

#endif
