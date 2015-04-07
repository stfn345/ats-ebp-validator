/*
Copyright (c) 2015, Cable Television Laboratories, Inc.(“CableLabs”)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of CableLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL CABLELABS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "scte35.h"
#include "log.h"
#include "ATSTestReport.h"


scte35_splice_info_section* scte35_splice_info_section_new()
{
   scte35_splice_info_section *sis = (scte35_splice_info_section *)calloc(1, sizeof(scte35_splice_info_section));
   return sis;
}

void scte35_splice_info_section_free(scte35_splice_info_section *sis)
{
   if (sis == NULL) 
   {
      return; 
   }

   if (sis->splice_command != NULL)
   {
      if(sis->splice_command_type == 0x00)
      {
         scte35_free_splice_null();
      }
      else if(sis->splice_command_type == 0x04)
      {
         scte35_free_splice_schedule((scte35_splice_schedule *)sis->splice_command);
      }
      else if(sis->splice_command_type == 0x05)
      {
         scte35_free_splice_insert((scte35_splice_insert *)sis->splice_command);
      }
      else if(sis->splice_command_type == 0x06)
      {
         scte35_free_time_signal((scte35_time_signal *)sis->splice_command);
      }
      else if(sis->splice_command_type == 0x07)
      {
         scte35_free_bandwidth_reservation();
      }
      else if(sis->splice_command_type == 0xff)
      {
         scte35_free_private_command((scte35_private_command *)sis->splice_command);
      }
   }

   if (sis->splice_descriptors != NULL) 
   {
      for (int i=0; i<vqarray_length(sis->splice_descriptors); i++)
      {
         scte35_splice_descriptor *splice_descriptor = (scte35_splice_descriptor*) vqarray_get (sis->splice_descriptors, i);
         scte35_free_splice_descriptor (splice_descriptor);
      }
      vqarray_free(sis->splice_descriptors); 
   }

   free(sis);
}

int is_splice_insert (scte35_splice_info_section *sis)
{
   return (sis->splice_command_type == 0x05);
}


uint64_t get_splice_insert_PTS (scte35_splice_info_section *sis)
{
   if (sis->splice_command_type == 0x05)
   {
      if (((scte35_splice_insert *)sis->splice_command)->splice_time != NULL)
      {
         if (((scte35_splice_insert *)sis->splice_command)->splice_time->time_specified_flag)
         {
            return ((scte35_splice_insert *)sis->splice_command)->splice_time->pts_time + sis->pts_adjustment;
         }
      }
   }

   return 0;
}


scte35_splice_insert * get_splice_insert (scte35_splice_info_section *sis)
{
   if (sis->splice_command_type == 0x05)
   {
      return (scte35_splice_insert *)(sis->splice_command);
   }

   return 0;
}

int is_time_signal (scte35_splice_info_section *sis)
{
   return (sis->splice_command_type == 0x06);
}

scte35_time_signal * get_time_signal (scte35_splice_info_section *sis)
{
   if (sis->splice_command_type == 0x06)
   {
      return (scte35_time_signal *)(sis->splice_command);
   }

   return 0;
}


int scte35_splice_info_section_read(scte35_splice_info_section *sis, uint8_t *buf, size_t buf_len,
   uint32_t payload_unit_start_indicator, psi_table_buffer_t *scte35TableBuffer)
{
   if (!payload_unit_start_indicator &&  scte35TableBuffer->buffer == NULL)
   {
      // this TS packet is not start of table, and we have no cached table data
      LOG_WARN ("scte35_splice_info_section_read: payload_unit_start_indicator not set and no cached data");
      return 0;
   }

   bs_t *b = NULL;

   if (payload_unit_start_indicator)
   {
      uint8_t payloadStartPtr = buf[0];
      buf += (payloadStartPtr + 1);
      buf_len -= (payloadStartPtr + 1);
      LOG_DEBUG_ARGS ("scte35_splice_info_section_read: payloadStartPtr = %d", payloadStartPtr);
   }

   // check for pat spanning multiple TS packets
   if (scte35TableBuffer->buffer != NULL)
   {
      LOG_DEBUG_ARGS ("scte35_splice_info_section_read: scte35TableBuffer detected: scte35TableBufferAllocSz = %d, scte35TableBufferUsedSz = %d", 
         scte35TableBuffer->bufferAllocSz, scte35TableBuffer->bufferUsedSz);
      size_t numBytesToCopy = buf_len;
      if (buf_len > (scte35TableBuffer->bufferAllocSz - scte35TableBuffer->bufferUsedSz))
      {
         numBytesToCopy = scte35TableBuffer->bufferAllocSz - scte35TableBuffer->bufferUsedSz;
      }
         
      LOG_DEBUG_ARGS ("scte35_splice_info_section_read: copying %d bytes to catBuffer", numBytesToCopy);
      memcpy (scte35TableBuffer->buffer + scte35TableBuffer->bufferUsedSz, buf, numBytesToCopy);
      scte35TableBuffer->bufferUsedSz += numBytesToCopy;
      
      if (scte35TableBuffer->bufferUsedSz < scte35TableBuffer->bufferAllocSz)
      {
         LOG_DEBUG ("scte35_splice_info_section_read: scte35Buffer not yet full -- returning");
         return 0;
      }

      b = bs_new(scte35TableBuffer->buffer, scte35TableBuffer->bufferUsedSz);
   }
   else
   {
      b = bs_new(buf, buf_len);
      sleep (1);
   }
            
   sis->table_id = bs_read_u8(b); 
   if (SCTE35_SPLICE_TABLE_ID != sis->table_id)
   {
      LOG_ERROR_ARGS ("scte35_splice_info_section_read: FAIL: table_id does not equal 0x%x", SCTE35_SPLICE_TABLE_ID);
      reportAddErrorLogArgs ("scte35_splice_info_section_read: FAIL: table_id does not equal 0x%x", SCTE35_SPLICE_TABLE_ID);
      return -1;
   }

   sis->section_syntax_indicator = bs_read_u1(b);
   sis->private_indicator = bs_read_u1(b);
   bs_skip_u(b, 2);  // reserved
   sis->section_length = bs_read_u(b, 12); 

   if (sis->section_length > bs_bytes_left(b))
   {
      LOG_DEBUG ("scte35_splice_info_section_read: Detected section spans more than one TS packet -- allocating buffer");

      if (scte35TableBuffer->buffer != NULL)
      {
         // should never get here
         LOG_ERROR ("scte35_splice_info_section_read: unexpected catBufffer");
         reportAddErrorLog ("scte35_splice_info_section_read: unexpected catBufffer");
         resetPSITableBuffer(scte35TableBuffer);
      }

      scte35TableBuffer->bufferAllocSz = sis->section_length + 3;
      scte35TableBuffer->buffer = (uint8_t *)calloc (sis->section_length + 3, 1);
      memcpy (scte35TableBuffer->buffer, buf, buf_len);
      scte35TableBuffer->bufferUsedSz = buf_len;

      bs_free (b);
      return 0;
   }

   sis->protocol_version = bs_read_u(b, 8); 
   sis->encrypted_packet = bs_read_u(b, 1); 
   sis->encryption_algorithm = bs_read_u(b, 6); 
   sis->pts_adjustment = bs_read_ull(b, 33);

   sis->cw_index = bs_read_u(b, 8);
   sis->tier = bs_read_u(b, 12); 
   sis->splice_command_length = bs_read_u(b, 12);
   sis->splice_command_type = bs_read_u(b, 8);

   if(sis->splice_command_type == 0x00)
   {
      scte35_parse_splice_null(b);
   }
   else if(sis->splice_command_type == 0x04)
   {
      sis->splice_command = scte35_parse_splice_schedule(b);
   }
   else if(sis->splice_command_type == 0x05)
   {
      sis->splice_command = scte35_parse_splice_insert(b);
   }
   else if(sis->splice_command_type == 0x06)
   {
      sis->splice_command = scte35_parse_time_signal(b);
   }
   else if(sis->splice_command_type == 0x07)
   {
      scte35_parse_bandwidth_reservation(b);
   }
   else if(sis->splice_command_type == 0xff)
   {
      sis->splice_command = scte35_parse_private_command(b, sis->splice_command_length);
   }

   uint16_t descriptor_loop_length = bs_read_u(b, 16);

   if (descriptor_loop_length != 0)
   {
      sis->splice_descriptors = vqarray_new();
   }
   for (int i=0; i<descriptor_loop_length; i++)
   {
      // read descriptors
      scte35_splice_descriptor* splice_descriptor = scte35_parse_splice_descriptor (b);
      vqarray_add (sis->splice_descriptors, splice_descriptor);
   }

   // GORP: read alignment bytes: calculate number of these from section length I think

   if (sis->encrypted_packet)
   {
      sis->E_CRC_32 = bs_read_u32(b);
   }

   sis->CRC_32 = bs_read_u32(b);
 
   resetPSITableBuffer(scte35TableBuffer);

   return 1;
}

void scte35_splice_info_section_print_stdout(const scte35_splice_info_section *sis)
{
   LOG_INFO ("\nSCTE35 Splice Info Section:");
   LOG_INFO_ARGS ("   table_id = %d", sis->table_id);
   LOG_INFO_ARGS ("   section_syntax_indicator = %d", sis->section_syntax_indicator);
   LOG_INFO_ARGS ("   private_indicator = %d", sis->private_indicator);
   LOG_INFO_ARGS ("   section_length = %d", sis->section_length);
   LOG_INFO_ARGS ("   protocol_version = %d", sis->protocol_version);
   LOG_INFO_ARGS ("   encrypted_packet = %d", sis->encrypted_packet);
   LOG_INFO_ARGS ("   encryption_algorithm = %d", sis->encryption_algorithm);

   LOG_INFO_ARGS ("   pts_adjustment = %"PRId64"", sis->pts_adjustment);

   LOG_INFO_ARGS ("   cw_index = %d", sis->cw_index);
   LOG_INFO_ARGS ("   tier = %d", sis->tier);
   LOG_INFO_ARGS ("   splice_command_length = %d", sis->splice_command_length);
   LOG_INFO_ARGS ("   splice_command_type = %d", sis->splice_command_type);

   if (sis->splice_command != NULL)
   {
      if(sis->splice_command_type == 0x00)
      {
         scte35_splice_null_print_stdout();
      }
      else if(sis->splice_command_type == 0x04)
      {
         scte35_splice_schedule_print_stdout((scte35_splice_schedule *) sis->splice_command);
      }
      else if(sis->splice_command_type == 0x05)
      {
         scte35_splice_insert_print_stdout ((scte35_splice_insert *) sis->splice_command);
      }
      else if(sis->splice_command_type == 0x06)
      {
         scte35_time_signal_print_stdout ((scte35_time_signal *) sis->splice_command);
      }
      else if(sis->splice_command_type == 0x07)
      {
         scte35_bandwidth_reservation_print_stdout ();
      }
      else if(sis->splice_command_type == 0xff)
      {
         scte35_private_command_print_stdout ((scte35_private_command *) sis->splice_command);
      }
   }

   /*
   vqarray_t *splice_descriptors;

   uint32_t E_CRC_32;
   uint32_t CRC_32;
   */

}

void scte35_splice_insert_print_stdout (const scte35_splice_insert *splice_insert)
{
   LOG_INFO ("\n   SCTE35 Splice Insert:");
   LOG_INFO_ARGS ("      splice_event_id = %d", splice_insert->splice_event_id);
   LOG_INFO_ARGS ("      splice_event_cancel_indicator = %d", splice_insert->splice_event_cancel_indicator);
   LOG_INFO_ARGS ("      out_of_network_indicator = %d", splice_insert->out_of_network_indicator);
   LOG_INFO_ARGS ("      program_splice_flag = %d", splice_insert->program_splice_flag);
   LOG_INFO_ARGS ("      duration_flag = %d", splice_insert->duration_flag);
   LOG_INFO_ARGS ("      splice_immediate_flag = %d", splice_insert->splice_immediate_flag);

   if (splice_insert->splice_time != NULL)
   {
      LOG_INFO_ARGS ("      splice_time->time_specified_flag = %d", splice_insert->splice_time->time_specified_flag);
      LOG_INFO_ARGS ("      splice_time->pts_time = %"PRId64"", splice_insert->splice_time->pts_time);
   }

   if (splice_insert->break_duration != NULL)
   {
      LOG_INFO_ARGS ("      break_duration->auto_return = %d", splice_insert->break_duration->auto_return);
      LOG_INFO_ARGS ("      break_duration->duration = %"PRId64"", splice_insert->break_duration->duration);
   }

   LOG_INFO_ARGS ("      unique_program_id = %d", splice_insert->unique_program_id);
   LOG_INFO_ARGS ("      avail_num = %d", splice_insert->avail_num);
   LOG_INFO_ARGS ("      avails_expected = %d", splice_insert->avails_expected);

   if (splice_insert->components != NULL)
   {
      LOG_INFO_ARGS ("component length = %d", vqarray_length(splice_insert->components));
      for (int i=0; i<vqarray_length(splice_insert->components); i++)
      {
         scte35_splice_insert_component *component = (scte35_splice_insert_component *) vqarray_get (splice_insert->components, i);
         LOG_INFO_ARGS ("      component %d:", i);
         LOG_INFO_ARGS ("         component_tag = %d", component->component_tag);
         if (component->splice_time != NULL)
         {
            LOG_INFO_ARGS ("         component time_specified_flag = %d", component->splice_time->time_specified_flag);
            LOG_INFO_ARGS ("         component pts_time = %"PRId64"", component->splice_time->pts_time);
         }
      }
   }
   else
   {
      LOG_INFO ("component length = 0");
   }
   LOG_INFO ("");
}


void scte35_splice_event_print_stdout (const scte35_splice_event *splice_event, int splice_event_num)
{
   LOG_INFO_ARGS ("\n   SCTE35 Splice Event %d:", splice_event_num);
   LOG_INFO_ARGS ("      splice_event_id = %d", splice_event->splice_event_id);
   LOG_INFO_ARGS ("      splice_event_cancel_indicator = %d", splice_event->splice_event_cancel_indicator);
   LOG_INFO_ARGS ("      out_of_network_indicator = %d", splice_event->out_of_network_indicator);
   LOG_INFO_ARGS ("      program_splice_flag = %d", splice_event->program_splice_flag);
   LOG_INFO_ARGS ("      duration_flag = %d", splice_event->duration_flag);

   if (splice_event->program_splice_flag == 1)
   {
      LOG_INFO_ARGS ("      utc_splice_time = %d", splice_event->utc_splice_time);
   }

   if (splice_event->break_duration != NULL)
   {
      LOG_INFO_ARGS ("      break_duration->auto_return = %d", splice_event->break_duration->auto_return);
      LOG_INFO_ARGS ("      break_duration->duration = %"PRId64"", splice_event->break_duration->duration);
   }

   LOG_INFO_ARGS ("      unique_program_id = %d", splice_event->unique_program_id);
   LOG_INFO_ARGS ("      avail_num = %d", splice_event->avail_num);
   LOG_INFO_ARGS ("      avails_expected = %d", splice_event->avails_expected);

   if (splice_event->components != NULL)
   {
      for (int i=0; i<vqarray_length(splice_event->components); i++)
      {
         scte35_splice_event_component *component = (scte35_splice_event_component *) vqarray_get (splice_event->components, i);
         LOG_INFO_ARGS ("      component %d:", i);
         LOG_INFO_ARGS ("         component_tag = %d", component->component_tag);
         LOG_INFO_ARGS ("         utc_splice_time = %d", component->splice_time);
      }
   }
   LOG_INFO ("");
}


void scte35_parse_splice_null(bs_t *b)
{
   // nothing to do here: splice_null is empty
}

void scte35_free_splice_null()
{
   // nothing to do here: splice_null is empty
}

scte35_splice_schedule* scte35_parse_splice_schedule(bs_t *b)
{
   scte35_splice_schedule *splice_schedule = (scte35_splice_schedule *)calloc(1, sizeof(scte35_splice_schedule));

   uint8_t splice_event_count = bs_read_u8(b);

   splice_schedule->splice_events = vqarray_new();
   for (int i=0; i<splice_event_count; i++)
   {
      scte35_splice_event* splice_event = scte35_parse_splice_event(b);
      vqarray_add (splice_schedule->splice_events, splice_event);
   }

   return splice_schedule;
}

void scte35_free_splice_schedule(scte35_splice_schedule *splice_schedule)
{
   if (splice_schedule->splice_events != NULL)
   {
      for (int i=0; i<vqarray_length(splice_schedule->splice_events); i++)
      {
         scte35_splice_event* splice_event = vqarray_get (splice_schedule->splice_events, i);
         scte35_free_splice_event(splice_event);
      }

      vqarray_free (splice_schedule->splice_events);
   }

   free (splice_schedule);
}

scte35_splice_event* scte35_parse_splice_event(bs_t *b)
{
   scte35_splice_event *splice_event = (scte35_splice_event *)calloc(1, sizeof(scte35_splice_event));

   splice_event->splice_event_id = bs_read_u32(b);
   splice_event->splice_event_cancel_indicator = bs_read_u(b, 1);
   bs_skip_u(b, 7);  // reserved

   if(splice_event->splice_event_cancel_indicator == 0)
   {
      splice_event->out_of_network_indicator = bs_read_u(b, 1);
      splice_event->program_splice_flag = bs_read_u(b, 1);
      splice_event->duration_flag = bs_read_u(b, 1);
      bs_skip_u(b, 5);  // reserved

      if (splice_event->program_splice_flag == 1)
      {
         splice_event->utc_splice_time = bs_read_u32(b);
      }
      if (splice_event->program_splice_flag == 0)
      {
         uint8_t component_count = bs_read_u8(b);
         if (component_count != 0)
         {
            splice_event->components = vqarray_new();
         }
         for (int i=0; i<component_count; i++)
         {
            scte35_splice_event_component* splice_component = scte35_parse_splice_event_component(b);
            vqarray_add(splice_event->components, splice_component);
         }
      }

      if (splice_event->duration_flag == 1)
      {
         splice_event->break_duration = scte35_parse_break_duration (b);
      }
   }

   splice_event->unique_program_id = bs_read_u16(b);
   splice_event->avail_num = bs_read_u8(b);
   splice_event->avails_expected = bs_read_u8(b);

   return splice_event;
}

void scte35_free_splice_event(scte35_splice_event* splice_event)
{
   if (splice_event->components != NULL)
   {
      for (int i=0; i<vqarray_length(splice_event->components); i++)
      {
         scte35_splice_event_component *component = (scte35_splice_event_component*) vqarray_get (splice_event->components, i);
         free (component);
      }
      vqarray_free (splice_event->components);
   }

   if (splice_event->break_duration != NULL)
   {
      free (splice_event->break_duration);
   }

   free (splice_event);
}

scte35_splice_insert* scte35_parse_splice_insert(bs_t *b)
{
   scte35_splice_insert *splice_insert = (scte35_splice_insert *)calloc(1, sizeof(scte35_splice_insert));

   splice_insert->splice_event_id = bs_read_u32(b);
   splice_insert->splice_event_cancel_indicator = bs_read_u(b, 1);
   bs_skip_u(b, 7);  // reserved

   if(splice_insert->splice_event_cancel_indicator == 0)
   {
      splice_insert->out_of_network_indicator = bs_read_u(b, 1);
      splice_insert->program_splice_flag = bs_read_u(b, 1);
      splice_insert->duration_flag = bs_read_u(b, 1);
      splice_insert->splice_immediate_flag = bs_read_u(b, 1);
      bs_skip_u(b, 4);  // reserved

      if ((splice_insert->program_splice_flag == 1) && (splice_insert->splice_immediate_flag == 0))
      {
         splice_insert->splice_time = scte35_parse_splice_time (b);
      }
      if (splice_insert->program_splice_flag == 0)
      {
         uint8_t component_count = bs_read_u8(b);
         if (component_count != 0)
         {
            splice_insert->components = vqarray_new();
         }
         for (int i=0; i<component_count; i++)
         {
            scte35_splice_insert_component* splice_component = scte35_parse_splice_insert_component(b, splice_insert->splice_immediate_flag);
            vqarray_add(splice_insert->components, splice_component);
         }
      }

      if (splice_insert->duration_flag == 1)
      {
         splice_insert->break_duration = scte35_parse_break_duration (b);
      }
   }

   splice_insert->unique_program_id = bs_read_u16(b);
   splice_insert->avail_num = bs_read_u8(b);
   splice_insert->avails_expected = bs_read_u8(b);

   return splice_insert;
}

void scte35_free_splice_insert(scte35_splice_insert* spice_insert)
{
   if (spice_insert->splice_time != NULL)
   {
      free (spice_insert->splice_time);
   }

   if (spice_insert->components != NULL)
   {
      for (int i=0; i<vqarray_length(spice_insert->components); i++)
      {
         scte35_splice_insert_component *component = (scte35_splice_insert_component*) vqarray_get (spice_insert->components, i);
         free (component);
      }
      vqarray_free (spice_insert->components);
   }

   if (spice_insert->break_duration != NULL)
   {
      free (spice_insert->break_duration);
   }

   free (spice_insert);
}

scte35_time_signal* scte35_parse_time_signal(bs_t *b)
{
   scte35_time_signal *time_signal = (scte35_time_signal *)calloc(1, sizeof(scte35_time_signal));

   time_signal->splice_time = scte35_parse_splice_time(b);

   return time_signal;
}

void scte35_free_time_signal(scte35_time_signal *time_signal)
{
   free (time_signal->splice_time);
   free (time_signal);
}

void scte35_parse_bandwidth_reservation(bs_t *b)
{
   // Nothing to be done here
}

void scte35_free_bandwidth_reservation()
{
   // Nothing to be done here
}

scte35_private_command* scte35_parse_private_command(bs_t *b, uint16_t command_sz)
{
   scte35_private_command *private_command = (scte35_private_command *)calloc(1, sizeof(scte35_private_command));

   private_command->identifier = bs_read_u32(b);
   if (command_sz > 4)
   {
      private_command->private_bytes_sz = command_sz - 4;
      private_command->private_bytes = (uint8_t *)calloc (private_command->private_bytes_sz, 1);
      for (int i=0; i<private_command->private_bytes_sz; i++)
      {
         private_command->private_bytes[i] = bs_read_u8(b);
      }
   }

   return private_command;
}

void scte35_free_private_command(scte35_private_command *private_command)
{
   if (private_command->private_bytes != NULL)
   {
      free (private_command->private_bytes);
   }

   free (private_command);
}

void scte35_splice_null_print_stdout ()
{
   LOG_INFO ("\n   SCTE35 Null");
}

void scte35_splice_schedule_print_stdout (const scte35_splice_schedule *splice_schedule)
{
   LOG_INFO ("\n   SCTE35 Splice Schedule: ");
   for (int i=0; i< vqarray_length(splice_schedule->splice_events); i++)
   {
      scte35_splice_event *splice_event = (scte35_splice_event*) vqarray_get (splice_schedule->splice_events, i);
      scte35_splice_event_print_stdout (splice_event, i);
   }
   LOG_INFO ("");
}

void scte35_time_signal_print_stdout (scte35_time_signal* time_signal)
{
   LOG_INFO ("\n   SCTE35 Time Signal: ");

   LOG_INFO_ARGS ("      splice_time->time_specified_flag = %d", time_signal->splice_time->time_specified_flag);
   LOG_INFO_ARGS ("      splice_time->pts_time = %"PRId64"", time_signal->splice_time->pts_time);
   LOG_INFO ("");
}

void scte35_bandwidth_reservation_print_stdout ()
{
   LOG_INFO ("\n   SCTE35 Bandwidth Reservation");
}

void scte35_private_command_print_stdout (scte35_private_command* private_command)
{
   LOG_INFO ("\n   SCTE35 Private Command: ");
   LOG_INFO_ARGS ("      identifier = %u ", private_command->identifier);
   LOG_INFO_ARGS ("      private_bytes_sz = %u ", private_command->private_bytes_sz);
   LOG_INFO ("");

   // GORP: print private_bytes
}


scte35_splice_time* scte35_parse_splice_time(bs_t *b)
{
   scte35_splice_time *splice_time = (scte35_splice_time *)calloc(1, sizeof(scte35_splice_time));

   splice_time->time_specified_flag = bs_read_u(b, 1);
   if (splice_time->time_specified_flag)
   {
      bs_skip_u(b, 6);  // reserved
      splice_time->pts_time = bs_read_ull(b, 33);
   }
   else
   {
      bs_skip_u(b, 7);  // reserved
   }

   return splice_time;
}

scte35_splice_insert_component* scte35_parse_splice_insert_component(bs_t *b, uint8_t splice_immediate_flag)
{
   scte35_splice_insert_component *splice_component = (scte35_splice_insert_component *)calloc(1, sizeof(scte35_splice_insert_component));

   splice_component->component_tag = bs_read_u(b, 8);
   if (splice_immediate_flag)
   {
      splice_component->splice_time = scte35_parse_splice_time (b);
   }

   return splice_component;
}

scte35_splice_event_component* scte35_parse_splice_event_component(bs_t *b)
{
   scte35_splice_event_component *splice_component = (scte35_splice_event_component *)calloc(1, sizeof(scte35_splice_event_component));

   splice_component->component_tag = bs_read_u8(b);
   splice_component->splice_time = bs_read_u32(b);

   return splice_component;
}

scte35_break_duration* scte35_parse_break_duration (bs_t *b)
{
   scte35_break_duration *break_duration = (scte35_break_duration *)calloc(1, sizeof(scte35_break_duration));

   break_duration->auto_return = bs_read_u(b, 1);
   bs_skip_u(b, 6);  // reserved
   break_duration->duration = bs_read_ull(b, 33);

   return break_duration;
}

scte35_splice_descriptor* scte35_parse_splice_descriptor (bs_t *b)
{
   scte35_splice_descriptor *splice_descriptor = (scte35_splice_descriptor *)calloc(1, sizeof(scte35_splice_descriptor));

   splice_descriptor->tag = bs_read_u8(b);
   splice_descriptor->length = bs_read_u8(b);
   splice_descriptor->identifier = bs_read_u32(b);

   if (splice_descriptor->length != 0)
   {
      splice_descriptor->private_bytes = calloc (splice_descriptor->length, sizeof (uint8_t));
      for (int i=0; i<splice_descriptor->length; i++)
      {
         splice_descriptor->private_bytes[i] = bs_read_u8(b);
      }
   }

   return splice_descriptor;
}

void scte35_free_splice_descriptor (scte35_splice_descriptor* splice_descriptor)
{
   if (splice_descriptor->private_bytes != NULL)
   {
      free (splice_descriptor->private_bytes);
   }

   free (splice_descriptor);
}

