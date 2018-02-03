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
/*
 * \file
 *         Multi-writer Quorum library
 * \author
 *         Konstantinos Peratinos <konper@student.chalmers.se>
 */

#include "contiki.h"
#include <string.h>

#include "chaos.h"
#include "chaos-random-generator.h"
#include "node.h"
#include "minmax.h"
#include "chaos-config.h"

#undef ENABLE_COOJA_DEBUG
#define ENABLE_COOJA_DEBUG COOJA
#include "dev/cooja-debug.h"

#ifndef COMMIT_THRESHOLD
#define COMMIT_THRESHOLD 6
#endif

#ifndef N_TX_COMPLETE
#define N_TX_COMPLETE 9
#endif

#ifndef CHAOS_RESTART_MIN
#define CHAOS_RESTART_MIN 6
#endif

#ifndef CHAOS_RESTART_MAX
#define CHAOS_RESTART_MAX 10
#endif

#define LIMIT_TX_NO_DELTA 0

#define FLAGS_LEN_X(X)   (((X) >> 3) + (((X) & 7) ? 1 : 0))
#define FLAGS_LEN   (FLAGS_LEN_X(chaos_node_count))
//#define LAST_FLAGS  ((1 << (((chaos_node_count - 1) % 8) + 1)) - 1)
#define LAST_FLAGS  ((1 << (((chaos_node_count - 1) & 7) + 1)) - 1)
//#define FLAG_SUM    (((FLAGS_LEN - 1) * 0xFF) + LAST_FLAGS)
#define FLAG_SUM    (((FLAGS_LEN - 1) << 8) - (FLAGS_LEN - 1) + LAST_FLAGS)

#if NETSTACK_CONF_WITH_CHAOS_NODE_DYNAMIC
#define FLAGS_ESTIMATE FLAGS_LEN_X(MAX_NODE_COUNT)
#warning "APP: due to packet size limitation: maximum network size = MAX_NODE_COUNT"
#else
#define FLAGS_ESTIMATE FLAGS_LEN_X(CHAOS_NODES)
#endif

//#if (CHAOS_APP_PAYLOAD_LEN-FLAGS_ESTIMATE-4) > 0
//#define APP_DUMMY_LEN MIN(CHAOS_APP_PAYLOAD_LEN-FLAGS_ESTIMATE-sizeof(uint32_t), CHAOS_MAX_PAYLOAD_LEN-FLAGS_ESTIMATE-sizeof(uint32_t))
//#else
//#define APP_DUMMY_LEN 0
//#endif

typedef struct __attribute__((packed)) max_t_struct {
  uint16_t value;
  uint16_t tag;
  uint16_t writer_id;
  uint8_t operation;   // 0 = Write,  1 =  Read
  uint8_t flags[];
} entry_t;

typedef struct __attribute__((packed)) max_t_local_struct {
  entry_t entry;
  uint8_t flags[FLAGS_ESTIMATE];
} entry_t_local;

static int tx = 0;
static int complete = 0;
//static uint16_t timestamp = 0;
static uint16_t completion_slot, off_slot;
static int tx_count_complete = 0;
static int invalid_rx_count = 0;
static int got_valid_rx = 0;
static unsigned short restart_threshold;
static entry_t_local entry_local; /* used only for house keeping and reporting */
static uint8_t* entry_flags;

static chaos_state_t
process(uint16_t round_count, uint16_t slot_count, chaos_state_t current_state, int chaos_txrx_success, size_t payload_length, uint8_t* rx_payload, uint8_t* tx_payload, uint8_t** app_flags)
{
  entry_t* tx_entry = (entry_t*)tx_payload;
  entry_t* rx_entry = (entry_t*)rx_payload;
  uint8_t rx_delta = 0;

   /* merge valid rx data & flags */
  if(chaos_txrx_success && current_state == CHAOS_RX) {
    got_valid_rx = 1;

    if (rx_entry->operation) { 
    // Read operation Received
        if (tx_entry->tag < rx_entry->tag) {
            tx_entry->value = rx_entry->value;
            tx_entry->tag = rx_entry->tag;
            tx_entry->writer_id = rx_entry->writer_id;
          }

             /*if (tx_entry->tag == rx_entry->tag && tx_entry->writer_id < rx_entry->writer_id) {
            tx_entry->value = rx_entry->value;
            tx_entry->writer_id = rx_entry->writer_id;
          } else if(tx_entry->tag < rx_entry->tag) {
          tx_entry->tag = rx_entry->tag;
          tx_entry->value = rx_entry->value;
          tx_entry->writer_id = rx_entry->writer_id;
        }*/

    } else { // Write Operation Receiven
        if ((tx_entry->tag == rx_entry->tag) && (tx_entry->writer_id < rx_entry->writer_id)) {
            tx_entry->value = rx_entry->value;
            tx_entry->tag = rx_entry->tag;
            tx_entry->writer_id = rx_entry->writer_id;
          } else if(tx_entry->tag < rx_entry->tag) {
          tx_entry->tag = rx_entry->tag;
          tx_entry->value = rx_entry->value;
          tx_entry->writer_id = rx_entry->writer_id;
        }
    }
    //merge flags and do tx decision based on flags
    tx = 0;
    uint16_t flag_sum = 0;
    int i;
    for( i = 0; i < FLAGS_LEN; i++){
      COOJA_DEBUG_STR("f");
      tx |= (rx_entry->flags[i] != tx_entry->flags[i]);
      tx_entry->flags[i] |= rx_entry->flags[i];
      flag_sum += tx_entry->flags[i];
    }
    rx_delta = tx;

    //all flags are set? Final flood: transmit result aggressively
    if( flag_sum >= (FLAG_SUM/2)+1 ){
      if(!complete){ //store when we reach completion
        completion_slot = slot_count;
      }
      tx = 1;
      complete = 1;
      LEDS_ON(LEDS_GREEN);
    }
  }

  /* decide next state */
  chaos_state_t next_state = CHAOS_RX;
  if( IS_INITIATOR() && current_state == CHAOS_INIT ){
    next_state = CHAOS_TX; //for the first tx of the initiator: no increase of tx_count here
    got_valid_rx = 1; //to enable retransmissions
  } else if(current_state == CHAOS_RX && chaos_txrx_success){
    invalid_rx_count = 0;
    if( tx ){
      /* if we have not received a delta, then we limit tx rate */
      next_state = CHAOS_TX;
      if( complete ){
        if(rx_delta){
          tx_count_complete = 0;
        } else {
          tx_count_complete++;
        }
      }
    }
  } else if(current_state == CHAOS_RX && !chaos_txrx_success && got_valid_rx){
    invalid_rx_count++;
    if(invalid_rx_count > restart_threshold){
      next_state = CHAOS_TX;
      invalid_rx_count = 0;
      if( complete ){
        tx_count_complete++;
      }
      restart_threshold = chaos_random_generator_fast() % (CHAOS_RESTART_MAX - CHAOS_RESTART_MIN) + CHAOS_RESTART_MIN;
    }
  } else if(current_state == CHAOS_TX && !chaos_txrx_success){ /* we missed tx go time. Retry */
    got_valid_rx = 1; //????? check me!
    next_state = CHAOS_TX;
  } else if(current_state == CHAOS_TX && tx_count_complete > N_TX_COMPLETE){
    next_state = CHAOS_OFF;
    LEDS_OFF(LEDS_GREEN);
  }

  /* for reporting the final result */
  if(complete || slot_count >= MAX_ROUND_MAX_SLOTS - 1) {
    entry_flags = tx_entry->flags;
    entry_local.entry.value = tx_entry->value;
    entry_local.entry.tag = tx_entry->tag;
    //entry_local.entry.writer_id = tx_entry->writer_id;
  }

  /* reporting progress */
  *app_flags = (current_state == CHAOS_TX) ? tx_entry->flags : rx_entry->flags;

  int end = (slot_count >= MAX_ROUND_MAX_SLOTS - 2) || (next_state == CHAOS_OFF);
  if(end){
    off_slot = slot_count;
  }
  return next_state;
}

int quorum_mw_get_flags_length() {
  return FLAGS_LEN;
}

int quorum_mw_is_pending(const uint16_t round_count){
  return 1;
}

uint16_t quorum_mw_get_off_slot(){
  return off_slot;
}

int quorum_mw_round_begin(const uint16_t round_number, const uint8_t app_id, uint16_t* value, uint16_t* tag, uint8_t operation, uint16_t* writer_id, uint8_t** final_flags)
{
  off_slot = MAX_ROUND_MAX_SLOTS;
  tx = 0;
  got_valid_rx = 0;
  complete = 0;
  completion_slot = 0;
  tx_count_complete = 0;
  invalid_rx_count = 0;

  /* init random restart threshold */
  restart_threshold = chaos_random_generator_fast() % (CHAOS_RESTART_MAX - CHAOS_RESTART_MIN) + CHAOS_RESTART_MIN;

  memset(&entry_local, 0, sizeof(entry_local));
  entry_local.entry.value = *value;
  entry_local.entry.tag = *tag;
  entry_local.entry.writer_id = 1;
  entry_local.entry.operation = operation;
  
   
  /* set my flag */
  unsigned int array_index = chaos_node_index / 8;
  unsigned int array_offset = chaos_node_index % 8;
  entry_local.entry.flags[array_index] |= 1 << (array_offset);

  chaos_round(round_number, app_id, (const uint8_t const*)&entry_local.entry, sizeof(entry_t) + quorum_mw_get_flags_length(), MAX_SLOT_LEN_DCO, MAX_ROUND_MAX_SLOTS, quorum_mw_get_flags_length(), process);

  memcpy(entry_local.entry.flags, entry_flags, quorum_mw_get_flags_length());
  *value = entry_local.entry.value;
  *tag = entry_local.entry.tag;
  *writer_id = entry_local.entry.writer_id;
  printf("WID %u\n", entry_local.entry.writer_id);
  //*operation = entry_local.entry.operation;
  *final_flags = entry_local.flags;

  return completion_slot;
}
