/* efi.c - generic EFI support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/misc.h>
#include <grub/charset.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/console_control.h>
#include <grub/efi/pe32.h>
#include <grub/time.h>
#include <grub/term.h>
#include <grub/kernel.h>
#include <grub/mm.h>
#include <grub/loader.h>

/* The handle of GRUB itself. Filled in by the startup code.  */
grub_efi_handle_t grub_efi_image_handle;

/* The pointer to a system table. Filled in by the startup code.  */
grub_efi_system_table_t *grub_efi_system_table;

static grub_efi_guid_t console_control_guid = GRUB_EFI_CONSOLE_CONTROL_GUID;
static grub_efi_guid_t loaded_image_guid = GRUB_EFI_LOADED_IMAGE_GUID;
static grub_efi_guid_t device_path_guid = GRUB_EFI_DEVICE_PATH_GUID;

void *
grub_efi_locate_protocol (grub_efi_guid_t *protocol, void *registration)
{
  void *interface;
  grub_efi_status_t status;

  status = efi_call_3 (grub_efi_system_table->boot_services->locate_protocol,
                       protocol, registration, &interface);
  if (status != GRUB_EFI_SUCCESS)
    return 0;

  return interface;
}

/* Return the array of handles which meet the requirement. If successful,
   the number of handles is stored in NUM_HANDLES. The array is allocated
   from the heap.  */
grub_efi_handle_t *
grub_efi_locate_handle (grub_efi_locate_search_type_t search_type,
			grub_efi_guid_t *protocol,
			void *search_key,
			grub_efi_uintn_t *num_handles)
{
  grub_efi_boot_services_t *b;
  grub_efi_status_t status;
  grub_efi_handle_t *buffer;
  grub_efi_uintn_t buffer_size = 8 * sizeof (grub_efi_handle_t);

  buffer = grub_malloc (buffer_size);
  if (! buffer)
    return 0;

  b = grub_efi_system_table->boot_services;
  status = efi_call_5 (b->locate_handle, search_type, protocol, search_key,
			     &buffer_size, buffer);
  if (status == GRUB_EFI_BUFFER_TOO_SMALL)
    {
      grub_free (buffer);
      buffer = grub_malloc (buffer_size);
      if (! buffer)
	return 0;

      status = efi_call_5 (b->locate_handle, search_type, protocol, search_key,
				 &buffer_size, buffer);
    }

  if (status != GRUB_EFI_SUCCESS)
    {
      grub_free (buffer);
      return 0;
    }

  *num_handles = buffer_size / sizeof (grub_efi_handle_t);
  return buffer;
}

void *
grub_efi_open_protocol (grub_efi_handle_t handle,
			grub_efi_guid_t *protocol,
			grub_efi_uint32_t attributes)
{
  grub_efi_boot_services_t *b;
  grub_efi_status_t status;
  void *interface;

  b = grub_efi_system_table->boot_services;
  status = efi_call_6 (b->open_protocol, handle,
		       protocol,
		       &interface,
		       grub_efi_image_handle,
		       0,
		       attributes);
  if (status != GRUB_EFI_SUCCESS)
    return 0;

  return interface;
}

int
grub_efi_set_text_mode (int on)
{
  grub_efi_console_control_protocol_t *c;
  grub_efi_screen_mode_t mode, new_mode;

  c = grub_efi_locate_protocol (&console_control_guid, 0);
  if (! c)
    /* No console control protocol instance available, assume it is
       already in text mode. */
    return 1;

  if (efi_call_4 (c->get_mode, c, &mode, 0, 0) != GRUB_EFI_SUCCESS)
    return 0;

  new_mode = on ? GRUB_EFI_SCREEN_TEXT : GRUB_EFI_SCREEN_GRAPHICS;
  if (mode != new_mode)
    if (efi_call_2 (c->set_mode, c, new_mode) != GRUB_EFI_SUCCESS)
      return 0;

  return 1;
}

void
grub_efi_stall (grub_efi_uintn_t microseconds)
{
  efi_call_1 (grub_efi_system_table->boot_services->stall, microseconds);
}

grub_efi_loaded_image_t *
grub_efi_get_loaded_image (grub_efi_handle_t image_handle)
{
  return grub_efi_open_protocol (image_handle,
				 &loaded_image_guid,
				 GRUB_EFI_OPEN_PROTOCOL_GET_PROTOCOL);
}

void
grub_reboot (void)
{
  grub_machine_fini (GRUB_LOADER_FLAG_NORETURN);
  efi_call_4 (grub_efi_system_table->runtime_services->reset_system,
              GRUB_EFI_RESET_COLD, GRUB_EFI_SUCCESS, 0, NULL);
  for (;;) ;
}

void
grub_exit (void)
{
  grub_machine_fini (GRUB_LOADER_FLAG_NORETURN);
  efi_call_4 (grub_efi_system_table->boot_services->exit,
              grub_efi_image_handle, GRUB_EFI_SUCCESS, 0, 0);
  for (;;) ;
}

grub_err_t
grub_efi_set_virtual_address_map (grub_efi_uintn_t memory_map_size,
				  grub_efi_uintn_t descriptor_size,
				  grub_efi_uint32_t descriptor_version,
				  grub_efi_memory_descriptor_t *virtual_map)
{
  grub_efi_runtime_services_t *r;
  grub_efi_status_t status;

  r = grub_efi_system_table->runtime_services;
  status = efi_call_4 (r->set_virtual_address_map, memory_map_size,
		       descriptor_size, descriptor_version, virtual_map);

  if (status == GRUB_EFI_SUCCESS)
    return GRUB_ERR_NONE;

  return grub_error (GRUB_ERR_IO, "set_virtual_address_map failed");
}

grub_err_t
grub_efi_set_variable(const char *var, const grub_efi_guid_t *guid,
		      void *data, grub_size_t datasize)
{
  grub_efi_status_t status;
  grub_efi_runtime_services_t *r;
  grub_efi_char16_t *var16;
  grub_size_t len, len16;

  len = grub_strlen (var);
  len16 = len * GRUB_MAX_UTF16_PER_UTF8;
  var16 = grub_malloc ((len16 + 1) * sizeof (var16[0]));
  if (!var16)
    return grub_errno;
  len16 = grub_utf8_to_utf16 (var16, len16, (grub_uint8_t *) var, len, NULL);
  var16[len16] = 0;

  r = grub_efi_system_table->runtime_services;

  status = efi_call_5 (r->set_variable, var16, guid, 
		       (GRUB_EFI_VARIABLE_NON_VOLATILE
			| GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS
			| GRUB_EFI_VARIABLE_RUNTIME_ACCESS),
		       datasize, data);
  grub_free (var16);
  if (status == GRUB_EFI_SUCCESS)
    return GRUB_ERR_NONE;

  return grub_error (GRUB_ERR_IO, "could not set EFI variable `%s'", var);
}

void *
grub_efi_get_variable (const char *var, const grub_efi_guid_t *guid,
		       grub_size_t *datasize_out)
{
  grub_efi_status_t status;
  grub_efi_uintn_t datasize = 0;
  grub_efi_runtime_services_t *r;
  grub_efi_char16_t *var16;
  void *data;
  grub_size_t len, len16;

  *datasize_out = 0;

  len = grub_strlen (var);
  len16 = len * GRUB_MAX_UTF16_PER_UTF8;
  var16 = grub_malloc ((len16 + 1) * sizeof (var16[0]));
  if (!var16)
    return NULL;
  len16 = grub_utf8_to_utf16 (var16, len16, (grub_uint8_t *) var, len, NULL);
  var16[len16] = 0;

  r = grub_efi_system_table->runtime_services;

  status = efi_call_5 (r->get_variable, var16, guid, NULL, &datasize, NULL);

  if (status != GRUB_EFI_BUFFER_TOO_SMALL || !datasize)
    {
      grub_free (var16);
      return NULL;
    }

  data = grub_malloc (datasize);
  if (!data)
    {
      grub_free (var16);
      return NULL;
    }

  status = efi_call_5 (r->get_variable, var16, guid, NULL, &datasize, data);
  grub_free (var16);

  if (status == GRUB_EFI_SUCCESS)
    {
      *datasize_out = datasize;
      return data;
    }

  grub_free (data);
  return NULL;
}

#pragma GCC diagnostic ignored "-Wcast-align"

/* Search the mods section from the PE32/PE32+ image. This code uses
   a PE32 header, but should work with PE32+ as well.  */
grub_addr_t
grub_efi_modules_addr (void)
{
  grub_efi_loaded_image_t *image;
  struct grub_pe32_header *header;
  struct grub_pe32_coff_header *coff_header;
  struct grub_pe32_section_table *sections;
  struct grub_pe32_section_table *section;
  struct grub_module_info *info;
  grub_uint16_t i;

  image = grub_efi_get_loaded_image (grub_efi_image_handle);
  if (! image)
    return 0;

  header = image->image_base;
  coff_header = &(header->coff_header);
  sections
    = (struct grub_pe32_section_table *) ((char *) coff_header
					  + sizeof (*coff_header)
					  + coff_header->optional_header_size);

  for (i = 0, section = sections;
       i < coff_header->num_sections;
       i++, section++)
    {
      if (grub_strcmp (section->name, "mods") == 0)
	break;
    }

  if (i == coff_header->num_sections)
    return 0;

  info = (struct grub_module_info *) ((char *) image->image_base
				      + section->virtual_address);
  if (info->magic != GRUB_MODULE_MAGIC)
    return 0;

  return (grub_addr_t) info;
}

#pragma GCC diagnostic error "-Wcast-align"

char *
grub_efi_get_filename (grub_efi_device_path_t *dp0)
{
  char *name = 0, *p, *pi;
  grub_size_t filesize = 0;
  grub_efi_device_path_t *dp;

  if (!dp0)
    return NULL;

  dp = dp0;

  while (1)
    {
      grub_efi_uint8_t type = GRUB_EFI_DEVICE_PATH_TYPE (dp);
      grub_efi_uint8_t subtype = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp);

      if (type == GRUB_EFI_END_DEVICE_PATH_TYPE)
	break;
      if (type == GRUB_EFI_MEDIA_DEVICE_PATH_TYPE
	       && subtype == GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE)
	{
	  grub_efi_uint16_t len;
	  len = ((GRUB_EFI_DEVICE_PATH_LENGTH (dp) - 4)
		 / sizeof (grub_efi_char16_t));
	  filesize += GRUB_MAX_UTF8_PER_UTF16 * len + 2;
	}

      dp = GRUB_EFI_NEXT_DEVICE_PATH (dp);
    }

  if (!filesize)
    return NULL;

  dp = dp0;

  p = name = grub_malloc (filesize);
  if (!name)
    return NULL;

  while (1)
    {
      grub_efi_uint8_t type = GRUB_EFI_DEVICE_PATH_TYPE (dp);
      grub_efi_uint8_t subtype = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp);

      if (type == GRUB_EFI_END_DEVICE_PATH_TYPE)
	break;
      else if (type == GRUB_EFI_MEDIA_DEVICE_PATH_TYPE
	       && subtype == GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE)
	{
	  grub_efi_file_path_device_path_t *fp;
	  grub_efi_uint16_t len;
	  grub_efi_char16_t *dup_name;

	  *p++ = '/';

	  len = ((GRUB_EFI_DEVICE_PATH_LENGTH (dp) - 4)
		 / sizeof (grub_efi_char16_t));
	  fp = (grub_efi_file_path_device_path_t *) dp;
	  /* According to EFI spec Path Name is NULL terminated */
	  while (len > 0 && fp->path_name[len - 1] == 0)
	    len--;

	  dup_name = grub_malloc (len * sizeof (*dup_name));
	  if (!dup_name)
	    {
	      grub_free (name);
	      return NULL;
	    }
	  p = (char *) grub_utf16_to_utf8 ((unsigned char *) p,
					    grub_memcpy (dup_name, fp->path_name, len * sizeof (*dup_name)),
					    len);
	  grub_free (dup_name);
	}

      dp = GRUB_EFI_NEXT_DEVICE_PATH (dp);
    }

  *p = '\0';

  for (pi = name, p = name; *pi;)
    {
      /* EFI breaks paths with backslashes.  */
      if (*pi == '\\' || *pi == '/')
	{
	  *p++ = '/';
	  while (*pi == '\\' || *pi == '/')
	    pi++;
	  continue;
	}
      *p++ = *pi++;
    }
  *p = '\0';

  return name;
}

grub_efi_device_path_t *
grub_efi_get_device_path (grub_efi_handle_t handle)
{
  return grub_efi_open_protocol (handle, &device_path_guid,
				 GRUB_EFI_OPEN_PROTOCOL_GET_PROTOCOL);
}

/* Return the device path node right before the end node.  */
grub_efi_device_path_t *
grub_efi_find_last_device_path (const grub_efi_device_path_t *dp)
{
  grub_efi_device_path_t *next, *p;

  if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (dp))
    return 0;

  for (p = (grub_efi_device_path_t *) dp, next = GRUB_EFI_NEXT_DEVICE_PATH (p);
       ! GRUB_EFI_END_ENTIRE_DEVICE_PATH (next);
       p = next, next = GRUB_EFI_NEXT_DEVICE_PATH (next))
    ;

  return p;
}

/* Duplicate a device path.  */
grub_efi_device_path_t *
grub_efi_duplicate_device_path (const grub_efi_device_path_t *dp)
{
  grub_efi_device_path_t *p;
  grub_size_t total_size = 0;

  for (p = (grub_efi_device_path_t *) dp;
       ;
       p = GRUB_EFI_NEXT_DEVICE_PATH (p))
    {
      total_size += GRUB_EFI_DEVICE_PATH_LENGTH (p);
      if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (p))
	break;
    }

  p = grub_malloc (total_size);
  if (! p)
    return 0;

  grub_memcpy (p, dp, total_size);
  return p;
}

static void
dump_vendor_path (const char *type, grub_efi_vendor_device_path_t *vendor)
{
  grub_uint32_t vendor_data_len = vendor->header.length - sizeof (*vendor);
  grub_printf ("/%sVendor(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)[%x: ",
	       type,
	       (unsigned) vendor->vendor_guid.data1,
	       (unsigned) vendor->vendor_guid.data2,
	       (unsigned) vendor->vendor_guid.data3,
	       (unsigned) vendor->vendor_guid.data4[0],
	       (unsigned) vendor->vendor_guid.data4[1],
	       (unsigned) vendor->vendor_guid.data4[2],
	       (unsigned) vendor->vendor_guid.data4[3],
	       (unsigned) vendor->vendor_guid.data4[4],
	       (unsigned) vendor->vendor_guid.data4[5],
	       (unsigned) vendor->vendor_guid.data4[6],
	       (unsigned) vendor->vendor_guid.data4[7],
	       vendor_data_len);
  if (vendor->header.length > sizeof (*vendor))
    {
      grub_uint32_t i;
      for (i = 0; i < vendor_data_len; i++)
	grub_printf ("%02x ", vendor->vendor_defined_data[i]);
    }
  grub_printf ("]");
}


/* Print the chain of Device Path nodes. This is mainly for debugging. */
void
grub_efi_print_device_path (grub_efi_device_path_t *dp)
{
  while (1)
    {
      grub_efi_uint8_t type = GRUB_EFI_DEVICE_PATH_TYPE (dp);
      grub_efi_uint8_t subtype = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp);
      grub_efi_uint16_t len = GRUB_EFI_DEVICE_PATH_LENGTH (dp);

      switch (type)
	{
	case GRUB_EFI_END_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE:
	      grub_printf ("/EndEntire\n");
	      //grub_putchar ('\n');
	      break;
	    case GRUB_EFI_END_THIS_DEVICE_PATH_SUBTYPE:
	      grub_printf ("/EndThis\n");
	      //grub_putchar ('\n');
	      break;
	    default:
	      grub_printf ("/EndUnknown(%x)\n", (unsigned) subtype);
	      break;
	    }
	  break;

	case GRUB_EFI_HARDWARE_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_PCI_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_pci_device_path_t *pci
		  = (grub_efi_pci_device_path_t *) dp;
		grub_printf ("/PCI(%x,%x)",
			     (unsigned) pci->function, (unsigned) pci->device);
	      }
	      break;
	    case GRUB_EFI_PCCARD_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_pccard_device_path_t *pccard
		  = (grub_efi_pccard_device_path_t *) dp;
		grub_printf ("/PCCARD(%x)",
			     (unsigned) pccard->function);
	      }
	      break;
	    case GRUB_EFI_MEMORY_MAPPED_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_memory_mapped_device_path_t *mmapped
		  = (grub_efi_memory_mapped_device_path_t *) dp;
		grub_printf ("/MMap(%x,%llx,%llx)",
			     (unsigned) mmapped->memory_type,
			     (unsigned long long) mmapped->start_address,
			     (unsigned long long) mmapped->end_address);
	      }
	      break;
	    case GRUB_EFI_VENDOR_DEVICE_PATH_SUBTYPE:
	      dump_vendor_path ("Hardware",
				(grub_efi_vendor_device_path_t *) dp);
	      break;
	    case GRUB_EFI_CONTROLLER_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_controller_device_path_t *controller
		  = (grub_efi_controller_device_path_t *) dp;
		grub_printf ("/Ctrl(%x)",
			     (unsigned) controller->controller_number);
	      }
	      break;
	    default:
	      grub_printf ("/UnknownHW(%x)", (unsigned) subtype);
	      break;
	    }
	  break;

	case GRUB_EFI_ACPI_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_ACPI_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_acpi_device_path_t *acpi
		  = (grub_efi_acpi_device_path_t *) dp;
		grub_printf ("/ACPI(%x,%x)",
			     (unsigned) acpi->hid,
			     (unsigned) acpi->uid);
	      }
	      break;
	    case GRUB_EFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_expanded_acpi_device_path_t *eacpi
		  = (grub_efi_expanded_acpi_device_path_t *) dp;
		grub_printf ("/ACPI(");

		if (GRUB_EFI_EXPANDED_ACPI_HIDSTR (dp)[0] == '\0')
		  grub_printf ("%x,", (unsigned) eacpi->hid);
		else
		  grub_printf ("%s,", GRUB_EFI_EXPANDED_ACPI_HIDSTR (dp));

		if (GRUB_EFI_EXPANDED_ACPI_UIDSTR (dp)[0] == '\0')
		  grub_printf ("%x,", (unsigned) eacpi->uid);
		else
		  grub_printf ("%s,", GRUB_EFI_EXPANDED_ACPI_UIDSTR (dp));

		if (GRUB_EFI_EXPANDED_ACPI_CIDSTR (dp)[0] == '\0')
		  grub_printf ("%x)", (unsigned) eacpi->cid);
		else
		  grub_printf ("%s)", GRUB_EFI_EXPANDED_ACPI_CIDSTR (dp));
	      }
	      break;
	    default:
	      grub_printf ("/UnknownACPI(%x)", (unsigned) subtype);
	      break;
	    }
	  break;

	case GRUB_EFI_MESSAGING_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_ATAPI_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_atapi_device_path_t *atapi
		  = (grub_efi_atapi_device_path_t *) dp;
		grub_printf ("/ATAPI(%x,%x,%x)",
			     (unsigned) atapi->primary_secondary,
			     (unsigned) atapi->slave_master,
			     (unsigned) atapi->lun);
	      }
	      break;
	    case GRUB_EFI_SCSI_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_scsi_device_path_t *scsi
		  = (grub_efi_scsi_device_path_t *) dp;
		grub_printf ("/SCSI(%x,%x)",
			     (unsigned) scsi->pun,
			     (unsigned) scsi->lun);
	      }
	      break;
	    case GRUB_EFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_fibre_channel_device_path_t *fc
		  = (grub_efi_fibre_channel_device_path_t *) dp;
		grub_printf ("/FibreChannel(%llx,%llx)",
			     (unsigned long long) fc->wwn,
			     (unsigned long long) fc->lun);
	      }
	      break;
	    case GRUB_EFI_1394_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_1394_device_path_t *firewire
		  = (grub_efi_1394_device_path_t *) dp;
		grub_printf ("/1394(%llx)",
			     (unsigned long long) firewire->guid);
	      }
	      break;
	    case GRUB_EFI_USB_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_usb_device_path_t *usb
		  = (grub_efi_usb_device_path_t *) dp;
		grub_printf ("/USB(%x,%x)",
			     (unsigned) usb->parent_port_number,
			     (unsigned) usb->usb_interface);
	      }
	      break;
	    case GRUB_EFI_USB_CLASS_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_usb_class_device_path_t *usb_class
		  = (grub_efi_usb_class_device_path_t *) dp;
		grub_printf ("/USBClass(%x,%x,%x,%x,%x)",
			     (unsigned) usb_class->vendor_id,
			     (unsigned) usb_class->product_id,
			     (unsigned) usb_class->device_class,
			     (unsigned) usb_class->device_subclass,
			     (unsigned) usb_class->device_protocol);
	      }
	      break;
	    case GRUB_EFI_I2O_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_i2o_device_path_t *i2o
		  = (grub_efi_i2o_device_path_t *) dp;
		grub_printf ("/I2O(%x)", (unsigned) i2o->tid);
	      }
	      break;
	    case GRUB_EFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_mac_address_device_path_t *mac
		  = (grub_efi_mac_address_device_path_t *) dp;
		grub_printf ("/MacAddr(%02x:%02x:%02x:%02x:%02x:%02x,%x)",
			     (unsigned) mac->mac_address[0],
			     (unsigned) mac->mac_address[1],
			     (unsigned) mac->mac_address[2],
			     (unsigned) mac->mac_address[3],
			     (unsigned) mac->mac_address[4],
			     (unsigned) mac->mac_address[5],
			     (unsigned) mac->if_type);
	      }
	      break;
	    case GRUB_EFI_IPV4_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_ipv4_device_path_t *ipv4
		  = (grub_efi_ipv4_device_path_t *) dp;
		grub_printf ("/IPv4(%u.%u.%u.%u,%u.%u.%u.%u,%u,%u,%x,%x)",
			     (unsigned) ipv4->local_ip_address[0],
			     (unsigned) ipv4->local_ip_address[1],
			     (unsigned) ipv4->local_ip_address[2],
			     (unsigned) ipv4->local_ip_address[3],
			     (unsigned) ipv4->remote_ip_address[0],
			     (unsigned) ipv4->remote_ip_address[1],
			     (unsigned) ipv4->remote_ip_address[2],
			     (unsigned) ipv4->remote_ip_address[3],
			     (unsigned) ipv4->local_port,
			     (unsigned) ipv4->remote_port,
			     (unsigned) ipv4->protocol,
			     (unsigned) ipv4->static_ip_address);
	      }
	      break;
	    case GRUB_EFI_IPV6_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_ipv6_device_path_t *ipv6
		  = (grub_efi_ipv6_device_path_t *) dp;
		grub_printf ("/IPv6(%x:%x:%x:%x:%x:%x:%x:%x,%x:%x:%x:%x:%x:%x:%x:%x,%u,%u,%x,%x)",
			     (unsigned) ipv6->local_ip_address[0],
			     (unsigned) ipv6->local_ip_address[1],
			     (unsigned) ipv6->local_ip_address[2],
			     (unsigned) ipv6->local_ip_address[3],
			     (unsigned) ipv6->local_ip_address[4],
			     (unsigned) ipv6->local_ip_address[5],
			     (unsigned) ipv6->local_ip_address[6],
			     (unsigned) ipv6->local_ip_address[7],
			     (unsigned) ipv6->remote_ip_address[0],
			     (unsigned) ipv6->remote_ip_address[1],
			     (unsigned) ipv6->remote_ip_address[2],
			     (unsigned) ipv6->remote_ip_address[3],
			     (unsigned) ipv6->remote_ip_address[4],
			     (unsigned) ipv6->remote_ip_address[5],
			     (unsigned) ipv6->remote_ip_address[6],
			     (unsigned) ipv6->remote_ip_address[7],
			     (unsigned) ipv6->local_port,
			     (unsigned) ipv6->remote_port,
			     (unsigned) ipv6->protocol,
			     (unsigned) ipv6->static_ip_address);
	      }
	      break;
	    case GRUB_EFI_INFINIBAND_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_infiniband_device_path_t *ib
		  = (grub_efi_infiniband_device_path_t *) dp;
		grub_printf ("/InfiniBand(%x,%llx,%llx,%llx)",
			     (unsigned) ib->port_gid[0], /* XXX */
			     (unsigned long long) ib->remote_id,
			     (unsigned long long) ib->target_port_id,
			     (unsigned long long) ib->device_id);
	      }
	      break;
	    case GRUB_EFI_UART_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_uart_device_path_t *uart
		  = (grub_efi_uart_device_path_t *) dp;
		grub_printf ("/UART(%llu,%u,%x,%x)",
			     (unsigned long long) uart->baud_rate,
			     uart->data_bits,
			     uart->parity,
			     uart->stop_bits);
	      }
	      break;
	    case GRUB_EFI_SATA_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_sata_device_path_t *sata;
		sata = (grub_efi_sata_device_path_t *) dp;
		grub_printf ("/Sata(%x,%x,%x)",
			     sata->hba_port,
			     sata->multiplier_port,
			     sata->lun);
	      }
	      break;

	    case GRUB_EFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE:
	      dump_vendor_path ("Messaging",
				(grub_efi_vendor_device_path_t *) dp);
	      break;
	    default:
	      grub_printf ("/UnknownMessaging(%x)", (unsigned) subtype);
	      break;
	    }
	  break;

	case GRUB_EFI_MEDIA_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_hard_drive_device_path_t *hd = (grub_efi_hard_drive_device_path_t *) dp;
		grub_printf ("/HD(%u,%llx,%llx,%02x%02x%02x%02x%02x%02x%02x%02x,%x,%x)",
			     hd->partition_number,
			     (unsigned long long) hd->partition_start,
			     (unsigned long long) hd->partition_size,
			     (unsigned) hd->partition_signature[0],
			     (unsigned) hd->partition_signature[1],
			     (unsigned) hd->partition_signature[2],
			     (unsigned) hd->partition_signature[3],
			     (unsigned) hd->partition_signature[4],
			     (unsigned) hd->partition_signature[5],
			     (unsigned) hd->partition_signature[6],
			     (unsigned) hd->partition_signature[7],
			     (unsigned) hd->partmap_type,
			     (unsigned) hd->signature_type);
	      }
	      break;
	    case GRUB_EFI_CDROM_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_cdrom_device_path_t *cd
		  = (grub_efi_cdrom_device_path_t *) dp;
		grub_printf ("/CD(%u,%llx,%llx)",
			     cd->boot_entry,
			     (unsigned long long) cd->partition_start,
			     (unsigned long long) cd->partition_size);
	      }
	      break;
	    case GRUB_EFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE:
	      dump_vendor_path ("Media",
				(grub_efi_vendor_device_path_t *) dp);
	      break;
	    case GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_file_path_device_path_t *fp;
		grub_uint8_t *buf;
		fp = (grub_efi_file_path_device_path_t *) dp;
		buf = grub_malloc ((len - 4) * 2 + 1);
		if (buf)
		  {
		    grub_efi_char16_t *dup_name = grub_malloc (len - 4);
		    if (!dup_name)
		      {
			grub_errno = GRUB_ERR_NONE;
			grub_printf ("/File((null))");
			grub_free (buf);
			break;
		      }
		    *grub_utf16_to_utf8 (buf, grub_memcpy (dup_name, fp->path_name, len - 4),
				       (len - 4) / sizeof (grub_efi_char16_t))
		      = '\0';
		    grub_free (dup_name);
		  }
		else
		  grub_errno = GRUB_ERR_NONE;
		grub_printf ("/File(%s)", buf);
		grub_free (buf);
	      }
	      break;
	    case GRUB_EFI_PROTOCOL_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_protocol_device_path_t *proto
		  = (grub_efi_protocol_device_path_t *) dp;
		grub_printf ("/Protocol(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
			     (unsigned) proto->guid.data1,
			     (unsigned) proto->guid.data2,
			     (unsigned) proto->guid.data3,
			     (unsigned) proto->guid.data4[0],
			     (unsigned) proto->guid.data4[1],
			     (unsigned) proto->guid.data4[2],
			     (unsigned) proto->guid.data4[3],
			     (unsigned) proto->guid.data4[4],
			     (unsigned) proto->guid.data4[5],
			     (unsigned) proto->guid.data4[6],
			     (unsigned) proto->guid.data4[7]);
	      }
	      break;
	    default:
	      grub_printf ("/UnknownMedia(%x)", (unsigned) subtype);
	      break;
	    }
	  break;

	case GRUB_EFI_BIOS_DEVICE_PATH_TYPE:
	  switch (subtype)
	    {
	    case GRUB_EFI_BIOS_DEVICE_PATH_SUBTYPE:
	      {
		grub_efi_bios_device_path_t *bios
		  = (grub_efi_bios_device_path_t *) dp;
		grub_printf ("/BIOS(%x,%x,%s)",
			     (unsigned) bios->device_type,
			     (unsigned) bios->status_flags,
			     (char *) (dp + 1));
	      }
	      break;
	    default:
	      grub_printf ("/UnknownBIOS(%x)", (unsigned) subtype);
	      break;
	    }
	  break;

	default:
	  grub_printf ("/UnknownType(%x,%x)\n",
		       (unsigned) type,
		       (unsigned) subtype);
	  return;
	  break;
	}

      if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (dp))
	break;

      dp = (grub_efi_device_path_t *) ((char *) dp + len);
    }
}

/* Compare device paths.  */
int
grub_efi_compare_device_paths (const grub_efi_device_path_t *dp1,
			       const grub_efi_device_path_t *dp2)
{
  if (! dp1 || ! dp2)
    /* Return non-zero.  */
    return 1;

  while (1)
    {
      grub_efi_uint8_t type1, type2;
      grub_efi_uint8_t subtype1, subtype2;
      grub_efi_uint16_t len1, len2;
      int ret;

      type1 = GRUB_EFI_DEVICE_PATH_TYPE (dp1);
      type2 = GRUB_EFI_DEVICE_PATH_TYPE (dp2);

      if (type1 != type2)
	return (int) type2 - (int) type1;

      subtype1 = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp1);
      subtype2 = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp2);

      if (subtype1 != subtype2)
	return (int) subtype1 - (int) subtype2;

      len1 = GRUB_EFI_DEVICE_PATH_LENGTH (dp1);
      len2 = GRUB_EFI_DEVICE_PATH_LENGTH (dp2);

      if (len1 != len2)
	return (int) len1 - (int) len2;

      ret = grub_memcmp (dp1, dp2, len1);
      if (ret != 0)
	return ret;

      if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (dp1))
	break;

      dp1 = (grub_efi_device_path_t *) ((char *) dp1 + len1);
      dp2 = (grub_efi_device_path_t *) ((char *) dp2 + len2);
    }

  return 0;
}

void * grub_efi_allocate_iso_buf(grub_uint64_t size)
{
    grub_efi_boot_services_t *b;
    grub_efi_status_t status;
    grub_efi_physical_address_t address = 0;
    grub_efi_uintn_t pages = GRUB_EFI_BYTES_TO_PAGES(size);

    b = grub_efi_system_table->boot_services;
    status = efi_call_4 (b->allocate_pages, GRUB_EFI_ALLOCATE_ANY_PAGES, GRUB_EFI_RUNTIME_SERVICES_DATA, pages, &address);
    if (status != GRUB_EFI_SUCCESS)
    {
        return NULL;
    }
    
    return (void *)(unsigned long)address;
}
