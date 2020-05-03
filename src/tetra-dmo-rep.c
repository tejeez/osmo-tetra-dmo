/*
* This is TETRA Type 1A DM-REP implementation.
*
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/bitvec.h>

#include <lower_mac/crc_simple.h>
#include <lower_mac/tetra_conv_enc.h>
#include <lower_mac/tetra_interleave.h>
#include <lower_mac/tetra_scramb.h>
#include <lower_mac/tetra_rm3014.h>
#include <lower_mac/viterbi.h>

#include "tetra_common.h"
#include <phy/tetra_burst.h>
#include <phy/tetra_burst_sync.h>

#include <zmq.h>
#include "suo.h"
#include "pdus.h"

#include <signal.h>

#include "hamtetra_config.h"

void *tetra_tall_ctx;
//struct tetra_mac_state *tms; // Moved to struct slotter_state, is that a good idea or not?
struct tetra_rx_state *trs;
// struct tetra_phy_state t_phy_state;

#define ENCODED_MAXLEN 0x900
void *zmq_rx_socket;
void *zmq_tx_socket;

#define swap16(x) ((x)<<8)|((x)>>8)

/*
void reprime_timeslot_slave(uint8_t slotnum) {
    uint64_t multiframe_duration = 3060000/3;
    struct timeslot *tn = &frame_buf_slave.tn[slotnum];
    tn->start_ts += multiframe_duration;
    tn->len = 0;

};
*/

void resync_timeslots(uint8_t fn, uint8_t tn) {
    printf("TODO: make resync_timeslots call timing_resync or something\n");
#if 0
    uint32_t duration_tn = 42500/3; // timeslot duration in microseconds
    uint64_t host = trs->host_burst_rx_timestamp; // end of burst in microseconds
    host -= duration_tn;
    uint64_t modem = trs->modem_burst_rx_timestamp; // end of burst in nanoseconds
    modem -= (duration_tn*1000);

    uint8_t master_tn = (fn*4+tn)-5;
    // uint8_t slave_tn = (master_tn-3)%72;

    printf("## Resync tx-buffer TS's from M%02d/%d to host-ts: %ld and modem-ts %ld\n", fn, tn, host, modem);

    for(int i=0; i<72; i++) {
        master_tn = (master_tn + i)%72;
        // slave_tn = (slave_tn + i)%72;
        struct timeslot *master_slot = &frame_buf_master.tn[master_tn];
        // struct timeslot *slave_slot = &frame_buf_master.tn[slave_tn];
        master_slot->start_ts = host;
        master_slot->start_ts_modem = modem;
        // slave_slot->start_ts = host;
        // slave_slot->start_ts_modem = modem;

        host += duration_tn;
        modem += (duration_tn*1000);
    }
#endif
}


// not used anymore
#if 0
// common function with tetra-rx-dmo so should be moved to common library
int floats_to_bits(const struct frame *in, struct frame *out, size_t maxlen) 
{
	out->m = in->m; // Copy metadata
	size_t len = in->m.len;
	if (len > maxlen) len = maxlen;
	out->m.len = len;

	uint8_t buffy_in[len], buffy_out[len];
	memcpy(buffy_in, in->data, len);

	for (int i = 0; i < len; ++i) {
		int sym = buffy_in[i];
		if (sym < 128) {
			buffy_out[i] = 0;
		} else {
			buffy_out[i] = 1;
		}
	}

	// memcpy(out->data, buffy_out+2, len-2);
	// out->m.len = len-2;
	memcpy(out->data, buffy_out, len);
	return 0;
}

int bits_to_floats(const struct frame *in, struct frame *out, size_t maxlen) 
{
	out->m = in->m; // Copy metadata
	size_t len = in->m.len;
	if (len > maxlen) len = maxlen;
	out->m.len = len;

	uint8_t buffy_in[len], buffy_out[len];
	memcpy(buffy_in, in->data, len);

	for (int i = 0; i < len; ++i) {
		int sym = buffy_in[i];
		if (sym < 1) {
			buffy_out[i] = 0;
		} else {
			buffy_out[i] = 0xff;
		}
	}

	memcpy(out->data, buffy_out, len);
	out->m.len = len;
	return 0;
}
#endif



// TODO fix everything
#if 0
// rx thread
void *rep_rx_thread()
{
    struct timeval now;
    // setup ZMQ RX
	void *zmq_context = zmq_ctx_new();
    zmq_rx_socket = zmq_socket(zmq_context, ZMQ_SUB);
    //int connret = zmq_connect(zmq_rx_socket, "tcp://localhost:43300");
    int connret = zmq_connect(zmq_rx_socket, "ipc:///tmp/dpsk-modem-rx");
	zmq_setsockopt(zmq_rx_socket, ZMQ_SUBSCRIBE, "", 0);

	while (1) {
        gettimeofday(&now, NULL);
        uint64_t now_us = (double)now.tv_sec*1.0e6+(double)now.tv_usec;

		int nread;
		zmq_msg_t input_msg;
		zmq_msg_init(&input_msg);
		nread = zmq_msg_recv(&input_msg, zmq_rx_socket, ZMQ_DONTWAIT);
		
		if (nread >= 0) {
			char encoded_buf[sizeof(struct frame) + ENCODED_MAXLEN];
			struct frame *encoded = (struct frame *)encoded_buf;
			int rc = floats_to_bits(zmq_msg_data(&input_msg), encoded, ENCODED_MAXLEN);
            trs->modem_burst_rx_timestamp = encoded->m.time;
            trs->host_burst_rx_timestamp = now_us;
            // printf("RX burst: %s\n", osmo_ubit_dump(encoded->data, encoded->m.len));
            tetra_burst_sync_in(trs, encoded->data, encoded->m.len);

		} else {
            // no burst in buffer
            if (tms->channel_state == DM_CHANNEL_S_MS_IDLE_UNKNOWN) {
                if (now_us > (tms->channel_state_last_chg+2.04e6)) { // 2 multiframes
                    printf("[RX-%ld] DMO channel state change: %d -> %d \n",now_us, tms->channel_state, DM_CHANNEL_S_MS_IDLE_FREE);
                    tms->channel_state = DM_CHANNEL_S_MS_IDLE_FREE;
                    tms->channel_state_last_chg = now_us;
                }
                
            }
        }
		zmq_msg_close(&input_msg);
	}

	zmq_ctx_destroy(zmq_context);



};

// tx thread
void rep_tx_thread() 
{
    unsigned int multiframe_counter = 0;
    uint8_t multiframes_since_last_presence = 0;
    unsigned int pointer_tn = 3;
    unsigned int pointer_slave_tn = 0;
    unsigned int counter = 0;
   	struct timeval now;

    struct tetra_tdma_time *phy_time = &t_phy_state.time;

   	void *zmq_context = zmq_ctx_new();
    zmq_tx_socket = zmq_socket(zmq_context, ZMQ_PUB);
    // int connret = zmq_connect(zmq_tx_socket, "tcp://localhost:43301");
    int connret = zmq_connect(zmq_tx_socket, "ipc:///tmp/dpsk-modem-tx");


    gettimeofday(&now, NULL);
    uint64_t now_us = (double)now.tv_sec*1.0e6+(double)now.tv_usec;
    uint64_t one_sec = now_us + 1000000;

    initialize_framebuffer(one_sec);

    struct timeslot current_ts = frame_buf_master.tn[pointer_tn];
    struct timeslot next_ts = frame_buf_master.tn[pointer_tn+1];
    // struct timeslot current_slave_ts = frame_buf_slave.tn[pointer_slave_tn];
    // struct timeslot next_slave_ts = frame_buf_slave.tn[pointer_tn+1];

    while (1) { 
        gettimeofday(&now, NULL);
        now_us = (double)now.tv_sec*1.0e6+(double)now.tv_usec;

        // just in case time has been resyncronized while in the loop
        pointer_tn = 4*(t_phy_state.time.fn-1) + (t_phy_state.time.tn-1);
        uint8_t pointer_next_tn = (pointer_tn+1)%72;
        next_ts = frame_buf_master.tn[pointer_next_tn];

        if (now_us > next_ts.start_ts){
            tetra_tdma_time_add_tn(phy_time, 1);
            current_ts = next_ts;
            pointer_tn = 4*(t_phy_state.time.fn-1) + (t_phy_state.time.tn-1);
            uint8_t pointer_next_tn = (pointer_tn+1)%72;
            // current_slave_ts = next_slave_ts;
            
            // if (current_ts.len > 0) {
                printf("\n[TX-%ld] [%d - %02d/%d] MASTER %02d/%02d/%d - SLAVE %02d/%02d/%d - start ts: %ld / %ld, delayed: %d, len: %d", 
                    now_us, tms->channel_state, phy_time->fn, phy_time->tn,
                    multiframe_counter, current_ts.fn, current_ts.tn, multiframe_counter, current_ts.slave_fn, current_ts.slave_tn,
                    current_ts.start_ts, current_ts.start_ts_modem, current_ts.delayed, current_ts.len);
            // }
            if (current_ts.len > 0 && current_ts.delayed == 0) {

                printf(", burst: %s ", osmo_ubit_dump(current_ts.burst, current_ts.len));
                // send burst to modem
                char encoded_buf[sizeof(struct frame) + ENCODED_MAXLEN];
			    struct frame *encoded = (struct frame *)encoded_buf;

                encoded->m.flags = 0;
                encoded->m.time = 0;
                encoded->m.len = current_ts.len;
                memcpy(encoded->data,current_ts.burst,current_ts.len);

           		zmq_msg_t input_msg;
    		    zmq_msg_init_size(&input_msg, sizeof(struct frame)+encoded->m.len);
                int rc = bits_to_floats(encoded, zmq_msg_data(&input_msg), ENCODED_MAXLEN);
                // rc = zmq_send(zmq_tx_socket, encoded, sizeof(struct frame)+encoded->m.len, ZMQ_DONTWAIT);
                rc = zmq_msg_send(&input_msg, zmq_tx_socket, ZMQ_DONTWAIT);
                // printf("ZMQSEND: %d", rc);

            }
            reprime_timeslot(pointer_tn);

            counter++;
            // pointer_tn = counter%72;
            pointer_tn = 4*(t_phy_state.time.fn-1) + (t_phy_state.time.tn-1);
            pointer_next_tn = (pointer_tn+1)%72;
            // pointer_slave_tn = (pointer_tn-2)%72;
            next_ts = frame_buf_master.tn[pointer_next_tn];
            // next_slave_ts = frame_buf_slave.tn[pointer_slave_tn];

            // when rolling over to next multiframe IN SLAVE
            if (next_ts.fn < current_ts.fn) {
                multiframe_counter++;
                if (tms->channel_state == DM_CHANNEL_S_MS_IDLE_FREE) {
                    if (++multiframes_since_last_presence >= presence_signal_multiframe_count[DT254]) {
                        // printf("presence signal away!\n");
                        multiframes_since_last_presence = 0;

                        uint8_t send_count = DN253*4;

                        for (int i = send_count; i>0; i--) { // repeat for N frames
                            struct timeslot *burst_slot;
                            uint8_t slot_num = (75-i) % 72;
                            uint8_t countdown = (send_count-1) / 4;

                            burst_slot = &frame_buf_master.tn[slot_num];
                            build_pdu_dpress_sync(burst_slot->fn, burst_slot->tn, countdown, burst_slot->burst);
                            burst_slot->len = 510;

                            if (i<4) {
                                burst_slot->delayed=1;
                            }


                        }
                    }

                }
            }

        }
    }

};
#endif

void catch_alarm(int sig) {
    struct tetra_tdma_time *phy_time = &t_phy_state.time;
    printf("  ## timer alarm! MAC PHY time: %02d/%d ##\n", phy_time->fn, phy_time->tn);
    signal(sig, catch_alarm);
}


// Remnants from old main
void init_more_tetra_dmo_rep_stuff(struct tetra_mac_state *tms)
{
	t_phy_state.time.fn=1;
	t_phy_state.time.tn=1;
	t_phy_state.time_adjust_cb_func = resync_timeslots;

	// initialize fragmentation slots
	memset((void *)&fragslots,0,sizeof(struct fragslot)*FRAGSLOT_NR_SLOTS);
	char desc[]="slot \0";
	for (int k=0;k<FRAGSLOT_NR_SLOTS;k++) {
		desc[4]='0'+k;
		fragslots[k].msgb=msgb_alloc(8192, desc);
		msgb_reset(fragslots[k].msgb);
	}

	/*gettimeofday(&now, NULL);
	uint64_t now_us = (double)now.tv_sec*1.0e6+(double)now.tv_usec;
	tms->channel_state_last_chg=now_us;*/
	// TODO look at timing again and redo the thing above

	tms->channel_state=DM_CHANNEL_S_DMREP_IDLE_UNKNOWN;
}


/* main function temporarily commented out to be able to link
 * this file into hamtetra_main */
#if 0
int main(int argc, char **argv)
{
    pthread_t thread_rx_handle;
   	struct timeval now;

    // initializations
	tms = talloc_zero(tetra_tall_ctx, struct tetra_mac_state);
	tetra_mac_state_init(tms);
	tms->infra_mode = TETRA_INFRA_DMO; 

	trs = talloc_zero(tetra_tall_ctx, struct tetra_rx_state);
	trs->burst_cb_priv = tms;

    t_phy_state.time.fn=1;
    t_phy_state.time.tn=1;
    t_phy_state.time_adjust_cb_func = resync_timeslots;

   	// initialize fragmentation slots
	memset((void *)&fragslots,0,sizeof(struct fragslot)*FRAGSLOT_NR_SLOTS); 
	char desc[]="slot \0";
	for (int k=0;k<FRAGSLOT_NR_SLOTS;k++) {
		desc[4]='0'+k;
		fragslots[k].msgb=msgb_alloc(8192, desc);
		msgb_reset(fragslots[k].msgb);
	}

    gettimeofday(&now, NULL);
    uint64_t now_us = (double)now.tv_sec*1.0e6+(double)now.tv_usec;
    tms->channel_state_last_chg=now_us;
    tms->channel_state=DM_CHANNEL_S_MS_IDLE_UNKNOWN;

    signal(SIGALRM, catch_alarm);
    struct itimerval timer_new;
    timer_new.it_interval.tv_usec = 42500/3;
    timer_new.it_interval.tv_sec = 0;
    timer_new.it_value.tv_sec = 1;
    timer_new.it_value.tv_usec = 0;

    setitimer(ITIMER_REAL, &timer_new, NULL);


    printf("## TETRA Type 1A DM-REP ##\n");

    // state machine
    pthread_create(&thread_rx_handle, NULL, rep_rx_thread, NULL);

    rep_tx_thread();

	talloc_free(tms);

    exit (0);
}
#endif
