/* serial.h - serial device interface */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010  Free Software Foundation, Inc.
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

#ifndef GRUB_SERIAL_HEADER
#define GRUB_SERIAL_HEADER	1

#include <grub/types.h>
#if (defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)
#include <grub/cpu/io.h>
#endif
#include <grub/usb.h>
#include <grub/list.h>
#include <grub/term.h>
#ifdef GRUB_MACHINE_IEEE1275
#include <grub/ieee1275/ieee1275.h>
#endif
#ifdef GRUB_MACHINE_ARC
#include <grub/arc/arc.h>
#endif

struct grub_serial_port;
struct grub_serial_config;

struct grub_serial_driver
{
  grub_err_t (*configure) (struct grub_serial_port *port,
			   struct grub_serial_config *config);
  int (*fetch) (struct grub_serial_port *port);
  void (*put) (struct grub_serial_port *port, const int c);
  void (*fini) (struct grub_serial_port *port);
};

/* The type of parity.  */
typedef enum
  {
    GRUB_SERIAL_PARITY_NONE,
    GRUB_SERIAL_PARITY_ODD,
    GRUB_SERIAL_PARITY_EVEN,
  } grub_serial_parity_t;

typedef enum
  {
    GRUB_SERIAL_STOP_BITS_1,
    GRUB_SERIAL_STOP_BITS_1_5,
    GRUB_SERIAL_STOP_BITS_2,
  } grub_serial_stop_bits_t;

struct grub_serial_config
{
  unsigned speed;
  int word_len;
  grub_serial_parity_t parity;
  grub_serial_stop_bits_t stop_bits;
  grub_uint64_t base_clock;
  int rtscts;
};

struct grub_serial_port
{
  struct grub_serial_port *next;
  struct grub_serial_port **prev;
  char *name;
  struct grub_serial_driver *driver;
  struct grub_serial_config config;
  int configured;
  int broken;

  /* This should be void *data but since serial is useful as an early console
     when malloc isn't available it's a union.
   */
  union
  {
#if (defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)
    grub_port_t port;
#endif
    struct
    {
      grub_usb_device_t usbdev;
      int configno;
      int interfno;
      char buf[64];
      int bufstart, bufend;
      struct grub_usb_desc_endp *in_endp;
      struct grub_usb_desc_endp *out_endp;
    };
    struct grub_escc_descriptor *escc_desc;
#ifdef GRUB_MACHINE_IEEE1275
    struct
    {
      grub_ieee1275_ihandle_t handle;
      struct ofserial_hash_ent *elem;
    };
#endif
#ifdef GRUB_MACHINE_EFI
    struct grub_efi_serial_io_interface *interface;
#endif
#ifdef GRUB_MACHINE_ARC
    struct
    {
      grub_arc_fileno_t handle;
      int handle_valid;
    };
#endif
  };
  grub_term_output_t term_out;
  grub_term_input_t term_in;
};

grub_err_t EXPORT_FUNC(grub_serial_register) (struct grub_serial_port *port);

void EXPORT_FUNC(grub_serial_unregister) (struct grub_serial_port *port);

  /* Convenience functions to perform primitive operations on a port.  */
static inline grub_err_t
grub_serial_port_configure (struct grub_serial_port *port,
			    struct grub_serial_config *config)
{
  return port->driver->configure (port, config);
}

static inline int
grub_serial_port_fetch (struct grub_serial_port *port)
{
  return port->driver->fetch (port);
}

static inline void
grub_serial_port_put (struct grub_serial_port *port, const int c)
{
  port->driver->put (port, c);
}

static inline void
grub_serial_port_fini (struct grub_serial_port *port)
{
  port->driver->fini (port);
}

  /* Set default settings.  */
static inline grub_err_t
grub_serial_config_defaults (struct grub_serial_port *port)
{
  struct grub_serial_config config =
    {
#ifdef GRUB_MACHINE_MIPS_LOONGSON
      .speed = 115200,
      /* On Loongson machines serial port has only 3 wires.  */
      .rtscts = 0,
#else
      .speed = 9600,
      .rtscts = 1,
#endif
      .word_len = 8,
      .parity = GRUB_SERIAL_PARITY_NONE,
      .stop_bits = GRUB_SERIAL_STOP_BITS_1,
      .base_clock = 0
    };

  return port->driver->configure (port, &config);
}

#if (defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)
void grub_ns8250_init (void);
char *grub_serial_ns8250_add_port (grub_port_t port);
#endif
#ifdef GRUB_MACHINE_IEEE1275
void grub_ofserial_init (void);
#endif
#ifdef GRUB_MACHINE_EFI
void
grub_efiserial_init (void);
#endif
#ifdef GRUB_MACHINE_ARC
void
grub_arcserial_init (void);
const char *
grub_arcserial_add_port (const char *path);
#endif

struct grub_serial_port *grub_serial_find (const char *name);
extern struct grub_serial_driver grub_ns8250_driver;
void EXPORT_FUNC(grub_serial_unregister_driver) (struct grub_serial_driver *driver);

#ifndef GRUB_MACHINE_EMU
extern void grub_serial_init (void);
extern void grub_serial_fini (void);
#endif

#endif
