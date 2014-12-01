#ifndef EBP_H_
#define EBP_H_

#include <stdint.h>
#include <vqarray.h>
#include <ts.h>
#include <descriptors.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define EBP_PARTITION_SEGMENT    1
#define EBP_PARTITION_FRAGMENT   2

typedef struct {

   uint8_t ebp_fragment_flag;
   uint8_t ebp_segment_flag;
   uint8_t ebp_sap_flag;
   uint8_t ebp_grouping_flag;
   uint8_t ebp_time_flag;
   uint8_t ebp_concealment_flag;
   uint8_t ebp_extension_flag;

   uint8_t ebp_ext_partition_flag;
   uint8_t ebp_sap_type;

   vqarray_t *ebp_grouping_ids; // array of uint8_t* grouping IDs

   uint64_t ebp_acquisition_time;

   uint8_t ebp_ext_partitions;

} ebp_t;

ebp_t *ebp_new();
void ebp_free(ebp_t *ebp);
int ebp_read(ebp_t *ebp, ts_scte128_private_data_t *scte128);
int ebp_print(const ebp_t *ebp, char *str, size_t str_len);
void ebp_print_stdout(const ebp_t *ebp);
int ebp_validate_groups(const ebp_t *ebp);
ebp_t* ebp_copy(const ebp_t *ebp);
void parseNTPTimestamp(uint64_t ntpTime, uint32_t *numSeconds, float *fractionalSecond);

typedef struct {

   uint8_t ebp_data_explicit_flag;
   uint8_t representation_id_flag;
   uint8_t partition_id;

   uint16_t ebp_pid;

   uint8_t boundary_flag;
   uint32_t ebp_distance;

   uint8_t sap_type_max;
   uint8_t acquisition_time_flag;

   uint64_t representation_id;

} ebp_partition_data_t;

typedef struct {
   descriptor_t descriptor;

   uint8_t num_partitions;

   uint8_t timescale_flag;

   uint32_t ticks_per_second;
   uint8_t ebp_distance_width_minus_1;

   vqarray_t *partition_data; // Array of ebp_partition_data_t

} ebp_descriptor_t;

#define EBP_DESCRIPTOR 0xE9

int ebp_descriptor_free(descriptor_t *desc);
descriptor_t* ebp_descriptor_read(descriptor_t *desc, bs_t *b);
int ebp_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len);
void ebp_descriptor_print_stdout(const ebp_descriptor_t *ebp_desc);
ebp_descriptor_t* ebp_descriptor_copy(const ebp_descriptor_t *ebp_desc);

int does_fragment_mark_boundary (const ebp_descriptor_t *ebp_desc);
int does_segment_mark_boundary (const ebp_descriptor_t *ebp_desc);

ebp_partition_data_t* get_fragment_partition (const ebp_descriptor_t *ebp_desc);
ebp_partition_data_t* get_segment_partition (const ebp_descriptor_t *ebp_desc);

uint8_t get_fragment_SAP_max (const ebp_descriptor_t *ebp_desc);
uint8_t get_segment_SAP_max (const ebp_descriptor_t *ebp_desc);



uint64_t ntohll(uint64_t num);


#ifdef __cplusplus
}
#endif

#endif /* EBP_H_ */
