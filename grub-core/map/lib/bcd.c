 /*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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
 *
 */

#include <grub/dl.h>
#include <grub/err.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/procfs.h>
#include <grub/disk.h>
#include <grub/msdos_partition.h>
#include <grub/gpt_partition.h>
#include <grub/charset.h>

#include <xz.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <misc.h>
#include <guid.h>
#include <vfat.h>
#include <bcd.h>
#include <reg.h>

#include "raw/bcdwim.c"
#include "raw/bcdvhd.c"
#include "raw/bcdwin.c"
#include "raw/bcdram.c"

#pragma GCC diagnostic ignored "-Wcast-align"

grub_uint8_t grub_bcd_data[BCD_DECOMPRESS_LEN];

static char *
get_bcd (grub_size_t *sz)
{
  *sz = 0;
  char *ret = NULL;
  *sz = BCD_DECOMPRESS_LEN;
  if (!*sz)
    return ret;
  ret = grub_malloc (*sz);
  if (!ret)
    return ret;
  grub_memcpy (ret, grub_bcd_data, *sz);
  return ret;
}

static struct grub_procfs_entry proc_bcd =
{
  .name = "bcd",
  .get_contents = get_bcd,
};

static void
load_bcd (enum bcd_type type)
{
  void *bcd;
  uint32_t bcd_len;
  switch (type)
  {
    case BOOT_RAW:
    case BOOT_WIM:
      bcd = bcd_wim;
      bcd_len = bcd_wim_len;
      break;
    case BOOT_VHD:
      bcd = bcd_vhd;
      bcd_len = bcd_vhd_len;
      break;
    case BOOT_WIN:
      bcd = bcd_win;
      bcd_len = bcd_win_len;
      break;
    case BOOT_RAMVHD:
      bcd = bcd_ram;
      bcd_len = bcd_ram_len;
      break;
    default:
      bcd = bcd_wim;
      bcd_len = bcd_wim_len;
      break;
  }
  grub_xz_decompress (bcd, bcd_len, grub_bcd_data, BCD_DECOMPRESS_LEN);
}

#if 0
static void
bcd_print_hex (const void *data, grub_size_t len)
{
  const grub_uint8_t *p = data;
  grub_size_t i;
  for (i = 0; i < len; i++)
    grub_printf ("%02x ", p[i]);
}
#endif

static void
bcd_replace_hex (const void *search, uint32_t search_len,
                 const void *replace, uint32_t replace_len, int count)
{
  uint8_t *p = grub_bcd_data;
  uint32_t offset;
  int cnt = 0;
  for (offset = 0; offset + search_len < BCD_DECOMPRESS_LEN; offset++)
  {
    if (grub_memcmp (p + offset, search, search_len) == 0)
    {
      cnt++;
#if 0
        grub_printf ("0x%08x ", offset);
        bcd_print_hex (search, search_len);
        grub_printf ("\n---> ");
        bcd_print_hex (replace, replace_len);
        grub_printf ("\n");
#endif
      grub_memcpy (p + offset, replace, replace_len);
      if (count && cnt == count)
        break;
    }
  }
}

static void
bcd_patch_path (const char *path)
{
  const char *search = "\\PATH_SIGN";
  char path8[256], *p;
  wchar_t path16[256];
  grub_size_t len;
  if (path[0] != '/')
    grub_snprintf (path8, 256, "/%s", path);
  else
    grub_strncpy (path8, path, 256);
  len = 2 * (strlen (path8) + 1);
  /* replace '/' to '\\' */
  p = path8;
  while (*p)
  {
    if (*p == '/')
      *p = '\\';
    p++;
  }
  /* UTF-8 to UTF-16le */
  grub_memset (path16, 0, sizeof (path16));
  grub_utf8_to_utf16 (path16, len, (grub_uint8_t *)path8, -1, NULL);

  bcd_replace_hex (search, strlen (search), path16, len, 2);
}

static grub_err_t
bcd_patch_dp (struct bcd_patch_data *cmd)
{
  grub_disk_t disk = 0;
  /* mbr*/
  grub_uint64_t part_start;
  struct grub_msdos_partition_mbr mbr;
  grub_disk_addr_t lba;
  /* gpt */
  struct grub_gpt_header gpt;
  struct grub_gpt_partentry *gpt_entry = NULL;
  grub_uint32_t gpt_entry_size;
  grub_uint64_t gpt_entry_pos;
  grub_uint8_t diskguid[16];
  grub_uint8_t partguid[16];
  int partnum;

  if (cmd->type == BOOT_RAW)
  {
    grub_uint32_t signature = VDISK_MBR_SIGNATURE;
    part_start = VDISK_PARTITION_LBA << GRUB_DISK_SECTOR_BITS;
    /* fill dp */
    memset (&cmd->dp, 0, sizeof (struct bcd_dp));
    memcpy (cmd->dp.partid, &part_start, 8);
    cmd->dp.partmap = 0x01;
    memcpy (cmd->dp.diskid, &signature, 4);
    bcd_replace_hex (BCD_DP_MAGIC, strlen (BCD_DP_MAGIC),
                   &cmd->dp, sizeof (struct bcd_dp), 2);
    return GRUB_ERR_NONE;
  }
  disk = grub_disk_open (cmd->file->device->disk->name);
  if (!disk)
    return grub_error (GRUB_ERR_BAD_OS, "failed to open parent disk");
  if (cmd->file->device->disk->partition->partmap->name[0] == 'g')
  {
    partnum = cmd->file->device->disk->partition->number;
    grub_disk_read (disk, 1, 0, GRUB_DISK_SECTOR_SIZE, &gpt);
    grub_guidcpy ((grub_packed_guid_t *)&diskguid, &gpt.guid);
    gpt_entry_pos = gpt.partitions << GRUB_DISK_SECTOR_BITS;
    gpt_entry_size = gpt.partentry_size;
    gpt_entry = grub_zalloc (gpt_entry_size);
    if (!gpt_entry)
    {
      grub_disk_close (disk);
      return grub_error (GRUB_ERR_BAD_OS, "out of memory");
    }
    grub_disk_read (disk, 0, gpt_entry_pos + partnum * gpt_entry_size,
                    gpt_entry_size, gpt_entry);
    grub_guidcpy ((grub_packed_guid_t *)&partguid, &gpt_entry->guid);
    /* fill dp */
    memset (&cmd->dp, 0, sizeof (struct bcd_dp));
    memcpy (cmd->dp.partid, partguid, 16);
    cmd->dp.partmap = 0x00;
    memcpy (cmd->dp.diskid, diskguid, 16);
    free (gpt_entry);
  }
  else
  {
    lba = grub_partition_get_start (cmd->file->device->disk->partition);
    part_start = lba << GRUB_DISK_SECTOR_BITS;
    grub_disk_read (disk, 0, 0, GRUB_DISK_SECTOR_SIZE, &mbr);
    /* fill dp */
    memset (&cmd->dp, 0, sizeof (struct bcd_dp));
    memcpy (cmd->dp.partid, &part_start, 8);
    cmd->dp.partmap = 0x01;
    memcpy (cmd->dp.diskid, mbr.unique_signature, 4);
  }
  grub_disk_close (disk);
  bcd_replace_hex (BCD_DP_MAGIC, strlen (BCD_DP_MAGIC),
                   &cmd->dp, sizeof (struct bcd_dp), 2);
  return GRUB_ERR_NONE;
}

static void
bcd_patch_hive (grub_reg_hive_t *hive, const wchar_t *keyname, void *val)
{
  HKEY root, objects, osloader, elements, key;
  grub_uint8_t *data = NULL;
  grub_uint32_t data_len = 0, type;
  hive->find_root (hive, &root);
  hive->find_key (hive, root, BCD_REG_ROOT, &objects);
  if (wcscasecmp (keyname, BCDOPT_TIMEOUT) == 0)
    hive->find_key (hive, objects, GUID_BOOTMGR, &osloader);
  else if (wcscasecmp (keyname, BCDOPT_IMGOFS) == 0)
    hive->find_key (hive, objects, GUID_RAMDISK, &osloader);
  else
    hive->find_key (hive, objects, GUID_OSENTRY, &osloader);
  hive->find_key (hive, osloader, BCD_REG_HKEY, &elements);
  hive->find_key (hive, elements, keyname, &key);
  hive->query_value_no_copy (hive, key, BCD_REG_HVAL,
                             (void **)&data, &data_len, &type);
  memcpy (data, val, data_len);
}

static void
bcd_parse_bool (grub_reg_hive_t *hive, const wchar_t *keyname, const char *s)
{
  uint8_t val = 0;
  if (grub_strcasecmp (s, "yes") == 0 || grub_strcasecmp (s, "on") == 0 ||
      grub_strcasecmp (s, "true") == 0 || grub_strcasecmp (s, "1") == 0)
    val = 1;
  bcd_patch_hive (hive, keyname, &val);
}

static void
bcd_parse_u64 (grub_reg_hive_t *hive, const wchar_t *keyname, const char *s)
{
  uint64_t val = 0;
  val = grub_strtoul (s, NULL, 0);
  bcd_patch_hive (hive, keyname, &val);
}

static void
bcd_parse_str (grub_reg_hive_t *hive, const wchar_t *keyname, const char *s)
{
  HKEY root, objects, osloader, elements, key;
  grub_uint16_t *data = NULL;
  grub_uint32_t data_len = 0, type;
  hive->find_root (hive, &root);
  hive->find_key (hive, root, BCD_REG_ROOT, &objects);
  hive->find_key (hive, objects, GUID_OSENTRY, &osloader);
  hive->find_key (hive, osloader, BCD_REG_HKEY, &elements);
  hive->find_key (hive, elements, keyname, &key);
  hive->query_value_no_copy (hive, key, BCD_REG_HVAL,
                             (void **)&data, &data_len, &type);
  grub_memset (data, 0, data_len);
  grub_utf8_to_utf16 (data, data_len, (grub_uint8_t *)s, -1, NULL);
}

grub_err_t
grub_patch_bcd (struct bcd_patch_data *cmd)
{
  char bcd_name[64];
  grub_file_t bcd_file = 0;
  grub_reg_hive_t *hive = NULL;
  void *data;
  grub_uint32_t bcd_len = BCD_DECOMPRESS_LEN;

  load_bcd (cmd->type);
  bcd_replace_hex (BCD_SEARCH_EXT, 8, BCD_REPLACE_EXT, 8, 0);

  if (cmd->type != BOOT_WIN)
    bcd_patch_path (cmd->path);

  if (bcd_patch_dp (cmd))
    return grub_errno;

  grub_snprintf (bcd_name, 64, "mem:%p:size:%u", grub_bcd_data, bcd_len);
  bcd_file = grub_file_open (bcd_name, GRUB_FILE_TYPE_CAT);
  grub_open_hive (bcd_file, &hive);
  if (!hive)
    return grub_error (GRUB_ERR_BAD_OS, "bcd hive load error.");
  if (cmd->testmode)
    bcd_parse_bool (hive, BCDOPT_TESTMODE, cmd->testmode);
  else
    bcd_parse_bool (hive, BCDOPT_TESTMODE, "no");
  if (cmd->highest)
    bcd_parse_bool (hive, BCDOPT_HIGHEST, cmd->highest);
  if (cmd->nx)
  {
    uint64_t nx = 0;
    if (grub_strcasecmp (cmd->nx, "OptIn") == 0)
      nx = NX_OPTIN;
    else if (grub_strcasecmp (cmd->nx, "OptOut") == 0)
      nx = NX_OPTOUT;
    else if (grub_strcasecmp (cmd->nx, "AlwaysOff") == 0)
      nx = NX_ALWAYSOFF;
    else if (grub_strcasecmp (cmd->nx, "AlwaysOn") == 0)
      nx = NX_ALWAYSON;
    bcd_patch_hive (hive, BCDOPT_NX, &nx);
  }
  if (cmd->pae)
  {
    uint64_t pae = 0;
    if (grub_strcasecmp (cmd->pae, "Default") == 0)
      pae = PAE_DEFAULT;
    else if (grub_strcasecmp (cmd->pae, "Enable") == 0)
      pae = PAE_ENABLE;
    else if (grub_strcasecmp (cmd->pae, "Disable") == 0)
      pae = PAE_DISABLE;
    bcd_patch_hive (hive, BCDOPT_PAE, &pae);
  }
  if (cmd->detecthal)
    bcd_parse_bool (hive, BCDOPT_DETHAL, cmd->detecthal);
  if (cmd->winpe)
    bcd_parse_bool (hive, BCDOPT_WINPE, cmd->winpe);
  if (cmd->imgoffset && cmd->type == BOOT_RAMVHD)
    bcd_parse_u64 (hive, BCDOPT_IMGOFS, cmd->imgoffset);
  if (cmd->timeout)
    bcd_parse_u64 (hive, BCDOPT_TIMEOUT, cmd->timeout);
  if (cmd->sos)
    bcd_parse_bool (hive, BCDOPT_SOS, cmd->sos);
  if (cmd->novesa)
    bcd_parse_bool (hive, BCDOPT_NOVESA, cmd->novesa);
  if (cmd->novga)
    bcd_parse_bool (hive, BCDOPT_NOVGA, cmd->novga);
  if (cmd->cmdline)
    bcd_parse_str (hive, BCDOPT_CMDLINE, cmd->cmdline);
  else
    bcd_parse_str (hive, BCDOPT_CMDLINE, BCD_DEFAULT_CMDLINE);
  if (cmd->winload)
    bcd_parse_str (hive, BCDOPT_WINLOAD, cmd->winload);
  else
    bcd_parse_str (hive, BCDOPT_WINLOAD, BCD_DEFAULT_WINLOAD);
  if (cmd->sysroot)
    bcd_parse_str (hive, BCDOPT_SYSROOT, cmd->sysroot);
  else
    bcd_parse_str (hive, BCDOPT_SYSROOT, BCD_DEFAULT_SYSROOT);

  /* write to bcd */
  hive->steal_data (hive, &data, &bcd_len);
  memcpy (grub_bcd_data, data, BCD_DECOMPRESS_LEN);
  free (data);
  hive->close (hive);

  return GRUB_ERR_NONE;
}

void
grub_load_bcd (void)
{
  grub_procfs_register ("bcd", &proc_bcd);
}

void
grub_unload_bcd (void)
{
  grub_procfs_unregister (&proc_bcd);
}
