/* udf.c - Universal Disk Format filesystem.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/err.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/dl.h>
#include <grub/types.h>
#include <grub/fshelp.h>
#include <grub/charset.h>
#include <grub/datetime.h>
#include <grub/udf.h>
#include <grub/ventoy.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define OFFSET_OF(TYPE, MEMBER) ((grub_size_t) &((TYPE *)0)->MEMBER)

grub_uint32_t g_last_disk_read_sector = 0;
grub_uint32_t g_last_fe_tag_ident = 0;
grub_uint32_t g_last_icb_read_sector = 0;
grub_uint32_t g_last_icb_read_sector_tag_ident = 0;
grub_uint32_t g_last_fileattr_read_sector = 0;
grub_uint32_t g_last_fileattr_read_sector_tag_ident = 0;
grub_uint32_t g_last_fileattr_offset = 0;
grub_uint64_t g_last_pd_length_offset = 0;

#define GRUB_UDF_MAX_PDS		2
#define GRUB_UDF_MAX_PMS		6

#define U16				grub_le_to_cpu16
#define U32				grub_le_to_cpu32
#define U64				grub_le_to_cpu64

#define GRUB_UDF_TAG_IDENT_PVD		0x0001
#define GRUB_UDF_TAG_IDENT_AVDP		0x0002
#define GRUB_UDF_TAG_IDENT_VDP		0x0003
#define GRUB_UDF_TAG_IDENT_IUVD		0x0004
#define GRUB_UDF_TAG_IDENT_PD		0x0005
#define GRUB_UDF_TAG_IDENT_LVD		0x0006
#define GRUB_UDF_TAG_IDENT_USD		0x0007
#define GRUB_UDF_TAG_IDENT_TD		0x0008
#define GRUB_UDF_TAG_IDENT_LVID		0x0009

#define GRUB_UDF_TAG_IDENT_FSD		0x0100
#define GRUB_UDF_TAG_IDENT_FID		0x0101
#define GRUB_UDF_TAG_IDENT_AED		0x0102
#define GRUB_UDF_TAG_IDENT_IE		0x0103
#define GRUB_UDF_TAG_IDENT_TE		0x0104
#define GRUB_UDF_TAG_IDENT_FE		0x0105
#define GRUB_UDF_TAG_IDENT_EAHD		0x0106
#define GRUB_UDF_TAG_IDENT_USE		0x0107
#define GRUB_UDF_TAG_IDENT_SBD		0x0108
#define GRUB_UDF_TAG_IDENT_PIE		0x0109
#define GRUB_UDF_TAG_IDENT_EFE		0x010A

#define GRUB_UDF_ICBTAG_TYPE_UNDEF	0x00
#define GRUB_UDF_ICBTAG_TYPE_USE	0x01
#define GRUB_UDF_ICBTAG_TYPE_PIE	0x02
#define GRUB_UDF_ICBTAG_TYPE_IE		0x03
#define GRUB_UDF_ICBTAG_TYPE_DIRECTORY	0x04
#define GRUB_UDF_ICBTAG_TYPE_REGULAR	0x05
#define GRUB_UDF_ICBTAG_TYPE_BLOCK	0x06
#define GRUB_UDF_ICBTAG_TYPE_CHAR	0x07
#define GRUB_UDF_ICBTAG_TYPE_EA		0x08
#define GRUB_UDF_ICBTAG_TYPE_FIFO	0x09
#define GRUB_UDF_ICBTAG_TYPE_SOCKET	0x0A
#define GRUB_UDF_ICBTAG_TYPE_TE		0x0B
#define GRUB_UDF_ICBTAG_TYPE_SYMLINK	0x0C
#define GRUB_UDF_ICBTAG_TYPE_STREAMDIR	0x0D

#define GRUB_UDF_ICBTAG_FLAG_AD_MASK	0x0007
#define GRUB_UDF_ICBTAG_FLAG_AD_SHORT	0x0000
#define GRUB_UDF_ICBTAG_FLAG_AD_LONG	0x0001
#define GRUB_UDF_ICBTAG_FLAG_AD_EXT	0x0002
#define GRUB_UDF_ICBTAG_FLAG_AD_IN_ICB	0x0003

#define GRUB_UDF_EXT_NORMAL		0x00000000
#define GRUB_UDF_EXT_NREC_ALLOC		0x40000000
#define GRUB_UDF_EXT_NREC_NALLOC	0x80000000
#define GRUB_UDF_EXT_MASK		0xC0000000

#define GRUB_UDF_FID_CHAR_HIDDEN	0x01
#define GRUB_UDF_FID_CHAR_DIRECTORY	0x02
#define GRUB_UDF_FID_CHAR_DELETED	0x04
#define GRUB_UDF_FID_CHAR_PARENT	0x08
#define GRUB_UDF_FID_CHAR_METADATA	0x10

#define GRUB_UDF_STD_IDENT_BEA01	"BEA01"
#define GRUB_UDF_STD_IDENT_BOOT2	"BOOT2"
#define GRUB_UDF_STD_IDENT_CD001	"CD001"
#define GRUB_UDF_STD_IDENT_CDW02	"CDW02"
#define GRUB_UDF_STD_IDENT_NSR02	"NSR02"
#define GRUB_UDF_STD_IDENT_NSR03	"NSR03"
#define GRUB_UDF_STD_IDENT_TEA01	"TEA01"

#define GRUB_UDF_CHARSPEC_TYPE_CS0	0x00
#define GRUB_UDF_CHARSPEC_TYPE_CS1	0x01
#define GRUB_UDF_CHARSPEC_TYPE_CS2	0x02
#define GRUB_UDF_CHARSPEC_TYPE_CS3	0x03
#define GRUB_UDF_CHARSPEC_TYPE_CS4	0x04
#define GRUB_UDF_CHARSPEC_TYPE_CS5	0x05
#define GRUB_UDF_CHARSPEC_TYPE_CS6	0x06
#define GRUB_UDF_CHARSPEC_TYPE_CS7	0x07
#define GRUB_UDF_CHARSPEC_TYPE_CS8	0x08

#define GRUB_UDF_PARTMAP_TYPE_1		1
#define GRUB_UDF_PARTMAP_TYPE_2		2

struct grub_udf_lb_addr
{
  grub_uint32_t block_num;
  grub_uint16_t part_ref;
} GRUB_PACKED;

struct grub_udf_short_ad
{
  grub_uint32_t length;
  grub_uint32_t position;
} GRUB_PACKED;

struct grub_udf_long_ad
{
  grub_uint32_t length;
  struct grub_udf_lb_addr block;
  grub_uint8_t imp_use[6];
} GRUB_PACKED;

struct grub_udf_extent_ad
{
  grub_uint32_t length;
  grub_uint32_t start;
} GRUB_PACKED;

struct grub_udf_charspec
{
  grub_uint8_t charset_type;
  grub_uint8_t charset_info[63];
} GRUB_PACKED;

struct grub_udf_timestamp
{
  grub_uint16_t type_and_timezone;
  grub_uint16_t year;
  grub_uint8_t month;
  grub_uint8_t day;
  grub_uint8_t hour;
  grub_uint8_t minute;
  grub_uint8_t second;
  grub_uint8_t centi_seconds;
  grub_uint8_t hundreds_of_micro_seconds;
  grub_uint8_t micro_seconds;
} GRUB_PACKED;

struct grub_udf_regid
{
  grub_uint8_t flags;
  grub_uint8_t ident[23];
  grub_uint8_t ident_suffix[8];
} GRUB_PACKED;

struct grub_udf_tag
{
  grub_uint16_t tag_ident;
  grub_uint16_t desc_version;
  grub_uint8_t tag_checksum;
  grub_uint8_t reserved;
  grub_uint16_t tag_serial_number;
  grub_uint16_t desc_crc;
  grub_uint16_t desc_crc_length;
  grub_uint32_t tag_location;
} GRUB_PACKED;

struct grub_udf_fileset
{
  struct grub_udf_tag tag;
  struct grub_udf_timestamp datetime;
  grub_uint16_t interchange_level;
  grub_uint16_t max_interchange_level;
  grub_uint32_t charset_list;
  grub_uint32_t max_charset_list;
  grub_uint32_t fileset_num;
  grub_uint32_t fileset_desc_num;
  struct grub_udf_charspec vol_charset;
  grub_uint8_t vol_ident[128];
  struct grub_udf_charspec fileset_charset;
  grub_uint8_t fileset_ident[32];
  grub_uint8_t copyright_file_ident[32];
  grub_uint8_t abstract_file_ident[32];
  struct grub_udf_long_ad root_icb;
  struct grub_udf_regid domain_ident;
  struct grub_udf_long_ad next_ext;
  struct grub_udf_long_ad streamdir_icb;
} GRUB_PACKED;

struct grub_udf_icbtag
{
  grub_uint32_t prior_recorded_num_direct_entries;
  grub_uint16_t strategy_type;
  grub_uint16_t strategy_parameter;
  grub_uint16_t num_entries;
  grub_uint8_t reserved;
  grub_uint8_t file_type;
  struct grub_udf_lb_addr parent_idb;
  grub_uint16_t flags;
} GRUB_PACKED;

struct grub_udf_file_ident
{
  struct grub_udf_tag tag;
  grub_uint16_t version_num;
  grub_uint8_t characteristics;
#define MAX_FILE_IDENT_LENGTH 256
  grub_uint8_t file_ident_length;
  struct grub_udf_long_ad icb;
  grub_uint16_t imp_use_length;
} GRUB_PACKED;

struct grub_udf_file_entry
{
  struct grub_udf_tag tag;
  struct grub_udf_icbtag icbtag;
  grub_uint32_t uid;
  grub_uint32_t gid;
  grub_uint32_t permissions;
  grub_uint16_t link_count;
  grub_uint8_t record_format;
  grub_uint8_t record_display_attr;
  grub_uint32_t record_length;
  grub_uint64_t file_size;
  grub_uint64_t blocks_recorded;
  struct grub_udf_timestamp access_time;
  struct grub_udf_timestamp modification_time;
  struct grub_udf_timestamp attr_time;
  grub_uint32_t checkpoint;
  struct grub_udf_long_ad extended_attr_idb;
  struct grub_udf_regid imp_ident;
  grub_uint64_t unique_id;
  grub_uint32_t ext_attr_length;
  grub_uint32_t alloc_descs_length;
  grub_uint8_t ext_attr[0];
} GRUB_PACKED;

struct grub_udf_extended_file_entry
{
  struct grub_udf_tag tag;
  struct grub_udf_icbtag icbtag;
  grub_uint32_t uid;
  grub_uint32_t gid;
  grub_uint32_t permissions;
  grub_uint16_t link_count;
  grub_uint8_t record_format;
  grub_uint8_t record_display_attr;
  grub_uint32_t record_length;
  grub_uint64_t file_size;
  grub_uint64_t object_size;
  grub_uint64_t blocks_recorded;
  struct grub_udf_timestamp access_time;
  struct grub_udf_timestamp modification_time;
  struct grub_udf_timestamp create_time;
  struct grub_udf_timestamp attr_time;
  grub_uint32_t checkpoint;
  grub_uint32_t reserved;
  struct grub_udf_long_ad extended_attr_icb;
  struct grub_udf_long_ad streamdir_icb;
  struct grub_udf_regid imp_ident;
  grub_uint64_t unique_id;
  grub_uint32_t ext_attr_length;
  grub_uint32_t alloc_descs_length;
  grub_uint8_t ext_attr[0];
} GRUB_PACKED;

struct grub_udf_vrs
{
  grub_uint8_t type;
  grub_uint8_t magic[5];
  grub_uint8_t version;
} GRUB_PACKED;

struct grub_udf_avdp
{
  struct grub_udf_tag tag;
  struct grub_udf_extent_ad vds;
} GRUB_PACKED;

struct grub_udf_pd
{
  struct grub_udf_tag tag;
  grub_uint32_t seq_num;
  grub_uint16_t flags;
  grub_uint16_t part_num;
  struct grub_udf_regid contents;
  grub_uint8_t contents_use[128];
  grub_uint32_t access_type;
  grub_uint32_t start;
  grub_uint32_t length;
} GRUB_PACKED;

struct grub_udf_partmap
{
  grub_uint8_t type;
  grub_uint8_t length;
  union
  {
    struct
    {
      grub_uint16_t seq_num;
      grub_uint16_t part_num;
    } type1;

    struct
    {
      grub_uint8_t ident[62];
    } type2;
  };
} GRUB_PACKED;

struct grub_udf_pvd
{
  struct grub_udf_tag tag;
  grub_uint32_t seq_num;
  grub_uint32_t pvd_num;
  grub_uint8_t ident[32];
  grub_uint16_t vol_seq_num;
  grub_uint16_t max_vol_seq_num;
  grub_uint16_t interchange_level;
  grub_uint16_t max_interchange_level;
  grub_uint32_t charset_list;
  grub_uint32_t max_charset_list;
  grub_uint8_t volset_ident[128];
  struct grub_udf_charspec desc_charset;
  struct grub_udf_charspec expl_charset;
  struct grub_udf_extent_ad vol_abstract;
  struct grub_udf_extent_ad vol_copyright;
  struct grub_udf_regid app_ident;
  struct grub_udf_timestamp recording_time;
  struct grub_udf_regid imp_ident;
  grub_uint8_t imp_use[64];
  grub_uint32_t pred_vds_loc;
  grub_uint16_t flags;
  grub_uint8_t reserved[22];
} GRUB_PACKED;

struct grub_udf_lvd
{
  struct grub_udf_tag tag;
  grub_uint32_t seq_num;
  struct grub_udf_charspec charset;
  grub_uint8_t ident[128];
  grub_uint32_t bsize;
  struct grub_udf_regid domain_ident;
  struct grub_udf_long_ad root_fileset;
  grub_uint32_t map_table_length;
  grub_uint32_t num_part_maps;
  struct grub_udf_regid imp_ident;
  grub_uint8_t imp_use[128];
  struct grub_udf_extent_ad integrity_seq_ext;
  grub_uint8_t part_maps[1608];
} GRUB_PACKED;

struct grub_udf_aed
{
  struct grub_udf_tag tag;
  grub_uint32_t prev_ae;
  grub_uint32_t ae_len;
} GRUB_PACKED;

struct grub_udf_data
{
  grub_disk_t disk;
  struct grub_udf_pvd pvd;
  struct grub_udf_lvd lvd;
  struct grub_udf_pd pds[GRUB_UDF_MAX_PDS];
  struct grub_udf_partmap *pms[GRUB_UDF_MAX_PMS];
  struct grub_udf_long_ad root_icb;
  int npd, npm, lbshift;
};

struct grub_fshelp_node
{
  struct grub_udf_data *data;
  int part_ref;
  union
  {
    struct grub_udf_file_entry fe;
    struct grub_udf_extended_file_entry efe;
    char raw[0];
  } block;
};

static inline grub_size_t
get_fshelp_size (struct grub_udf_data *data)
{
  struct grub_fshelp_node *x = NULL;
  return sizeof (*x)
    + (1 << (GRUB_DISK_SECTOR_BITS
	     + data->lbshift)) - sizeof (x->block);
}

static grub_dl_t my_mod;

static grub_uint32_t
grub_udf_get_block (struct grub_udf_data *data,
		    grub_uint16_t part_ref, grub_uint32_t block)
{
  part_ref = U16 (part_ref);

  if (part_ref >= data->npm)
    {
      grub_error (GRUB_ERR_BAD_FS, "invalid part ref");
      return 0;
    }

  return (U32 (data->pds[data->pms[part_ref]->type1.part_num].start)
          + U32 (block));
}

static grub_err_t
grub_udf_read_icb (struct grub_udf_data *data,
		   struct grub_udf_long_ad *icb,
		   struct grub_fshelp_node *node)
{
  grub_uint32_t block;

  block = grub_udf_get_block (data,
			      icb->block.part_ref,
                              icb->block.block_num);

  if (grub_errno)
    return grub_errno;

  if (grub_disk_read (data->disk, block << data->lbshift, 0,
		      1 << (GRUB_DISK_SECTOR_BITS
			    + data->lbshift),
		      &node->block))
    return grub_errno;

  g_last_disk_read_sector = block;
  g_last_fe_tag_ident = U16(node->block.fe.tag.tag_ident);

  if ((U16 (node->block.fe.tag.tag_ident) != GRUB_UDF_TAG_IDENT_FE) &&
      (U16 (node->block.fe.tag.tag_ident) != GRUB_UDF_TAG_IDENT_EFE))
    return grub_error (GRUB_ERR_BAD_FS, "invalid fe/efe descriptor");

  node->part_ref = icb->block.part_ref;
  node->data = data;
  return 0;
}

static grub_disk_addr_t
grub_udf_read_block (grub_fshelp_node_t node, grub_disk_addr_t fileblock)
{
  char *buf = NULL;
  char *ptr;
  grub_ssize_t len;
  grub_disk_addr_t filebytes;

  switch (U16 (node->block.fe.tag.tag_ident))
    {
    case GRUB_UDF_TAG_IDENT_FE:
      ptr = (char *) &node->block.fe.ext_attr[0] + U32 (node->block.fe.ext_attr_length);
      len = U32 (node->block.fe.alloc_descs_length);
      break;

    case GRUB_UDF_TAG_IDENT_EFE:
      ptr = (char *) &node->block.efe.ext_attr[0] + U32 (node->block.efe.ext_attr_length);
      len = U32 (node->block.efe.alloc_descs_length);
      break;

    default:
      grub_error (GRUB_ERR_BAD_FS, "invalid file entry");
      return 0;
    }

  if ((U16 (node->block.fe.icbtag.flags) & GRUB_UDF_ICBTAG_FLAG_AD_MASK)
      == GRUB_UDF_ICBTAG_FLAG_AD_SHORT)
    {
      struct grub_udf_short_ad *ad = (struct grub_udf_short_ad *) ptr;

      filebytes = fileblock * U32 (node->data->lvd.bsize);
      while (len >= (grub_ssize_t) sizeof (struct grub_udf_short_ad))
	{
	  grub_uint32_t adlen = U32 (ad->length) & 0x3fffffff;
	  grub_uint32_t adtype = U32 (ad->length) >> 30;
	  if (adtype == 3)
	    {
	      struct grub_udf_aed *extension;
	      grub_disk_addr_t sec = grub_udf_get_block(node->data,
							node->part_ref,
							ad->position);
	      if (!buf)
		{
		  buf = grub_malloc (U32 (node->data->lvd.bsize));
		  if (!buf)
		    return 0;
		}
	      if (grub_disk_read (node->data->disk, sec << node->data->lbshift,
				  0, adlen, buf))
		goto fail;

	      extension = (struct grub_udf_aed *) buf;
	      if (U16 (extension->tag.tag_ident) != GRUB_UDF_TAG_IDENT_AED)
		{
		  grub_error (GRUB_ERR_BAD_FS, "invalid aed tag");
		  goto fail;
		}

	      len = U32 (extension->ae_len);
	      ad = (struct grub_udf_short_ad *)
		    (buf + sizeof (struct grub_udf_aed));
	      continue;
	    }

	  if (filebytes < adlen)
	    {
	      grub_uint32_t ad_pos = ad->position;
	      grub_free (buf);
	      return ((U32 (ad_pos) & GRUB_UDF_EXT_MASK) ? 0 :
		      (grub_udf_get_block (node->data, node->part_ref, ad_pos)
		       + (filebytes >> (GRUB_DISK_SECTOR_BITS
					+ node->data->lbshift))));
	    }

	  filebytes -= adlen;
	  ad++;
	  len -= sizeof (struct grub_udf_short_ad);
	}
    }
  else
    {
      struct grub_udf_long_ad *ad = (struct grub_udf_long_ad *) ptr;

      filebytes = fileblock * U32 (node->data->lvd.bsize);
      while (len >= (grub_ssize_t) sizeof (struct grub_udf_long_ad))
	{
	  grub_uint32_t adlen = U32 (ad->length) & 0x3fffffff;
	  grub_uint32_t adtype = U32 (ad->length) >> 30;
	  if (adtype == 3)
	    {
	      struct grub_udf_aed *extension;
	      grub_disk_addr_t sec = grub_udf_get_block(node->data,
							ad->block.part_ref,
							ad->block.block_num);
	      if (!buf)
		{
		  buf = grub_malloc (U32 (node->data->lvd.bsize));
		  if (!buf)
		    return 0;
		}
	      if (grub_disk_read (node->data->disk, sec << node->data->lbshift,
				  0, adlen, buf))
		goto fail;

	      extension = (struct grub_udf_aed *) buf;
	      if (U16 (extension->tag.tag_ident) != GRUB_UDF_TAG_IDENT_AED)
		{
		  grub_error (GRUB_ERR_BAD_FS, "invalid aed tag");
		  goto fail;
		}

	      len = U32 (extension->ae_len);
	      ad = (struct grub_udf_long_ad *)
		    (buf + sizeof (struct grub_udf_aed));
	      continue;
	    }
	      
	  if (filebytes < adlen)
	    {
	      grub_uint32_t ad_block_num = ad->block.block_num;
	      grub_uint32_t ad_part_ref = ad->block.part_ref;
	      grub_free (buf);
	      return ((U32 (ad_block_num) & GRUB_UDF_EXT_MASK) ?  0 :
		      (grub_udf_get_block (node->data, ad_part_ref,
					   ad_block_num)
		       + (filebytes >> (GRUB_DISK_SECTOR_BITS
				        + node->data->lbshift))));
	    }

	  filebytes -= adlen;
	  ad++;
	  len -= sizeof (struct grub_udf_long_ad);
	}
    }

fail:
  grub_free (buf);

  return 0;
}

static grub_ssize_t
grub_udf_read_file (grub_fshelp_node_t node,
		    grub_disk_read_hook_t read_hook, void *read_hook_data,
		    grub_off_t pos, grub_size_t len, char *buf)
{
  switch (U16 (node->block.fe.icbtag.flags) & GRUB_UDF_ICBTAG_FLAG_AD_MASK)
    {
    case GRUB_UDF_ICBTAG_FLAG_AD_IN_ICB:
      {
	char *ptr;

	ptr = ((U16 (node->block.fe.tag.tag_ident) == GRUB_UDF_TAG_IDENT_FE) ?
	       ((char *) &node->block.fe.ext_attr[0]
                + U32 (node->block.fe.ext_attr_length)) :
	       ((char *) &node->block.efe.ext_attr[0]
                + U32 (node->block.efe.ext_attr_length)));

	grub_memcpy (buf, ptr + pos, len);

	return len;
      }

    case GRUB_UDF_ICBTAG_FLAG_AD_EXT:
      grub_error (GRUB_ERR_BAD_FS, "invalid extent type");
      return 0;
    }

  return grub_fshelp_read_file (node->data->disk, node,
				read_hook, read_hook_data,
				pos, len, buf, grub_udf_read_block,
				U64 (node->block.fe.file_size),
				node->data->lbshift, 0);
}

static unsigned sblocklist[] = { 256, 512, 0 };

static struct grub_udf_data *
grub_udf_mount (grub_disk_t disk)
{
  struct grub_udf_data *data = 0;
  struct grub_udf_fileset root_fs;
  unsigned *sblklist;
  grub_uint32_t block, vblock;
  int i, lbshift;

  data = grub_malloc (sizeof (struct grub_udf_data));
  if (!data)
    return 0;

  data->disk = disk;

  /* Search for Anchor Volume Descriptor Pointer (AVDP)
   * and determine logical block size.  */
  block = 0;
  for (lbshift = 0; lbshift < 4; lbshift++)
    {
      for (sblklist = sblocklist; *sblklist; sblklist++)
        {
	  struct grub_udf_avdp avdp;

	  if (grub_disk_read (disk, *sblklist << lbshift, 0,
			      sizeof (struct grub_udf_avdp), &avdp))
	    {
	      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	      goto fail;
	    }

	  if (U16 (avdp.tag.tag_ident) == GRUB_UDF_TAG_IDENT_AVDP &&
	      U32 (avdp.tag.tag_location) == *sblklist)
	    {
	      block = U32 (avdp.vds.start);
	      break;
	    }
	}

      if (block)
	break;
    }

  if (!block)
    {
      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
      goto fail;
    }
  data->lbshift = lbshift;

  /* Search for Volume Recognition Sequence (VRS).  */
  for (vblock = (32767 >> (lbshift + GRUB_DISK_SECTOR_BITS)) + 1;;
       vblock += (2047 >> (lbshift + GRUB_DISK_SECTOR_BITS)) + 1)
    {
      struct grub_udf_vrs vrs;

      if (grub_disk_read (disk, vblock << lbshift, 0,
			  sizeof (struct grub_udf_vrs), &vrs))
	{
	  grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	  goto fail;
	}

      if ((!grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_NSR03, 5)) ||
	  (!grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_NSR02, 5)))
	break;

      if ((grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_BEA01, 5)) &&
	  (grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_BOOT2, 5)) &&
	  (grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_CD001, 5)) &&
	  (grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_CDW02, 5)) &&
	  (grub_memcmp (vrs.magic, GRUB_UDF_STD_IDENT_TEA01, 5)))
	{
	  grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	  goto fail;
	}
    }

  data->npd = data->npm = 0;
  /* Locate Partition Descriptor (PD) and Logical Volume Descriptor (LVD).  */
  while (1)
    {
      struct grub_udf_tag tag;

      if (grub_disk_read (disk, block << lbshift, 0,
			  sizeof (struct grub_udf_tag), &tag))
	{
	  grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	  goto fail;
	}

      tag.tag_ident = U16 (tag.tag_ident);
      if (tag.tag_ident == GRUB_UDF_TAG_IDENT_PVD)
	{
	  if (grub_disk_read (disk, block << lbshift, 0,
			      sizeof (struct grub_udf_pvd),
			      &data->pvd))
	    {
	      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	      goto fail;
	    }
	}
      else if (tag.tag_ident == GRUB_UDF_TAG_IDENT_PD)
	{
	  if (data->npd >= GRUB_UDF_MAX_PDS)
	    {
	      grub_error (GRUB_ERR_BAD_FS, "too many PDs");
	      goto fail;
	    }

	  if (grub_disk_read (disk, block << lbshift, 0,
			      sizeof (struct grub_udf_pd),
			      &data->pds[data->npd]))
	    {
	      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	      goto fail;
	    }

      g_last_pd_length_offset = (block << lbshift) * 512 + OFFSET_OF(struct grub_udf_pd, length);

	  data->npd++;
	}
      else if (tag.tag_ident == GRUB_UDF_TAG_IDENT_LVD)
	{
	  int k;

	  struct grub_udf_partmap *ppm;

	  if (grub_disk_read (disk, block << lbshift, 0,
			      sizeof (struct grub_udf_lvd),
			      &data->lvd))
	    {
	      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
	      goto fail;
	    }

	  if (data->npm + U32 (data->lvd.num_part_maps) > GRUB_UDF_MAX_PMS)
	    {
	      grub_error (GRUB_ERR_BAD_FS, "too many partition maps");
	      goto fail;
	    }

	  ppm = (struct grub_udf_partmap *) &data->lvd.part_maps;
	  for (k = U32 (data->lvd.num_part_maps); k > 0; k--)
	    {
	      if (ppm->type != GRUB_UDF_PARTMAP_TYPE_1)
		{
		  grub_error (GRUB_ERR_BAD_FS, "partmap type not supported");
		  goto fail;
		}

	      data->pms[data->npm++] = ppm;
	      ppm = (struct grub_udf_partmap *) ((char *) ppm +
                                                 U32 (ppm->length));
	    }
	}
      else if (tag.tag_ident > GRUB_UDF_TAG_IDENT_TD)
	{
	  grub_error (GRUB_ERR_BAD_FS, "invalid tag ident");
	  goto fail;
	}
      else if (tag.tag_ident == GRUB_UDF_TAG_IDENT_TD)
	break;

      block++;
    }

  for (i = 0; i < data->npm; i++)
    {
      int j;

      for (j = 0; j < data->npd; j++)
	if (data->pms[i]->type1.part_num == data->pds[j].part_num)
	  {
	    data->pms[i]->type1.part_num = j;
	    break;
	  }

      if (j == data->npd)
	{
	  grub_error (GRUB_ERR_BAD_FS, "can\'t find PD");
	  goto fail;
	}
    }

  block = grub_udf_get_block (data,
			      data->lvd.root_fileset.block.part_ref,
			      data->lvd.root_fileset.block.block_num);

  if (grub_errno)
    goto fail;

  if (grub_disk_read (disk, block << lbshift, 0,
		      sizeof (struct grub_udf_fileset), &root_fs))
    {
      grub_error (GRUB_ERR_BAD_FS, "not an UDF filesystem");
      goto fail;
    }

  if (U16 (root_fs.tag.tag_ident) != GRUB_UDF_TAG_IDENT_FSD)
    {
      grub_error (GRUB_ERR_BAD_FS, "invalid fileset descriptor");
      goto fail;
    }

  data->root_icb = root_fs.root_icb;

  return data;

fail:
  grub_free (data);
  return 0;
}

#ifdef GRUB_UTIL
grub_disk_addr_t
grub_udf_get_cluster_sector (grub_disk_t disk, grub_uint64_t *sec_per_lcn)
{
  grub_disk_addr_t ret;
  static struct grub_udf_data *data;

  data = grub_udf_mount (disk);
  if (!data)
    return 0;

  ret = U32 (data->pds[data->pms[0]->type1.part_num].start);
  *sec_per_lcn = 1ULL << data->lbshift;
  grub_free (data);
  return ret;
}
#endif

static char *
read_string (const grub_uint8_t *raw, grub_size_t sz, char *outbuf)
{
  grub_uint16_t *utf16 = NULL;
  grub_size_t utf16len = 0;

  if (sz == 0)
    return NULL;

  if (raw[0] != 8 && raw[0] != 16)
    return NULL;

  if (raw[0] == 8)
    {
      unsigned i;
      utf16len = sz - 1;
      utf16 = grub_malloc (utf16len * sizeof (utf16[0]));
      if (!utf16)
	return NULL;
      for (i = 0; i < utf16len; i++)
	utf16[i] = raw[i + 1];
    }
  if (raw[0] == 16)
    {
      unsigned i;
      utf16len = (sz - 1) / 2;
      utf16 = grub_malloc (utf16len * sizeof (utf16[0]));
      if (!utf16)
	return NULL;
      for (i = 0; i < utf16len; i++)
	utf16[i] = (raw[2 * i + 1] << 8) | raw[2*i + 2];
    }
  if (!outbuf)
    outbuf = grub_malloc (utf16len * GRUB_MAX_UTF8_PER_UTF16 + 1);
  if (outbuf)
    *grub_utf16_to_utf8 ((grub_uint8_t *) outbuf, utf16, utf16len) = '\0';
  grub_free (utf16);
  return outbuf;
}

static char *
read_dstring (const grub_uint8_t *raw, grub_size_t sz)
{
  grub_size_t len;

  if (raw[0] == 0) {
      char *outbuf = grub_malloc (1);
      if (!outbuf)
	return NULL;
      outbuf[0] = 0;
      return outbuf;
    }

  len = raw[sz - 1];
  if (len > sz - 1)
    len = sz - 1;
  return read_string (raw, len, NULL);
}

static int
grub_udf_iterate_dir (grub_fshelp_node_t dir,
		      grub_fshelp_iterate_dir_hook_t hook, void *hook_data)
{
  grub_fshelp_node_t child;
  struct grub_udf_file_ident dirent;
  grub_off_t offset = 0;

  child = grub_malloc (get_fshelp_size (dir->data));
  if (!child)
    return 0;

  /* The current directory is not stored.  */
  grub_memcpy (child, dir, get_fshelp_size (dir->data));

  if (hook (".", GRUB_FSHELP_DIR, child, hook_data))
    return 1;

  while (offset < U64 (dir->block.fe.file_size))
    {
      if (grub_udf_read_file (dir, 0, 0, offset, sizeof (dirent),
			      (char *) &dirent) != sizeof (dirent))
	return 0;

      if (U16 (dirent.tag.tag_ident) != GRUB_UDF_TAG_IDENT_FID)
	{
	  grub_error (GRUB_ERR_BAD_FS, "invalid fid tag");
	  return 0;
	}

      offset += sizeof (dirent) + U16 (dirent.imp_use_length);
      if (!(dirent.characteristics & GRUB_UDF_FID_CHAR_DELETED))
	{
	  child = grub_malloc (get_fshelp_size (dir->data));
	  if (!child)
	    return 0;

          if (grub_udf_read_icb (dir->data, &dirent.icb, child))
	    return 0;

          g_last_icb_read_sector = g_last_disk_read_sector;
          g_last_icb_read_sector_tag_ident = g_last_fe_tag_ident;
          if (dirent.characteristics & GRUB_UDF_FID_CHAR_PARENT)
	    {
	      /* This is the parent directory.  */
	      if (hook ("..", GRUB_FSHELP_DIR, child, hook_data))
	        return 1;
	    }
          else
	    {
	      enum grub_fshelp_filetype type;
	      char *filename;
	      grub_uint8_t raw[MAX_FILE_IDENT_LENGTH];

	      type = ((dirent.characteristics & GRUB_UDF_FID_CHAR_DIRECTORY) ?
		      (GRUB_FSHELP_DIR) : (GRUB_FSHELP_REG));
	      if (child->block.fe.icbtag.file_type == GRUB_UDF_ICBTAG_TYPE_SYMLINK)
		type = GRUB_FSHELP_SYMLINK;

	      if ((grub_udf_read_file (dir, 0, 0, offset,
				       dirent.file_ident_length,
				       (char *) raw))
		  != dirent.file_ident_length)
		return 0;

	      filename = read_string (raw, dirent.file_ident_length, 0);
	      if (!filename)
		grub_print_error ();

	      if (filename && hook (filename, type, child, hook_data))
		{
		    g_last_fileattr_read_sector = g_last_icb_read_sector;
            g_last_fileattr_read_sector_tag_ident = g_last_icb_read_sector_tag_ident;
            g_last_fileattr_offset = (grub_uint32_t)((child->block.fe.ext_attr + child->block.fe.ext_attr_length) - (grub_uint8_t *)&(child->block.fe));
		  grub_free (filename);
		  return 1;
		}
	      grub_free (filename);
	    }
	}

      /* Align to dword boundary.  */
      offset = (offset + dirent.file_ident_length + 3) & (~3);
    }

  return 0;
}

static char *
grub_udf_read_symlink (grub_fshelp_node_t node)
{
  grub_size_t sz = U64 (node->block.fe.file_size);
  grub_uint8_t *raw;
  const grub_uint8_t *ptr;
  char *out, *optr;

  if (sz < 4)
    return NULL;
  raw = grub_malloc (sz);
  if (!raw)
    return NULL;
  if (grub_udf_read_file (node, NULL, NULL, 0, sz, (char *) raw) < 0)
    {
      grub_free (raw);
      return NULL;
    }

  out = grub_malloc (sz * 2 + 1);
  if (!out)
    {
      grub_free (raw);
      return NULL;
    }

  optr = out;

  for (ptr = raw; ptr < raw + sz; )
    {
      grub_size_t s;
      if ((grub_size_t) (ptr - raw + 4) > sz)
	goto fail;
      if (!(ptr[2] == 0 && ptr[3] == 0))
	goto fail;
      s = 4 + ptr[1];
      if ((grub_size_t) (ptr - raw + s) > sz)
	goto fail;
      switch (*ptr)
	{
	case 1:
	  if (ptr[1])
	    goto fail;
	  /* Fallthrough.  */
	case 2:
	  /* in 4 bytes. out: 1 byte.  */
	  optr = out;
	  *optr++ = '/';
	  break;
	case 3:
	  /* in 4 bytes. out: 3 bytes.  */
	  if (optr != out)
	    *optr++ = '/';
	  *optr++ = '.';
	  *optr++ = '.';
	  break;
	case 4:
	  /* in 4 bytes. out: 2 bytes.  */
	  if (optr != out)
	    *optr++ = '/';
	  *optr++ = '.';
	  break;
	case 5:
	  /* in 4 + n bytes. out, at most: 1 + 2 * n bytes.  */
	  if (optr != out)
	    *optr++ = '/';
	  if (!read_string (ptr + 4, s - 4, optr))
	    goto fail;
	  optr += grub_strlen (optr);
	  break;
	default:
	  goto fail;
	}
      ptr += s;
    }
  *optr = 0;
  grub_free (raw);
  return out;

 fail:
  grub_free (raw);
  grub_free (out);
  grub_error (GRUB_ERR_BAD_FS, "invalid symlink");
  return NULL;
}

/* Context for grub_udf_dir.  */
struct grub_udf_dir_ctx
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
};

/* Helper for grub_udf_dir.  */
static int
grub_udf_dir_iter (const char *filename, enum grub_fshelp_filetype filetype,
		   grub_fshelp_node_t node, void *data)
{
  struct grub_udf_dir_ctx *ctx = data;
  struct grub_dirhook_info info;
  const struct grub_udf_timestamp *tstamp = NULL;

  grub_memset (&info, 0, sizeof (info));
  info.dir = ((filetype & GRUB_FSHELP_TYPE_MASK) == GRUB_FSHELP_DIR);
  if (U16 (node->block.fe.tag.tag_ident) == GRUB_UDF_TAG_IDENT_FE)
    tstamp = &node->block.fe.modification_time;
  else if (U16 (node->block.fe.tag.tag_ident) == GRUB_UDF_TAG_IDENT_EFE)
    tstamp = &node->block.efe.modification_time;

  if (tstamp && (U16 (tstamp->type_and_timezone) & 0xf000) == 0x1000)
    {
      grub_int16_t tz;
      struct grub_datetime datetime;

      datetime.year = U16 (tstamp->year);
      datetime.month = tstamp->month;
      datetime.day = tstamp->day;
      datetime.hour = tstamp->hour;
      datetime.minute = tstamp->minute;
      datetime.second = tstamp->second;

      tz = U16 (tstamp->type_and_timezone) & 0xfff;
      if (tz & 0x800)
	tz |= 0xf000;
      if (tz == -2047)
	tz = 0;

      info.mtimeset = !!grub_datetime2unixtime (&datetime, &info.mtime);

      info.mtime -= 60 * tz;
    }
  if (!info.dir)
    info.size = U64 (node->block.fe.file_size);
  grub_free (node);
  return ctx->hook (filename, &info, ctx->hook_data);
}

static grub_err_t
grub_udf_dir (grub_device_t device, const char *path,
	      grub_fs_dir_hook_t hook, void *hook_data)
{
  struct grub_udf_dir_ctx ctx = { hook, hook_data };
  struct grub_udf_data *data = 0;
  struct grub_fshelp_node *rootnode = 0;
  struct grub_fshelp_node *foundnode = 0;

  grub_dl_ref (my_mod);

  data = grub_udf_mount (device->disk);
  if (!data)
    goto fail;

  rootnode = grub_malloc (get_fshelp_size (data));
  if (!rootnode)
    goto fail;

  if (grub_udf_read_icb (data, &data->root_icb, rootnode))
    goto fail;

  if (grub_fshelp_find_file (path, rootnode,
			     &foundnode,
			     grub_udf_iterate_dir, grub_udf_read_symlink,
			     GRUB_FSHELP_DIR))
    goto fail;

  grub_udf_iterate_dir (foundnode, grub_udf_dir_iter, &ctx);

  if (foundnode != rootnode)
    grub_free (foundnode);

fail:
  grub_free (rootnode);

  grub_free (data);

  grub_dl_unref (my_mod);

  return grub_errno;
}

static grub_err_t
grub_udf_open (struct grub_file *file, const char *name)
{
  struct grub_udf_data *data;
  struct grub_fshelp_node *rootnode = 0;
  struct grub_fshelp_node *foundnode;

  grub_dl_ref (my_mod);

  data = grub_udf_mount (file->device->disk);
  if (!data)
    goto fail;

  rootnode = grub_malloc (get_fshelp_size (data));
  if (!rootnode)
    goto fail;

  if (grub_udf_read_icb (data, &data->root_icb, rootnode))
    goto fail;

  if (grub_fshelp_find_file (name, rootnode,
			     &foundnode,
			     grub_udf_iterate_dir, grub_udf_read_symlink,
			     GRUB_FSHELP_REG))
    goto fail;

  file->data = foundnode;
  file->offset = 0;
  file->size = U64 (foundnode->block.fe.file_size);

  grub_free (rootnode);

  return 0;

fail:
  grub_dl_unref (my_mod);

  grub_free (data);
  grub_free (rootnode);

  return grub_errno;
}

static grub_ssize_t
grub_udf_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_fshelp_node *node = (struct grub_fshelp_node *) file->data;

  return grub_udf_read_file (node, file->read_hook, file->read_hook_data,
			     file->offset, len, buf);
}

static grub_err_t
grub_udf_close (grub_file_t file)
{
  if (file->data)
    {
      struct grub_fshelp_node *node = (struct grub_fshelp_node *) file->data;

      grub_free (node->data);
      grub_free (node);
    }

  grub_dl_unref (my_mod);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_udf_label (grub_device_t device, char **label)
{
  struct grub_udf_data *data;
  data = grub_udf_mount (device->disk);

  if (data)
    {
      *label = read_dstring (data->lvd.ident, sizeof (data->lvd.ident));
      grub_free (data);
    }
  else
    *label = 0;

  return grub_errno;
}

static char *
gen_uuid_from_volset (char *volset_ident)
{
  grub_size_t i;
  grub_size_t len;
  grub_size_t nonhexpos;
  grub_uint8_t buf[17];
  char *uuid;

  len = grub_strlen (volset_ident);
  if (len < 8)
    return NULL;

  uuid = grub_malloc (17);
  if (!uuid)
    return NULL;

  if (len > 16)
    len = 16;

  grub_memset (buf, 0, sizeof (buf));
  grub_memcpy (buf, volset_ident, len);

  nonhexpos = 16;
  for (i = 0; i < 16; ++i)
    {
      if (!grub_isxdigit (buf[i]))
        {
          nonhexpos = i;
          break;
        }
    }

  if (nonhexpos < 8)
    {
      grub_snprintf (uuid, 17, "%02x%02x%02x%02x%02x%02x%02x%02x",
                    buf[0], buf[1], buf[2], buf[3],
                    buf[4], buf[5], buf[6], buf[7]);
    }
  else if (nonhexpos < 16)
    {
      for (i = 0; i < 8; ++i)
        uuid[i] = grub_tolower (buf[i]);
      grub_snprintf (uuid+8, 9, "%02x%02x%02x%02x",
                    buf[8], buf[9], buf[10], buf[11]);
    }
  else
    {
      for (i = 0; i < 16; ++i)
        uuid[i] = grub_tolower (buf[i]);
      uuid[16] = 0;
    }

  return uuid;
}

static grub_err_t
grub_udf_uuid (grub_device_t device, char **uuid)
{
  char *volset_ident;
  struct grub_udf_data *data;
  data = grub_udf_mount (device->disk);

  if (data)
    {
      volset_ident = read_dstring (data->pvd.volset_ident, sizeof (data->pvd.volset_ident));
      if (volset_ident)
        {
          *uuid = gen_uuid_from_volset (volset_ident);
          grub_free (volset_ident);
        }
      else
        *uuid = 0;
      grub_free (data);
    }
  else
    *uuid = 0;

  return grub_errno;
}

grub_uint64_t grub_udf_get_file_offset(grub_file_t file)
{
    grub_disk_addr_t sector;
    struct grub_fshelp_node *node = (struct grub_fshelp_node *)file->data;
    
    sector = grub_udf_read_block(node, 0);
    
    return 512 * (sector << node->data->lbshift);
}

grub_uint64_t grub_udf_get_last_pd_size_offset(void)
{
    return g_last_pd_length_offset;
}

grub_uint64_t grub_udf_get_last_file_attr_offset
(
    grub_file_t file, 
    grub_uint32_t *startBlock,
    grub_uint64_t *fe_entry_size_offset
)
{
    grub_uint64_t attr_offset;
    struct grub_fshelp_node *node;
    struct grub_udf_data *data;

    node = (struct grub_fshelp_node *)file->data;
    data = node->data;

    *startBlock = data->pds[data->pms[0]->type1.part_num].start;

    attr_offset = g_last_fileattr_read_sector * 2048 + g_last_fileattr_offset;
    
    if (GRUB_UDF_TAG_IDENT_FE == g_last_fileattr_read_sector_tag_ident)
    {
        *fe_entry_size_offset = g_last_fileattr_read_sector * 2048 + OFFSET_OF(struct grub_udf_file_entry, file_size);
    }
    else
    {
        *fe_entry_size_offset = g_last_fileattr_read_sector * 2048 + OFFSET_OF(struct grub_udf_extended_file_entry, file_size);
    }

    return attr_offset;
}

static struct grub_fs grub_udf_fs = {
  .name = "udf",
  .fs_dir = grub_udf_dir,
  .fs_open = grub_udf_open,
  .fs_read = grub_udf_read,
  .fs_close = grub_udf_close,
  .fs_label = grub_udf_label,
  .fs_uuid = grub_udf_uuid,
#ifdef GRUB_UTIL
  .reserved_first_sector = 1,
  .blocklist_install = 1,
#endif
  .next = 0
};

GRUB_MOD_INIT (udf)
{
  grub_fs_register (&grub_udf_fs);
  my_mod = mod;
}

GRUB_MOD_FINI (udf)
{
  grub_fs_unregister (&grub_udf_fs);
}
