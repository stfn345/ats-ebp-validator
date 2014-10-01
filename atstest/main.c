
#include <stdlib.h>
#include <errno.h>
#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>

#define TS_SIZE 188

static int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{
   return 1;
}

static int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   return 1;
}

static int pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   pid_info_t *pi = NULL;
   for (int i = 0; i < vqarray_length(m2p->pids); i++) // TODO replace linear search w/ hashtable lookup in the future
   {
      if ((pi = vqarray_get(m2p->pids, i)) != NULL)
      {
         int handle_pid = 0;

         if (IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            // Look for the SCTE128 descriptor in the ES loop
            descriptor_t *desc = NULL;
            for (int d = 0; d < vqarray_length(pi->es_info->descriptors); d++)
            {
               if ((desc = vqarray_get(pi->es_info->descriptors, d)) != NULL && desc->tag == 0x97)
               {
                  mpeg2ts_program_enable_scte128(m2p);
               }
            }
            handle_pid = 1;
         }
         else if (IS_AUDIO_STREAM(pi->es_info->stream_type))
         {
            handle_pid = 1;
         }

         if (handle_pid)
         {
            pes_demux_t *pd = pes_demux_new(validate_pes_packet);
            pd->pes_arg = NULL;
            pd->pes_arg_destructor = NULL;
            pd->process_pes_packet = validate_pes_packet;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_handler = calloc(1, sizeof(demux_pid_handler_t));
            demux_handler->process_ts_packet = pes_demux_process_ts_packet;
            demux_handler->arg = pd;
            demux_handler->arg_destructor = (arg_destructor_t)pes_demux_free;


            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_validator = calloc(1, sizeof(demux_pid_handler_t));
            demux_validator->process_ts_packet = validate_ts_packet;
            demux_validator->arg = NULL;
            demux_validator->arg_destructor = NULL;

            // hook PID processor to PID
            mpeg2ts_program_register_pid_processor(m2p, pi->es_info->elementary_PID, demux_handler, demux_validator);
         }
      }
   }

   return 1;
}

static int pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{
   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)pmt_processor;
   }
   return 1;
}

int main(int argc, char** argv) {

   if (argc < 2)
   {
      return 1;
   }

   char *fname = argv[1];
   if (fname == NULL || fname[0] == 0)
   {
      LOG_ERROR("No input file provided");
      return 1;
   }

   FILE *infile = NULL;
   if ((infile = fopen(fname, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("Cannot open file %s - %s", fname, strerror(errno));
      return 1;
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR("Error creating MPEG-2 STREAM object");
      return 1;
   }

   m2s->pat_processor = (pat_processor_t)pat_processor;

   int num_packets = 4096;
   uint8_t *ts_buf = malloc(TS_SIZE * 4096);

   while ((num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
   {
      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         mpeg2ts_stream_read_ts_packet(m2s, ts);
      }
   }

   mpeg2ts_stream_free(m2s);

   fclose(infile);

   return tslib_errno;
}
