/*
** Copyright (C) 2015  Cable Television Laboratories, Inc.
** Contact: http://www.cablelabs.com/
 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __H_SCTE35
#define __H_SCTE35

#include <stdint.h>
#include <vqarray.h>
#include <ts.h>
#include <descriptors.h>

typedef struct 
{
   uint8_t time_specified_flag;
   uint64_t pts_time;

} scte35_splice_time;

typedef struct 
{
   uint8_t component_tag;
   scte35_splice_time *splice_time;

} scte35_splice_insert_component;

typedef struct 
{
   uint8_t component_tag;
   uint32_t splice_time;

} scte35_splice_event_component;

typedef struct 
{
   uint8_t auto_return;
   uint64_t duration;

} scte35_break_duration;

typedef struct 
{
   uint8_t tag;
   uint8_t length;
   uint32_t identifier;

   uint8_t* private_bytes;

} scte35_splice_descriptor;

typedef struct 
{
   uint8_t table_id; 
   uint8_t section_syntax_indicator; 
   uint8_t private_indicator; 
   uint16_t section_length; 
   uint8_t protocol_version; 
   uint8_t encrypted_packet; 
   uint8_t encryption_algorithm; 
   uint64_t pts_adjustment; 
   uint8_t cw_index;
   uint16_t tier; 
   uint16_t splice_command_length; 
   uint8_t splice_command_type;

   void *splice_command;

   vqarray_t *splice_descriptors;

   uint32_t E_CRC_32;
   uint32_t CRC_32;

} scte35_splice_info_section;

typedef struct 
{
   uint32_t splice_event_id;
   uint8_t splice_event_cancel_indicator;
   uint8_t out_of_network_indicator;
   uint8_t program_splice_flag;
   uint8_t duration_flag;
   uint8_t splice_immediate_flag;
   scte35_splice_time *splice_time;
   vqarray_t* components;
   scte35_break_duration *break_duration;
   uint16_t unique_program_id;
   uint8_t avail_num;
   uint8_t avails_expected;

} scte35_splice_insert;

typedef struct 
{
   uint32_t splice_event_id;
   uint8_t splice_event_cancel_indicator;
   uint8_t out_of_network_indicator;
   uint8_t program_splice_flag;
   uint8_t duration_flag;
   uint32_t utc_splice_time;
   vqarray_t* components;
   scte35_break_duration *break_duration;
   uint16_t unique_program_id;
   uint8_t avail_num;
   uint8_t avails_expected;

} scte35_splice_event;

typedef struct 
{
   vqarray_t* splice_events;

} scte35_splice_schedule;

typedef struct 
{
   scte35_splice_time *splice_time;

} scte35_time_signal;

typedef struct 
{
   uint32_t identifier;
   uint32_t private_bytes_sz;
   uint8_t* private_bytes;

} scte35_private_command;


#define SCTE35_DESCRIPTOR 0x99  // GORP
#define SCTE35_SPLICE_TABLE_ID 0xFC  // GORP??

scte35_splice_info_section* scte35_splice_info_section_new(); 
void scte35_splice_info_section_free(scte35_splice_info_section *sis); 
int scte35_splice_info_section_read(scte35_splice_info_section *sis, uint8_t *buf, size_t buf_len); 
void scte35_splice_info_section_print_stdout(const scte35_splice_info_section *sis); 

uint64_t get_splice_insert_PTS (scte35_splice_info_section *sis);
int is_splice_insert (scte35_splice_info_section *sis);

void scte35_parse_splice_null(bs_t *b);
scte35_splice_schedule* scte35_parse_splice_schedule(bs_t *b);
scte35_splice_insert* scte35_parse_splice_insert(bs_t *b);
scte35_time_signal* scte35_parse_time_signal(bs_t *b);
void scte35_parse_bandwidth_reservation(bs_t *b);
scte35_private_command* scte35_parse_private_command(bs_t *b, uint16_t command_sz);
scte35_splice_event* scte35_parse_splice_event(bs_t *b);


void scte35_free_splice_null();
void scte35_free_splice_schedule(scte35_splice_schedule *splice_schedule);
void scte35_free_splice_insert(scte35_splice_insert* splice_insert);
void scte35_free_time_signal(scte35_time_signal* time_signal);
void scte35_free_bandwidth_reservation();
void scte35_free_private_command(scte35_private_command *private_command);
void scte35_free_splice_event(scte35_splice_event* spice_event);

void scte35_splice_null_print_stdout ();
void scte35_splice_schedule_print_stdout (const scte35_splice_schedule *splice_schedule);
void scte35_splice_event_print_stdout (const scte35_splice_event *splice_event, int splice_event_num);
void scte35_splice_insert_print_stdout (const scte35_splice_insert *splice_insert);
void scte35_time_signal_print_stdout (scte35_time_signal* time_signal);
void scte35_bandwidth_reservation_print_stdout ();
void scte35_private_command_print_stdout (scte35_private_command *private_command);


scte35_splice_time* scte35_parse_splice_time(bs_t *b);
scte35_splice_insert_component* scte35_parse_splice_insert_component(bs_t *b, uint8_t splice_immediate_flag);
scte35_splice_event_component* scte35_parse_splice_event_component(bs_t *b);
scte35_break_duration* scte35_parse_break_duration (bs_t *b);

scte35_splice_descriptor* scte35_parse_splice_descriptor (bs_t *b);
void scte35_free_splice_descriptor (scte35_splice_descriptor* splice_descriptor);


#endif  // __H_SCTE35

