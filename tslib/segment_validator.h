


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "libts_common.h"
#include "ts.h"
#include "psi.h"
#include "mpeg2ts_demux.h"
#include "log.h"
#include "pes.h"
#include "tpes.h"
#include "vqarray.h"

#ifndef __H_SEGMENT_VALIDATOR
#define __H_SEGMENT_VALIDATOR

#define TS_STATE_PAT   0x01
#define TS_STATE_PMT   0x02
#define TS_STATE_PCR   0x04
#define TS_STATE_ECM   0x08

#define TS_TEST_DASH   0x01
#define TS_TEST_MAIN   0x02
#define TS_TEST_SIMPLE 0x04

#define PID_EMSG    0x04

typedef enum {
   UNKNOWN_CONTENT_COMPONENT = 0x00,
   VIDEO_CONTENT_COMPONENT, 
   AUDIO_CONTENT_COMPONENT, 
   NUM_CONTENT_COMPONENTS
} content_compenent_t; 

typedef enum {
   MEDIA_SEGMENT = 0x00,
   INITIALIZAION_SEGMENT, 
   REPRESENTATION_INDEX_SEGMENT
} segment_type_t; 


typedef struct
{
   int PID; 
   int SAP; 
   int SAP_type; 
   int64_t EPT; // earliest playout time 
   int64_t LPT; // latest playout time 
   int64_t duration; // duration of latest pes packet
   uint64_t pes_cnt; 
   uint64_t ts_cnt; 
   int content_component; 
   int continuity_counter; 
   vqarray_t *ecm_pids;
} pid_validator_t; 

typedef struct
{
   uint32_t conformance_level; 
   int64_t  last_pcr; 
   long segment_start; 
   long segment_end; 
   vqarray_t *pids; 
   int PCR_PID; 
   int status; // 0 == fail
   uint32_t psi_tables_seen;
   segment_type_t segment_type;
   int use_initializaion_segment;
   program_map_section_t *initializaion_segment_pmt;      /// parsed PMT
} dash_validator_t; 


int pat_processor(mpeg2ts_stream_t *m2s, void *arg); 
int pmt_processor(mpeg2ts_program_t *m2p, void *arg); 
int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg); 
int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg); 
/*
int doSegmentValidation(dash_validator_t *dash_validator, char *fname, dash_validator_t *dash_validator_init,
                        data_segment_iframes_t* pIFrameData, unsigned int segmentDuration);
*/
void doDASHEventValidation(uint8_t* buf, int len);

#endif
