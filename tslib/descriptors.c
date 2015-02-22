/* 
 Copyright (c) 2014-, ISO/IEC JTC1/SC29/WG11 
 Written by Alex Giladi <alex.giladi@gmail.com> and Vlad Zbarsky <zbarsky@cornell.edu>
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the ISO/IEC nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

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

#include "libts_common.h"
#include "ts.h"
#include "descriptors.h"
#include "log.h"

#include <hashtable.h>
#include <hashtable_str.h>

#include "ATSTestReport.h"


DEFINE_HASHTABLE_INSERT(descriptor_table_insert, uint32_t, descriptor_table_entry_t);
DEFINE_HASHTABLE_SEARCH(descriptor_table_search, uint32_t, descriptor_table_entry_t);
DEFINE_HASHTABLE_REMOVE(descriptor_table_remove, uint32_t, descriptor_table_entry_t);

static hashtable_t *g_descriptor_table = NULL;

int register_descriptor(descriptor_table_entry_t *desc)
{
   descriptor_table_remove(g_descriptor_table, &desc->tag);
   return descriptor_table_insert(g_descriptor_table, &desc->tag, desc);
}

// "factory methods"
int read_descriptor_loop(vqarray_t *desc_list, bs_t *b, int length) 
{ 
//   printf ("read_descriptor_loop\n");
   int desc_start = bs_pos(b); 
   
   while (length > bs_pos(b) - desc_start) 
   {
//      printf ("read_descriptor_loop -- 1n");
      descriptor_t *desc = descriptor_new(); 
      desc = descriptor_read(desc, b); 
      vqarray_add(desc_list, desc);
   }
   
   return bs_pos(b) - desc_start;
}

int write_descriptor_loop(vqarray_t *desc_list, bs_t *b) 
{ 
   // TODO actually implement descriptor loop writing
   return 0;
}

int print_descriptor_loop(vqarray_t *desc_list, int level, char *str, size_t str_len) 
{ 
   int bytes = 0; 
   for (int i = 0; i < vqarray_length(desc_list); i++) 
   {
      descriptor_t *desc = vqarray_get(desc_list, i); 
      if (desc != NULL) bytes += descriptor_print(desc, level, str + bytes, str_len - bytes);
   }
   return bytes;
}

descriptor_t* descriptor_new() 
{ 
   descriptor_t *desc = malloc(sizeof(descriptor_t)); 
   return desc;
}

void descriptor_free(descriptor_t *desc) 
{ 
   if (desc == NULL) return;
   descriptor_table_entry_t *dte =
         descriptor_table_search(g_descriptor_table, &desc->tag);
   if (dte == NULL) return;

   dte->free_descriptor(desc);
}

descriptor_t* descriptor_read(descriptor_t *desc, bs_t *b) 
{ 
   descriptor_t *retVal = NULL;

   desc->tag = bs_read_u8(b);
   desc->length = bs_read_u8(b);

//   printf ("descriptor_read: tag = %d, length = %d\n", desc->tag, desc->length);

   if (desc == NULL || b == NULL) return NULL;
   descriptor_table_entry_t *dte =
         descriptor_table_search(g_descriptor_table, &desc->tag);
   
   if (dte != NULL)
   {
      retVal = dte->read_descriptor(desc, b);
   }
   else
   {
      bs_skip_bytes(b, desc->length);
   }

   return retVal;
}

int descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len) 
{ 
   if (desc == NULL || str == NULL || str_len < 2 || tslib_loglevel < TSLIB_LOG_LEVEL_INFO) return 0; 
   int bytes = 0; 
   descriptor_table_entry_t *dte =
         descriptor_table_search(g_descriptor_table, (uint32_t*)&desc->tag);

   if (dte != NULL)
   {
      bytes = dte->print_descriptor(desc, level, str, str_len);
   }
   else
   {
      bytes += SKIT_LOG_UINT(str + bytes, level, desc->tag, str_len - bytes);
      bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes);
   }
   
   return bytes;
}

descriptor_t* language_descriptor_new(descriptor_t *desc) 
{ 
   language_descriptor_t *ld = NULL; 
   if (desc == NULL) 
   {
      ld = (language_descriptor_t *)calloc(1, sizeof(language_descriptor_t)); 
      ld->descriptor.tag = ISO_639_LANGUAGE_DESCRIPTOR;
   } 
   else 
   {
      ld = realloc(desc, sizeof(language_descriptor_t)); 
      ld->languages = NULL; 
      ld->num_languages = 0;
   }
   return (descriptor_t *)ld;
}

int language_descriptor_free(descriptor_t *desc)
{ 
   if (desc == NULL) return 0;
   if (desc->tag != ISO_639_LANGUAGE_DESCRIPTOR) return 0;

   language_descriptor_t *ld = (language_descriptor_t *)desc;
   if (ld->languages != NULL) free(ld->languages);
   free(ld);
   return 1;
}

descriptor_t* language_descriptor_read(descriptor_t *desc, bs_t *b) 
{ 
   if ((desc == NULL) || (b == NULL)) 
   {
      SAFE_REPORT_TS_ERR(-1); 
      return NULL;
   }
   if (desc->tag != ISO_639_LANGUAGE_DESCRIPTOR) 
   {
      LOG_ERROR_ARGS("Language descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, ISO_639_LANGUAGE_DESCRIPTOR); 
      reportAddErrorLogArgs("Language descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, ISO_639_LANGUAGE_DESCRIPTOR); 
      SAFE_REPORT_TS_ERR(-50); 
      return NULL;
   }
   
   language_descriptor_t *ld = (language_descriptor_t *)calloc(1, sizeof(language_descriptor_t)); 
   
   ld->descriptor.tag = desc->tag; 
   ld->descriptor.length = desc->length; 
   
   ld->num_languages = ld->descriptor.length / 4; // language takes 4 bytes
   
   if (ld->num_languages > 0) 
   {
      ld->languages = malloc(ld->num_languages * sizeof(language_descriptor_t)); 
      for (int i = 0; i < ld->num_languages; i++) 
      {
         ld->languages[i].ISO_639_language_code[0] = bs_read_u8(b); 
         ld->languages[i].ISO_639_language_code[1] = bs_read_u8(b); 
         ld->languages[i].ISO_639_language_code[2] = bs_read_u8(b); 
         ld->languages[i].ISO_639_language_code[3] = 0; 
         ld->languages[i].audio_type = bs_read_u8(b);
//         printf ("language_descriptor_read: %s, %x\n", ld->languages[i].ISO_639_language_code,
//            ld->languages[i].audio_type);
      }
   }

 /*  printf ("language_descriptor_read: %x %x %x, %x\n", ld->languages[i].ISO_639_language_code[0],
      ld->languages[i].ISO_639_language_code[1], ld->languages[i].ISO_639_language_code[2],
      ld->languages[i].audio_type);
   */
   free(desc); 
   return (descriptor_t *)ld;
}

int language_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len) 
{ 
   int bytes = 0; 
   if (desc == NULL) return 0; 
   if (desc->tag != ISO_639_LANGUAGE_DESCRIPTOR) return 0; 
   
   language_descriptor_t *ld = (language_descriptor_t *)desc; 
   
   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "ISO_639_language_descriptor", str_len - bytes); 
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes); 
   
   if (ld->num_languages > 0) 
   {
      for (int i = 0; i < ld->num_languages; i++) 
      {
         bytes += SKIT_LOG_STR(str + bytes, level, strtol(ld->languages[i].ISO_639_language_code, NULL, 10), str_len - bytes); 
         bytes += SKIT_LOG_UINT(str + bytes, level, ld->languages[i].audio_type, str_len - bytes);
      }
   }
   
   return bytes;
}


descriptor_t* component_name_descriptor_new(descriptor_t *desc) 
{ 
   component_name_descriptor_t *cnd = NULL; 
   if (desc == NULL) 
   {
      cnd = (component_name_descriptor_t *)calloc(1, sizeof(component_name_descriptor_t)); 
      cnd->descriptor.tag = COMPONENT_NAME_DESCRIPTOR;
   } 
   else 
   {
      cnd = realloc(desc, sizeof(component_name_descriptor_t)); 
      cnd->num_names = 0;
      cnd->names = NULL; 
      cnd->languages = NULL; 
   }

   return (descriptor_t *)cnd;
}

int component_name_descriptor_free(descriptor_t *desc)
{ 
   if (desc == NULL) return 0;
   if (desc->tag != COMPONENT_NAME_DESCRIPTOR) return 0;

   component_name_descriptor_t *cnd = (component_name_descriptor_t *)desc;

   for (int i=0; i<cnd->num_names; i++)
   {
      if (cnd->names[i] != NULL) free (cnd->names[i]);
   }

   if (cnd->names != NULL) free(cnd->names);
   if (cnd->languages != NULL) free(cnd->languages);

   free(cnd);
   
   return 1;
}

descriptor_t* component_name_descriptor_read(descriptor_t *desc, bs_t *b) 
{ 
   if ((desc == NULL) || (b == NULL)) 
   {
      SAFE_REPORT_TS_ERR(-1); 
      return NULL;
   }
   if (desc->tag != COMPONENT_NAME_DESCRIPTOR) 
   {
      LOG_ERROR_ARGS("Component Name descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, COMPONENT_NAME_DESCRIPTOR); 
      reportAddErrorLogArgs("Component Name descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, COMPONENT_NAME_DESCRIPTOR); 
      SAFE_REPORT_TS_ERR(-50); 
      return NULL;
   }
   
   component_name_descriptor_t *cnd = (component_name_descriptor_t *)calloc(1, sizeof(component_name_descriptor_t)); 
   
   cnd->descriptor.tag = desc->tag; 
   cnd->descriptor.length = desc->length; 
      
   cnd->num_names = bs_read_u8(b); 

   cnd->languages = (iso639_lang_t*) calloc (cnd->num_names, sizeof(iso639_lang_t));
   cnd->names = (char **)calloc (cnd->num_names, sizeof(char *));

   for (int i=0; i<cnd->num_names; i++)
   {
      cnd->languages[i].ISO_639_language_code[0] = bs_read_u8(b); 
      cnd->languages[i].ISO_639_language_code[1] = bs_read_u8(b); 
      cnd->languages[i].ISO_639_language_code[2] = bs_read_u8(b); 
      cnd->languages[i].ISO_639_language_code[3] = 0; 

      int totalNumBytes = 0;
      uint8_t num_segments = bs_read_u8(b); 
      for (int j=0; j< num_segments; j++) 
      {
         uint8_t compression_type = bs_read_u8(b); 
         uint8_t mode = bs_read_u8(b); 
         uint8_t number_bytes = bs_read_u8(b); 
            
         cnd->names[i] = (char *)realloc (cnd->names[i], totalNumBytes + number_bytes);
         bs_read_bytes(b, (uint8_t*)(cnd->names[i] + totalNumBytes), number_bytes); 

         totalNumBytes += number_bytes;

         if (compression_type != 0)
         {
            // GORP: error here -- not supported
            // but I think this will still work since we are just comparing -- if
            // all streams use same compression!
         }
      }
   }

   /*
   multiple_string_structure() 
   {
      number_strings (8 uimsbf)
      for (i=0; i< number_strings; i++) 
      {
         ISO_639_language_code (24 uimsbf)
         number_segments (8 uimsbf)
         for (j=0; j< number_segments; j++) 
         {
            compression_type (8 uimsbf)
            mode (8 uimsbf)
            number_bytes (8 uimsbf)
            for (k=0; k< number_bytes; k++)
            {
               compressed_string_byte [k] (8 bslbf)
            }
         }
      }
   }
   */

   free(desc); 
   return (descriptor_t *)cnd;
}

int component_name_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len) 
{ 
   int bytes = 0; 
   if (desc == NULL) return 0; 
   if (desc->tag != COMPONENT_NAME_DESCRIPTOR) return 0; 
   
   component_name_descriptor_t *ld = (component_name_descriptor_t *)desc; 
   
   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "component_name_descriptor", str_len - bytes); 
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes); 
// GORP: fix this
//   bytes += SKIT_LOG_STR(str + bytes, level, ld->name, str_len - bytes); 
   
   return bytes;
}


descriptor_t* ca_descriptor_new(descriptor_t *desc)
{
   ca_descriptor_t *cad = NULL;
   cad = (ca_descriptor_t *)calloc(1, sizeof(ca_descriptor_t));
   cad->descriptor.tag = CA_DESCRIPTOR;
   if (desc != NULL) cad->descriptor.tag = desc->length;
   free(desc);
   return (descriptor_t *)cad;
}

int ca_descriptor_free(descriptor_t *desc)
{
   if (desc == NULL) return 0;
   if (desc->tag != CA_DESCRIPTOR) return 0;

   ca_descriptor_t *cad = (ca_descriptor_t *)desc;
   if (cad->private_data_bytes != NULL) free(cad->private_data_bytes);

   free(cad);
   return 1;
}

descriptor_t* ca_descriptor_read(descriptor_t *desc, bs_t *b)
{
   if ((desc == NULL) || (b == NULL)) return NULL;

   ca_descriptor_t *cad = (ca_descriptor_t *)ca_descriptor_new(desc);

   cad->CA_system_ID = bs_read_u16(b);
   bs_skip_u(b, 3);
   cad->CA_PID = bs_read_u(b, 13);
   cad->_private_data_bytes_buf_len = cad->descriptor.length - 4; // we just read 4 bytes
   cad->private_data_bytes = malloc(cad->_private_data_bytes_buf_len);
   bs_read_bytes(b, cad->private_data_bytes, cad->_private_data_bytes_buf_len);

   return (descriptor_t *)cad;
}

int ca_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len)
{
   int bytes = 0;
   if (desc == NULL) return 0;
   if (desc->tag != CA_DESCRIPTOR) return 0;

   ca_descriptor_t *cad = (ca_descriptor_t *)desc;


   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "CA_descriptor", str_len - bytes);
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes);

   bytes += SKIT_LOG_UINT(str + bytes, level, cad->CA_PID, str_len - bytes);
   bytes += SKIT_LOG_UINT(str + bytes, level, cad->CA_system_ID, str_len - bytes);
   return bytes;
}

descriptor_t* max_bitrate_descriptor_new(descriptor_t *desc)
{
   max_bitrate_descriptor_t *maxbr = NULL;
   maxbr = (max_bitrate_descriptor_t *)calloc(1, sizeof(max_bitrate_descriptor_t));
   maxbr->descriptor.tag = MAXIMUM_BITRATE_DESCRIPTOR;
   if (desc != NULL)
   {
      maxbr->descriptor.length = desc->length;
      free(desc);
   }
   return (descriptor_t *)maxbr;
}

int max_bitrate_descriptor_free(descriptor_t *desc)
{
   if (desc == NULL) return 0;
   if (desc->tag != MAXIMUM_BITRATE_DESCRIPTOR) return 0;

   max_bitrate_descriptor_t *maxbr = (max_bitrate_descriptor_t *)desc;
   free(maxbr);
   return 1;
}

descriptor_t* max_bitrate_descriptor_read(descriptor_t *desc, bs_t *b)
{
   if ((desc == NULL) || (b == NULL)) return NULL;

   max_bitrate_descriptor_t *maxbr =
         (max_bitrate_descriptor_t *)max_bitrate_descriptor_new(desc);


   bs_skip_u(b, 2);
   maxbr->max_bitrate = bs_read_u(b, 22);

   return (descriptor_t *)maxbr;
}

int max_bitrate_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len)
{
   int bytes = 0;
   if (desc == NULL) return 0;
   if (desc->tag != MAXIMUM_BITRATE_DESCRIPTOR) return 0;

   max_bitrate_descriptor_t *maxbr = (max_bitrate_descriptor_t *)desc;

   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "max_bitrate_descriptor", str_len - bytes);
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes);

   bytes += SKIT_LOG_UINT(str + bytes, level, maxbr->max_bitrate, str_len - bytes);
   return bytes;
}

/*
descriptor_tag (8)
descriptor_length	(8)
sample_rate_code	(3)
bsid	(5)
bit_rate_code	(6)
surround_mode	(2)
bsmod	(3)
num_channels	(4)
full_svc	(1)
langcod	(8)
if num_channels==0
  langcod2	(8)
if bsmod<2
  mainid	(3)	
  priority	(2)
  reserved	(3)
else
  asvcflags	(8)
end if
textlen	(7)	//'0000000'
text_code	(1)	//'0'
language_flag	(1)
language_flag_2	(1)  //	'0'
reserved	(6)	// '111111'
if language_flag == 1
  language	(3*8)
*/

descriptor_t* ac3_descriptor_new(descriptor_t *desc) 
{ 
   ac3_descriptor_t *ld = NULL; 
   if (desc == NULL) 
   {
      ld = (ac3_descriptor_t *)calloc(1, sizeof(ac3_descriptor_t)); 
      ld->descriptor.tag = AC3_DESCRIPTOR;
   } 
   else 
   {
      ld = realloc(desc, sizeof(ac3_descriptor_t)); 
   }
   return (descriptor_t *)ld;
}

int ac3_descriptor_free(descriptor_t *desc)
{ 
   if (desc == NULL) return 0;
   if (desc->tag != AC3_DESCRIPTOR) return 0;

   ac3_descriptor_t *ld = (ac3_descriptor_t *)desc;

   free(ld);
   return 1;
}

descriptor_t* ac3_descriptor_read(descriptor_t *desc, bs_t *b) 
{ 
   if ((desc == NULL) || (b == NULL)) 
   {
      SAFE_REPORT_TS_ERR(-1); 
      return NULL;
   }
   if (desc->tag != AC3_DESCRIPTOR) 
   {
      LOG_ERROR_ARGS("AC3 descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, AC3_DESCRIPTOR); 
      reportAddErrorLogArgs("AC3 descriptor has tag 0x%02X instead of expected 0x%02X", 
                     desc->tag, AC3_DESCRIPTOR); 
      SAFE_REPORT_TS_ERR(-50); 
      return NULL;
   }
   
   ac3_descriptor_t *ac3d = (ac3_descriptor_t *)calloc(1, sizeof(ac3_descriptor_t)); 
   
   ac3d->descriptor.tag = desc->tag; 
   ac3d->descriptor.length = desc->length; 

   ac3d->sample_rate_code = bs_read_u(b, 3);
   ac3d->bsid = bs_read_u(b, 5);
   ac3d->bit_rate_code = bs_read_u(b, 6);
   ac3d->surround_mode = bs_read_u(b, 2);
   ac3d->bsmod = bs_read_u(b, 3);
   ac3d->num_channels = bs_read_u(b, 4);
   ac3d->full_svc = bs_read_u(b, 1);
   ac3d->langcod = bs_read_u8(b);

   if (ac3d->num_channels==0)   ac3d->langcod2 = bs_read_u8(b);

   if (ac3d->bsmod<2)
   {
      ac3d->mainid = bs_read_u(b, 3);
      ac3d->priority = bs_read_u(b, 2);
      bs_read_u(b, 3);  // reserved -- discard
   }
   else
   {
      ac3d->asvcflags = bs_read_u8(b);
   }

   ac3d->textlen = bs_read_u(b, 7);
   ac3d->text_code = bs_read_u(b, 1);
   ac3d->language_flag = bs_read_u(b, 1);
   ac3d->language_flag_2 = bs_read_u(b, 1);
   bs_read_u(b, 6);  // reserved -- discard

   if (ac3d->language_flag == 1)
   {
      ac3d->language[0] = bs_read_u8(b); 
      ac3d->language[1] = bs_read_u8(b); 
      ac3d->language[2] = bs_read_u8(b); 
      ac3d->language[3] = 0; 
   }

   free(desc); 
   return (descriptor_t *)ac3d;
}

int ac3_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len) 
{ 
   int bytes = 0; 
   if (desc == NULL) return 0; 
   if (desc->tag != AC3_DESCRIPTOR) return 0; 
   
   ac3_descriptor_t *ld = (ac3_descriptor_t *)desc; 

   // GORP: fill in
   
/*   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "ISO_639_language_descriptor", str_len - bytes); 
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes); 
   
   if (ld->num_languages > 0) 
   {
      for (int i = 0; i < ld->num_languages; i++) 
      {
         bytes += SKIT_LOG_STR(str + bytes, level, strtol(ld->languages[i].ISO_639_language_code, NULL, 10), str_len - bytes); 
         bytes += SKIT_LOG_UINT(str + bytes, level, ld->languages[i].audio_type, str_len - bytes);
      }
   }
   */
   
   return bytes;
}


void init_descriptors()
{
   if (g_descriptor_table != NULL)
      return;

   g_descriptor_table =
         hashtable_new(hashtable_hashfn_uint32, hashtable_eqfn_uint32);

   // Register our known descriptors
   descriptor_table_entry_t *d;

   // ISO 639 language
   d = calloc(1, sizeof(descriptor_table_entry_t));
   d->tag = ISO_639_LANGUAGE_DESCRIPTOR;
   d->free_descriptor = language_descriptor_free;
   d->print_descriptor = language_descriptor_print;
   d->read_descriptor = language_descriptor_read;
   register_descriptor(d);

   // Component Name Descriptor
   d = calloc(1, sizeof(descriptor_table_entry_t));
   d->tag = COMPONENT_NAME_DESCRIPTOR;
   d->free_descriptor = component_name_descriptor_free;
   d->print_descriptor = component_name_descriptor_print;
   d->read_descriptor = component_name_descriptor_read;
   register_descriptor(d);

   // Component Name Descriptor
   d = calloc(1, sizeof(descriptor_table_entry_t));
   d->tag = AC3_DESCRIPTOR;
   d->free_descriptor = ac3_descriptor_free;
   d->print_descriptor = ac3_descriptor_print;
   d->read_descriptor = ac3_descriptor_read;
   register_descriptor(d);

   // CA descriptor
   d = calloc(1, sizeof(descriptor_table_entry_t));
   d->tag = CA_DESCRIPTOR;
   d->free_descriptor = ca_descriptor_free;
   d->print_descriptor = ca_descriptor_print;
   d->read_descriptor = ca_descriptor_read;
   register_descriptor(d);

   // Max bitrate descriptor
   d = calloc(1, sizeof(descriptor_table_entry_t));
   d->tag = MAXIMUM_BITRATE_DESCRIPTOR;
   d->free_descriptor = max_bitrate_descriptor_free;
   d->print_descriptor = max_bitrate_descriptor_print;
   d->read_descriptor = max_bitrate_descriptor_read;
   register_descriptor(d);
}

/*
 2, video_stream_descriptor
 3, audio_stream_descriptor
 4, hierarchy_descriptor
 5, registration_descriptor
 6, data_stream_alignment_descriptor
 7, target_background_grid_descriptor
 8, video_window_descriptor
 9, CA_descriptor
 10, ISO_639_language_descriptor
 11, system_clock_descriptor
 12, multiplex_buffer_utilization_descriptor
 13, copyright_descriptor
 14,maximum_bitrate_descriptor
 15,private_data_indicator_descriptor
 16,smoothing_buffer_descriptor
 17,STD_descriptor
 18,IBP_descriptor
 // 19-26 Defined in ISO/IEC 13818-6
 27,MPEG-4_video_descriptor
 28,MPEG-4_audio_descriptor
 29,IOD_descriptor
 30,SL_descriptor
 31,FMC_descriptor
 32,external_ES_ID_descriptor
 33,MuxCode_descriptor
 34,FmxBufferSize_descriptor
 35,multiplexbuffer_descriptor
 36,content_labeling_descriptor
 37,metadata_pointer_descriptor
 38,metadata_descriptor
 39,metadata_STD_descriptor
 40,AVC video descriptor
 41,IPMP_descriptor
 42,AVC timing and HRD descriptor
 43,MPEG-2_AAC_audio_descriptor
 44,FlexMuxTiming_descriptor
 45,MPEG-4_text_descriptor
 46,MPEG-4_audio_extension_descriptor
 47,auxiliary_video_stream_descriptor
 48,SVC extension descriptor
 49,MVC extension descriptor
 50,J2K video descriptor
 51,MVC operation point descriptor
 52,MPEG2_stereoscopic_video_format_descriptor
 53,Stereoscopic_program_info_descriptor
 54,Stereoscopic_video_info_descriptor
 */
