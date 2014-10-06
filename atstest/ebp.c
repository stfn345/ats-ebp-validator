
#include <ebp.h>
#include <bs.h>

ebp_t *ebp_new()
{
   return calloc(1, sizeof(ebp_t));
}

void ebp_free(ebp_t *ebp)
{
   if (ebp != NULL)
   {
      free(ebp);
   }
}

int ebp_read(ebp_t *ebp, ts_scte128_private_data_t *scte128)
{
   if (ebp == NULL || scte128 == NULL)
   {
      return 0;
   }

   bs_t *b = bs_new(scte128->private_data_bytes.bytes, scte128->private_data_bytes.len);

   ebp->ebp_fragment_flag = bs_read_u1(b);
   ebp->ebp_segment_flag = bs_read_u1(b);
   ebp->ebp_sap_flag = bs_read_u1(b);
   ebp->ebp_grouping_flag = bs_read_u1(b);
   ebp->ebp_time_flag = bs_read_u1(b);
   ebp->ebp_concealment_flag = bs_read_u1(b);

   bs_skip_u1(b);

   ebp->ebp_extension_flag = bs_read_u1(b);

   if (ebp->ebp_extension_flag)
   {
      ebp->ebp_ext_partition_flag = bs_read_u1(b);
      bs_skip_u(b,7);
   }

   if (ebp->ebp_sap_flag)
   {
      ebp->ebp_sap_type = bs_read_u(b, 3);
      bs_skip_u(b, 5);
   }

   if (ebp->ebp_grouping_flag)
   {
      ebp->ebp_grouping_ids = vqarray_new();
      uint32_t more = 1;
      while (more)
      {
         more = bs_read_u1(b);;
         uint32_t *grouping_id = calloc(1, sizeof(uint32_t));
         vqarray_add(ebp->ebp_grouping_ids, (vqarray_elem_t*)grouping_id);
      }

   }

   if (ebp->ebp_time_flag)
   {
      ebp->ebp_acquisition_time = bs_read_u64(b);
   }

   if (ebp->ebp_ext_partition_flag)
   {
      ebp->ebp_ext_partitions = bs_read_u8(b);
   }

   return 1;
}

int ebp_print(const ebp_t *ebp, char *str, size_t str_len)
{
   return 1;
}

static descriptor_t* ebp_descriptor_new(descriptor_t *desc)
{
   ebp_descriptor_t *ebp = NULL;
   ebp = (ebp_descriptor_t *)calloc(1, sizeof(ebp_descriptor_t));
   ebp->descriptor.tag = EBP_DESCRIPTOR;
   if (desc != NULL)
   {
      ebp->descriptor.length = desc->length;
      free(desc);
   }
   return (descriptor_t *)ebp;
}

int ebp_descriptor_free(descriptor_t *desc)
{
   if (desc == NULL) return 0;
   if (desc->tag != EBP_DESCRIPTOR) return 0;

   ebp_descriptor_t *ebp = (ebp_descriptor_t *)desc;
   free(ebp);
   return 1;
}

descriptor_t* ebp_descriptor_read(descriptor_t *desc, bs_t *b)
{
   if ((desc == NULL) || (b == NULL)) return NULL;

   ebp_descriptor_t *ebp =
         (ebp_descriptor_t *)ebp_descriptor_new(desc);

   ebp->num_partitions = bs_read_u(b, 5);
   ebp->timescale_flag = bs_read_u1(b);
   bs_skip_u(b, 2);

   ebp->ticks_per_second = (ebp->timescale_flag) ? bs_read_u(b, 22) : 1;
   ebp->ebp_distance_width_minus_1 = (ebp->timescale_flag) ? bs_read_u(b, 3) : 0;

   if (ebp->num_partitions > 0)
   {
      int i = ebp->num_partitions;
      ebp->partition_data = vqarray_new();
      while (i-- > 0)
      {
         ebp_partition_data_t *partition_data = calloc(1, sizeof(ebp_partition_data_t));
         partition_data->ebp_data_explicit_flag = bs_read_u1(b);
         partition_data->representation_id_flag = bs_read_u1(b);
         partition_data->partition_id = bs_read_u(b, 5);

         if (partition_data->ebp_data_explicit_flag == 0)
         {
            bs_skip_u1(b);
            partition_data->ebp_pid = bs_read_u(b, 13);
            bs_skip_u(b, 3);
         }
         else
         {
            int ebp_distance_width = (ebp->ebp_distance_width_minus_1 + 1) * 8;
            partition_data->boundary_flag = bs_read_u1(b);
            partition_data->ebp_distance = bs_read_u(b, ebp_distance_width);

            if (partition_data->boundary_flag == 1)
            {
               partition_data->sap_type_max = bs_read_u(b, 3);
               bs_skip_u(b, 4);
            }
            else
            {
               bs_skip_u(b, 7);
            }

            partition_data->acquisition_time_flag = bs_read_u1(b);
         }

         if (partition_data->representation_id_flag == 1)
         {
            partition_data->representation_id = bs_read_u64(b);
         }
      }
   }
   return (descriptor_t *)ebp;
}

int ebp_descriptor_print(const descriptor_t *desc, int level, char *str, size_t str_len)
{
   int bytes = 0;
   if (desc == NULL) return 0;
   if (desc->tag != EBP_DESCRIPTOR) return 0;

   ebp_descriptor_t *maxbr = (ebp_descriptor_t *)desc;

   bytes += SKIT_LOG_UINT_VERBOSE(str + bytes, level, desc->tag, "ebp_descriptor", str_len - bytes);
   bytes += SKIT_LOG_UINT(str + bytes, level, desc->length, str_len - bytes);

   //bytes += SKIT_LOG_UINT(str + bytes, level, maxbr->max_bitrate, str_len - bytes);
   return bytes;
}

