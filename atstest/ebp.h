#ifndef EBP_H_
#define EBP_H_

#include <stdint.h>
#include <vqarray.h>
#include <ts.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* EBP_H_ */
