/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2017 Beshr Al Nahas and Olaf Landsiedel.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/
/**
 * \file
 *         A2-Synchrotron Multi Writer application.
 * \author
 *          
 *         Konstantinos Peratinos <konper@student.chalmers.se>
 *
 */

#include "contiki.h"
#include <stdio.h> /* For printf() */
#include "net/netstack.h"

#include "chaos-control.h"
#include "node.h"
#include "mw-quorum.h"

static uint16_t value = 0;
static uint16_t tag = 0;
static uint8_t operation = 1;
static uint16_t round_count_local = 0;
static uint8_t* flags;
static uint16_t complete = 0;
static uint16_t off_slot;

static void round_begin(const uint16_t round_count, const uint8_t id);

CHAOS_APP(chaos_quorum, MAX_SLOT_LEN, MAX_ROUND_MAX_SLOTS, 1, q_is_pending, round_begin);
#if NETSTACK_CONF_WITH_CHAOS_NODE_DYNAMIC
#include "join.h"
CHAOS_APPS(&join, &chaos_quorum);
#else
CHAOS_APPS(&chaos_quorum);
#endif /* NETSTACK_CONF_WITH_CHAOS_NODE_DYNAMIC */

PROCESS(chaos_quorum_process, "Chaos Quorum Process");
AUTOSTART_PROCESSES(&chaos_quorum_process);

PROCESS_THREAD(chaos_quorum_process, evda, data)
{
	PROCESS_BEGIN();
  printf("{boot} Single Writer Quorum Application\n");

  NETSTACK_MAC.on();
#if FAULTY_NODE_ID
  memset(rx_pkt_crc_err, 0, sizeof(rx_pkt_crc_err));
#endif
	while( 1 ){
		PROCESS_YIELD();
		if(chaos_has_node_index){
      //if (round_count_local % NODE_COUNT m == chaos_node_index) {}
        if (IS_INITIATOR()) {
          printf("{rd %u res} written: %u, ts: %u, fin: %i/%i, node id: %u, n: %u\n", round_count_local, value, tag, complete, off_slot, node_id, chaos_node_count);
        }
        else { printf("{rd %u res} read: %u, ts: %u, fin: %i/%i, node id: %u, n: %u\n", round_count_local, value, tag, complete, off_slot, node_id, chaos_node_count); }
      
//      int latency = complete *
//      printf("{rd %u prf} latency = %f, total slot time = %f\n", complete, off_slot);
      if(complete == 0){
        int i;
        printf("{rd %u fl}", round_count_local);
        for( i=0; i<quorum_get_flags_length(); i++ ){
          printf(" %02x", flags[i]);
        }
        printf("\n");
      }

#if FAULTY_NODE_ID
      //print the faulty CRC but passing packet (discovered by MIC check)
      if(rx_pkt_crc_err[sizeof(rx_pkt_crc_err)-1]){
        int i;
        printf("{rd %u crc}", round_count_local);
        for( i=0; i<sizeof(rx_pkt_crc_err); i++ ){
          printf(" %02x", rx_pkt_crc_err[i]);
        }
        printf("\n");
        memset(rx_pkt_crc_err, 0, sizeof(rx_pkt_crc_err));
      }
#endif
		} else {
      printf("{rd %u res} Quorum: waiting to join, n: %u\n", round_count_local, chaos_node_count);
		}
	}
	PROCESS_END();
}

static void round_begin(const uint16_t round_count, const uint8_t id){
  if(IS_INITIATOR()) {
    operation = 0;
    tag = round_count_local+1;
    value = (chaos_node_count)*(round_count_local) % 13;
    printf("Init writing :%u tag is :%u", value, tag);
  } /*else {  
    operation = 1;
    tag = 0;
    value = 0;
  }*/
  complete = quorum_round_begin(round_count, id, &value, &tag, operation, &flags);
  off_slot = quorum_get_off_slot();
  round_count_local = round_count;
  process_poll(&chaos_quorum_process);
}




/* Perform Write every 3 rounds
  if (round_count % 3 == 0 && IS_INITIATOR()) {
    tx_entry.entry = entry_local.entry*2+1;
    //Single writer, we do not need to perform a read
    tx_entry.tag = ++timestamp;
    next_state = CHAOS_TX;
  } */