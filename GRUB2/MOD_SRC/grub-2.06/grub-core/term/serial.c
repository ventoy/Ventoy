/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2004,2005,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/serial.h>
#include <grub/term.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/terminfo.h>
#if !defined (GRUB_MACHINE_EMU) && ((defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__))
#include <grub/cpu/io.h>
#endif
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/list.h>
#ifdef GRUB_MACHINE_MIPS_LOONGSON
#include <grub/machine/kernel.h>
#endif
#ifdef GRUB_MACHINE_IEEE1275
#include <grub/ieee1275/console.h>
#endif

GRUB_MOD_LICENSE ("GPLv3+");

#define FOR_SERIAL_PORTS(var) FOR_LIST_ELEMENTS((var), (grub_serial_ports))

enum
  {
    OPTION_UNIT,
    OPTION_PORT,
    OPTION_SPEED,
    OPTION_WORD,
    OPTION_PARITY,
    OPTION_STOP,
    OPTION_BASE_CLOCK,
    OPTION_RTSCTS
  };

/* Argument options.  */
static const struct grub_arg_option options[] =
{
  {"unit",   'u', 0, N_("Set the serial unit."),             0, ARG_TYPE_INT},
  {"port",   'p', 0, N_("Set the serial port address."),     0, ARG_TYPE_STRING},
  {"speed",  's', 0, N_("Set the serial port speed."),       0, ARG_TYPE_INT},
  {"word",   'w', 0, N_("Set the serial port word length."), 0, ARG_TYPE_INT},
  {"parity", 'r', 0, N_("Set the serial port parity."),      0, ARG_TYPE_STRING},
  {"stop",   't', 0, N_("Set the serial port stop bits."),   0, ARG_TYPE_INT},
  {"base-clock",   'b', 0, N_("Set the base frequency."),   0, ARG_TYPE_STRING},
  {"rtscts",   'f', 0, N_("Enable/disable RTS/CTS."),   "on|off", ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

static struct grub_serial_port *grub_serial_ports;

struct grub_serial_output_state
{
  struct grub_terminfo_output_state tinfo;
  struct grub_serial_port *port;
};

struct grub_serial_input_state
{
  struct grub_terminfo_input_state tinfo;
  struct grub_serial_port *port;
};

static void 
serial_put (grub_term_output_t term, const int c)
{
  struct grub_serial_output_state *data = term->data;
  data->port->driver->put (data->port, c);
}

static int
serial_fetch (grub_term_input_t term)
{
  struct grub_serial_input_state *data = term->data;
  return data->port->driver->fetch (data->port);
}

static const struct grub_serial_input_state grub_serial_terminfo_input_template =
  {
    .tinfo =
    {
      .readkey = serial_fetch
    }
  };

static const struct grub_serial_output_state grub_serial_terminfo_output_template =
  {
    .tinfo =
    {
      .put = serial_put,
      .size = { 80, 24 }
    }
  };

static struct grub_serial_input_state grub_serial_terminfo_input;

static struct grub_serial_output_state grub_serial_terminfo_output;

static int registered = 0;

static struct grub_term_input grub_serial_term_input =
{
  .name = "serial",
  .init = grub_terminfo_input_init,
  .getkey = grub_terminfo_getkey,
  .data = &grub_serial_terminfo_input
};

static struct grub_term_output grub_serial_term_output =
{
  .name = "serial",
  .init = grub_terminfo_output_init,
  .putchar = grub_terminfo_putchar,
  .getwh = grub_terminfo_getwh,
  .getxy = grub_terminfo_getxy,
  .gotoxy = grub_terminfo_gotoxy,
  .cls = grub_terminfo_cls,
  .setcolorstate = grub_terminfo_setcolorstate,
  .setcursor = grub_terminfo_setcursor,
  .flags = GRUB_TERM_CODE_TYPE_ASCII,
  .data = &grub_serial_terminfo_output,
  .progress_update_divisor = GRUB_PROGRESS_SLOW
};



struct grub_serial_port *
grub_serial_find (const char *name)
{
  struct grub_serial_port *port;

  FOR_SERIAL_PORTS (port)
    if (grub_strcmp (port->name, name) == 0)
      break;

#if ((defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)) && !defined(GRUB_MACHINE_EMU) && !defined(GRUB_MACHINE_ARC)
  if (!port && grub_memcmp (name, "port", sizeof ("port") - 1) == 0
      && grub_isxdigit (name [sizeof ("port") - 1]))
    {
      name = grub_serial_ns8250_add_port (grub_strtoul (&name[sizeof ("port") - 1],
							0, 16));
      if (!name)
	return NULL;

      FOR_SERIAL_PORTS (port)
	if (grub_strcmp (port->name, name) == 0)
	  break;
    }
#endif

#ifdef GRUB_MACHINE_IEEE1275
  if (!port && grub_memcmp (name, "ieee1275/", sizeof ("ieee1275/") - 1) == 0)
    {
      name = grub_ofserial_add_port (&name[sizeof ("ieee1275/") - 1]);
      if (!name)
	return NULL;

      FOR_SERIAL_PORTS (port)
	if (grub_strcmp (port->name, name) == 0)
	  break;
    }
#endif

  return port;
}

static grub_err_t
grub_cmd_serial (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  char pname[40];
  const char *name = NULL;
  struct grub_serial_port *port;
  struct grub_serial_config config;
  grub_err_t err;

  if (state[OPTION_UNIT].set)
    {
      grub_snprintf (pname, sizeof (pname), "com%ld",
		     grub_strtoul (state[0].arg, 0, 0));
      name = pname;
    }

  if (state[OPTION_PORT].set)
    {
      grub_snprintf (pname, sizeof (pname), "port%lx",
		     grub_strtoul (state[1].arg, 0, 0));
      name = pname;
    }

  if (argc >= 1)
    name = args[0];

  if (!name)
    name = "com0";

  port = grub_serial_find (name);
  if (!port)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, 
		       N_("serial port `%s' isn't found"),
		       name);

  config = port->config;

  if (state[OPTION_SPEED].set) {
    config.speed = grub_strtoul (state[OPTION_SPEED].arg, 0, 0);
    if (config.speed == 0)
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
			 N_("unsupported serial port parity"));
  }

  if (state[OPTION_WORD].set)
    config.word_len = grub_strtoul (state[OPTION_WORD].arg, 0, 0);

  if (state[OPTION_PARITY].set)
    {
      if (! grub_strcmp (state[OPTION_PARITY].arg, "no"))
	config.parity = GRUB_SERIAL_PARITY_NONE;
      else if (! grub_strcmp (state[OPTION_PARITY].arg, "odd"))
	config.parity = GRUB_SERIAL_PARITY_ODD;
      else if (! grub_strcmp (state[OPTION_PARITY].arg, "even"))
	config.parity = GRUB_SERIAL_PARITY_EVEN;
      else
	return grub_error (GRUB_ERR_BAD_ARGUMENT,
			   N_("unsupported serial port parity"));
    }

  if (state[OPTION_RTSCTS].set)
    {
      if (grub_strcmp (state[OPTION_RTSCTS].arg, "on") == 0)
	config.rtscts = 1;
      else if (grub_strcmp (state[OPTION_RTSCTS].arg, "off") == 0)
	config.rtscts = 0;
      else
	return grub_error (GRUB_ERR_BAD_ARGUMENT,
			   N_("unsupported serial port flow control"));
    }

  if (state[OPTION_STOP].set)
    {
      if (! grub_strcmp (state[OPTION_STOP].arg, "1"))
	config.stop_bits = GRUB_SERIAL_STOP_BITS_1;
      else if (! grub_strcmp (state[OPTION_STOP].arg, "2"))
	config.stop_bits = GRUB_SERIAL_STOP_BITS_2;
      else if (! grub_strcmp (state[OPTION_STOP].arg, "1.5"))
	config.stop_bits = GRUB_SERIAL_STOP_BITS_1_5;
      else
	return grub_error (GRUB_ERR_BAD_ARGUMENT,
			   N_("unsupported serial port stop bits number"));
    }

  if (state[OPTION_BASE_CLOCK].set)
    {
      char *ptr;
      config.base_clock = grub_strtoull (state[OPTION_BASE_CLOCK].arg, &ptr, 0);
      if (grub_errno)
	return grub_errno;
      if (ptr && *ptr == 'M')
	config.base_clock *= 1000000;
      if (ptr && (*ptr == 'k' || *ptr == 'K'))
	config.base_clock *= 1000;
    }

  if (config.speed == 0)
    config.speed = 9600;

  /* Initialize with new settings.  */
  err = port->driver->configure (port, &config);
  if (err)
    return err;
#if !defined (GRUB_MACHINE_EMU) && !defined(GRUB_MACHINE_ARC) && ((defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__))

  /* Compatibility kludge.  */
  if (port->driver == &grub_ns8250_driver)
    {
      if (!registered)
	{
	  grub_terminfo_output_register (&grub_serial_term_output, "vt100");

	  grub_term_register_input ("serial", &grub_serial_term_input);
	  grub_term_register_output ("serial", &grub_serial_term_output);
	}
      grub_serial_terminfo_output.port = port;
      grub_serial_terminfo_input.port = port;
      registered = 1;
    }
#endif
  return GRUB_ERR_NONE;
}

#ifdef GRUB_MACHINE_MIPS_LOONGSON
const char loongson_defserial[][6] =
  {
    [GRUB_ARCH_MACHINE_YEELOONG] = "com0",
    [GRUB_ARCH_MACHINE_FULOONG2F]  = "com2",
    [GRUB_ARCH_MACHINE_FULOONG2E]  = "com1"
  };
#endif

grub_err_t
grub_serial_register (struct grub_serial_port *port)
{
  struct grub_term_input *in;
  struct grub_term_output *out;
  struct grub_serial_input_state *indata;
  struct grub_serial_output_state *outdata;

  in = grub_malloc (sizeof (*in));
  if (!in)
    return grub_errno;

  indata = grub_malloc (sizeof (*indata));
  if (!indata)
    {
      grub_free (in);
      return grub_errno;
    }

  grub_memcpy (in, &grub_serial_term_input, sizeof (*in));
  in->data = indata;
  in->name = grub_xasprintf ("serial_%s", port->name);
  grub_memcpy (indata, &grub_serial_terminfo_input, sizeof (*indata));

  if (!in->name)
    {
      grub_free (in);
      grub_free (indata);
      return grub_errno;
    }

  out = grub_zalloc (sizeof (*out));
  if (!out)
    {
      grub_free (indata);
      grub_free ((char *) in->name);
      grub_free (in);
      return grub_errno;
    }

  outdata = grub_malloc (sizeof (*outdata));
  if (!outdata)
    {
      grub_free (indata);
      grub_free ((char *) in->name);
      grub_free (out);
      grub_free (in);
      return grub_errno;
    }

  grub_memcpy (out, &grub_serial_term_output, sizeof (*out));
  out->data = outdata;
  out->name = in->name;
  grub_memcpy (outdata, &grub_serial_terminfo_output, sizeof (*outdata));

  grub_list_push (GRUB_AS_LIST_P (&grub_serial_ports), GRUB_AS_LIST (port));
  ((struct grub_serial_input_state *) in->data)->port = port;
  ((struct grub_serial_output_state *) out->data)->port = port;
  port->term_in = in;
  port->term_out = out;
  grub_terminfo_output_register (out, "vt100");
#ifdef GRUB_MACHINE_MIPS_LOONGSON
  if (grub_strcmp (port->name, loongson_defserial[grub_arch_machine]) == 0)
    {
      grub_term_register_input_active ("serial_*", in);
      grub_term_register_output_active ("serial_*", out);
    }
  else
    {
      grub_term_register_input_inactive ("serial_*", in);
      grub_term_register_output_inactive ("serial_*", out);
    }
#else
  grub_term_register_input ("serial_*", in);
  grub_term_register_output ("serial_*", out);
#endif

  return GRUB_ERR_NONE;
}

void
grub_serial_unregister (struct grub_serial_port *port)
{
  if (port->driver->fini)
    port->driver->fini (port);
  
  if (port->term_in)
    grub_term_unregister_input (port->term_in);
  if (port->term_out)
    grub_term_unregister_output (port->term_out);

  grub_list_remove (GRUB_AS_LIST (port));
}

void
grub_serial_unregister_driver (struct grub_serial_driver *driver)
{
  struct grub_serial_port *port, *next;
  for (port = grub_serial_ports; port; port = next)
    {
      next = port->next;
      if (port->driver == driver)
	grub_serial_unregister (port);
    }
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(serial)
{
  cmd = grub_register_extcmd ("serial", grub_cmd_serial, 0,
			      N_("[OPTIONS...]"),
			      N_("Configure serial port."), options);
  grub_memcpy (&grub_serial_terminfo_output,
	       &grub_serial_terminfo_output_template,
	       sizeof (grub_serial_terminfo_output));

  grub_memcpy (&grub_serial_terminfo_input,
	       &grub_serial_terminfo_input_template,
	       sizeof (grub_serial_terminfo_input));

#if !defined (GRUB_MACHINE_EMU) && !defined(GRUB_MACHINE_ARC) && ((defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__))
  grub_ns8250_init ();
#endif
#ifdef GRUB_MACHINE_IEEE1275
  grub_ofserial_init ();
#endif
#ifdef GRUB_MACHINE_EFI
  grub_efiserial_init ();
#endif
#ifdef GRUB_MACHINE_ARC
  grub_arcserial_init ();
#endif
}

GRUB_MOD_FINI(serial)
{
  while (grub_serial_ports)
    grub_serial_unregister (grub_serial_ports);
  if (registered)
    {
      grub_term_unregister_input (&grub_serial_term_input);
      grub_term_unregister_output (&grub_serial_term_output);
    }
  grub_unregister_extcmd (cmd);
}
