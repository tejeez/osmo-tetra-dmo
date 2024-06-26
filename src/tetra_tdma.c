/* TETRA TDMA time functions, see Section 7.3 of EN 300 392-2 */

/* (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include "tetra_tdma.h"

static void normalize_mn(struct tetra_tdma_time *tm)
{
	if (tm->mn > 60)
		tm->mn = (tm->mn%60);
}

static void normalize_fn(struct tetra_tdma_time *tm)
{
	uint32_t mn_delta;

	if (tm->fn > 18) {
		mn_delta = tm->fn/18;
		tm->fn = (tm->fn%18);
		tm->mn += mn_delta;
	}
	normalize_mn(tm);
}

static void normalize_tn(struct tetra_tdma_time *tm)
{
	uint32_t fn_delta;

	if (tm->tn > 4) {
		fn_delta = tm->tn/4;
		tm->tn = (tm->tn%4);
		tm->fn += fn_delta;
	}
	normalize_fn(tm);
}

static void normalize_sn(struct tetra_tdma_time *tm)
{
	uint32_t tn_delta;

	if (tm->sn > 255) {
		tn_delta = (tm->sn/255);
		tm->sn = (tm->sn % 255) + 1;
		tm->tn += tn_delta;
	}
	normalize_tn(tm);
}

void tetra_tdma_time_add_sym(struct tetra_tdma_time *tm, uint32_t sym_count)
{
	tm->sn += sym_count;
	normalize_sn(tm);
}

void tetra_tdma_time_add_tn(struct tetra_tdma_time *tm, uint32_t tn_count)
{
	tm->tn += tn_count;
	normalize_tn(tm);
}

void tetra_tdma_time_add_fn(struct tetra_tdma_time *tm, uint32_t fn_count)
{
	tm->fn += fn_count;
	normalize_fn(tm);
}

char *tetra_tdma_time_dump(const struct tetra_tdma_time *tm)
{
	static char buf[256];

	snprintf(buf, sizeof(buf), "%02u/%02u/%u/%03u", tm->mn, tm->fn, tm->tn, tm->sn);

	return buf;
}

uint32_t tetra_tdma_time2fn(struct tetra_tdma_time *tm)
{
	return (((tm->hn * 60) + tm->mn) * 18) + tm->fn;
}

void tetra_tdma_time_add_burst_delta(struct tetra_tdma_time *tm, struct tetra_rx_state *trs)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	uint64_t host_ts = now.tv_sec*1000000+now.tv_usec;
	
	if (trs->modem_prev_burst_rx_timestamp > 0) {
		uint32_t tn_ns = 14166667; // 1 timeslot = 510 modulation bits = 510 * 250/9us = ~14166667ns = ~14,167ms 

		uint64_t curr = trs->modem_burst_rx_timestamp;
		uint64_t prev = trs->modem_prev_burst_rx_timestamp;
		int64_t delta_ns = curr - prev;
		float delta_ts = (float)delta_ns / tn_ns;
		uint32_t delta_ts_int = round(delta_ts);

		printf("\n#### MODEM SYNC BURST CURR %ld, PREV %ld -> DELTA %ld (+%d TN)", curr, prev, delta_ns, delta_ts_int);
		printf("\n ### HOST CURR %ld\n", host_ts );
		trs->modem_prev_burst_rx_timestamp = trs->modem_burst_rx_timestamp;
		tetra_tdma_time_add_tn(tm, delta_ts_int);
		trs->host_burst_rx_timestamp = host_ts;

	} else {
		trs->modem_prev_burst_rx_timestamp = trs->modem_burst_rx_timestamp;
		trs->host_burst_rx_timestamp = host_ts;

	}
}
