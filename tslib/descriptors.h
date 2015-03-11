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

#ifndef _TSLIB_DESCRIPTORS_H_
#define _TSLIB_DESCRIPTORS_H_        

#include <stdint.h>

#include "bs.h"
#include "common.h"
#include "vqarray.h"
#include "log.h"

typedef enum {
	DESCRIPTOR_TAG_RESERVED = 0,
	DESCRIPTOR_TAG_FORBIDDEN,
    VIDEO_STREAM_DESCRIPTOR,
    AUDIO_STREAM_DESCRIPTOR,
    HIERARCHY_DESCRIPTOR,
    REGISTRATION_DESCRIPTOR,
    DATA_STREAM_ALIGNMENT_DESCRIPTOR,
    TARGET_BACKGROUND_GRID_DESCRIPTOR, 
    VIDEO_WINDOW_DESCRIPTOR, 
    CA_DESCRIPTOR, 
    ISO_639_LANGUAGE_DESCRIPTOR,
    SYSTEM_CLOCK_DESCRIPTOR,
    MULTIPLEX_BUFFER_UTILIZATION_DESCRIPTOR,
    COPYRIGHT_DESCRIPTOR,
    MAXIMUM_BITRATE_DESCRIPTOR,
    PRIVATE_DATA_INDICATOR_DESCRIPTOR,
    SMOOTHING_BUFFER_DESCRIPTOR,
    STD_DESCRIPTOR,
    IBP_DESCRIPTOR,
    MPEG4_VIDEO_DESCRIPTOR = 27,
    MPEG4_AUDIO_DESCRIPTOR,
    IOD_DESCRIPTOR,
    SL_DESCRIPTOR,
    FMC_DESCRIPTOR,
    EXTERNAL_ES_ID_DESCRIPTOR,
    MUXCODE_DESCRIPTOR,
    FMXBUFFERSIZE_DESCRIPTOR,
    MULTIPLEXBUFFER_DESCRIPTOR,
    CONTENT_LABELING_DESCRIPTOR,
    METADATA_POINTER_DESCRIPTOR,
    METADATA_DESCRIPTOR,
    METADATA_STD_DESCRIPTOR,
    AVC_VIDEO_DESCRIPTOR,
    IPMP_DESCRIPTOR,
    AVC_TIMING_HRD_DESCRIPTOR,
    MPEG2_AAC_AUDIO_DESCRIPTOR,
    FLEXMUX_TIMING_DESCRIPTOR,
    MPEG4_TEXT_DESCRIPTOR,
    MPEG4_AUDIO_EXTENSION_DESCRIPTOR,
    AUXILIARY_VIDEO_STREAM_DESCRIPTOR,
    SVC_EXTENSION_DESCRIPTOR,
    MVC_EXTENSION_DESCRIPTOR,
    J2K_VIDEO_DESCRIPTOR,
    MVC_OPERATION_POINT_DESCRIPTOR,
    MPEG2_STEREOSCOPIC_VIDEO_FORMAT_DESCRIPTOR,
    STEREOSCOPIC_PROGRAM_INFO_DESCRIPTOR,
    STEREOSCOPIC_VIDEO_INFO_DESCRIPTOR,
    // end of descriptors defined in ISO/IEC 13818-1:2012

    ADAPTATION_FIELD_DATA_DESCRIPTOR = 0x97,
    // end of SCTE 128 descriptors

    AC3_DESCRIPTOR = 0x81,
    COMPONENT_NAME_DESCRIPTOR = 0xA3,

} mpeg_descriptor_t;

typedef struct {
   uint32_t tag;
   uint32_t length;
} descriptor_t;

typedef descriptor_t* (*descriptor_reader_t)(descriptor_t*, bs_t *);
typedef int (*descriptor_printer_t)(const descriptor_t *, int, char *, size_t);
typedef int (*descriptor_destructor_t)(descriptor_t*);

typedef struct {
	uint32_t tag;
   descriptor_reader_t read_descriptor;
   descriptor_printer_t print_descriptor;
   descriptor_destructor_t free_descriptor;
} descriptor_table_entry_t; 

// Must be called to initialize known descriptors and the descriptor
// registration system
void init_descriptors();

// Register a new descriptor to be parsed by the system
int register_descriptor(descriptor_table_entry_t *desc);

// "factory methods"
int read_descriptor_loop(vqarray_t *desc_list, bs_t *b, int length);
int write_descriptor_loop(vqarray_t *desc_list, bs_t *b);
int print_descriptor_loop(vqarray_t *desc_list, int level, char *str, size_t str_len);


descriptor_t* descriptor_new();
void descriptor_free(descriptor_t* desc);
descriptor_t* descriptor_read(descriptor_t* desc, bs_t* b);
int descriptor_print(const descriptor_t* desc, int level, char* str, size_t str_len);

typedef struct {
	char ISO_639_language_code[4];
	uint32_t audio_type;
} iso639_lang_t;

typedef struct {
	descriptor_t descriptor;
	iso639_lang_t* languages;
	int num_languages;
} language_descriptor_t;

//descriptor_t* language_descriptor_new(descriptor_t* desc);
//int language_descriptor_free(descriptor_t* desc);
//descriptor_t* language_descriptor_read(descriptor_t* desc, bs_t* b);
//int language_descriptor_print(const descriptor_t* desc, int level, char* str, size_t str_len);

typedef struct {
	descriptor_t descriptor;
	int num_names;
	iso639_lang_t* languages;
	char **names;
} component_name_descriptor_t;

typedef struct {
	descriptor_t descriptor;

   uint8_t sample_rate_code;
   uint8_t bsid;
   uint8_t bit_rate_code;
   uint8_t surround_mode;
   uint8_t bsmod;
   uint8_t num_channels;
   uint8_t full_svc;
   uint8_t langcod;
   uint8_t langcod2;
   uint8_t mainid;
   uint8_t priority;
   uint8_t asvcflags;
   uint8_t textlen;
   uint8_t text_code;
   uint8_t language_flag;
   uint8_t language_flag_2;
	char language[4];
	char language_2[4];

} ac3_descriptor_t;


typedef struct {
   descriptor_t descriptor;
   uint32_t CA_system_ID;
   uint32_t CA_PID;
   uint8_t *private_data_bytes;
   size_t _private_data_bytes_buf_len;
} ca_descriptor_t;

//descriptor_t* ca_descriptor_new(descriptor_t *desc);
//int ca_descriptor_free(descriptor_t *desc);
//descriptor_t* ca_descriptor_read(descriptor_t *desc, bs_t *b);
//int ca_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len);


typedef struct {
   descriptor_t descriptor;
   uint32_t max_bitrate;
} max_bitrate_descriptor_t;

//descriptor_t* max_bitrate_descriptor_new(descriptor_t *desc);
//int max_bitrate_descriptor_free(descriptor_t *desc);
//descriptor_t* max_bitrate_descriptor_read(descriptor_t *desc, bs_t *b);
//int max_bitrate_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len);



#endif
