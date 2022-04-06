/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2022  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/efi/efi.h>
#include <grub/efi/api.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define GRUB_EFI_SIMPLE_POINTER_GUID  \
  { 0x31878c87, 0x0b75, 0x11d5, \
    { 0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

typedef struct
{
  grub_efi_int32_t x;
  grub_efi_int32_t y;
  grub_efi_int32_t z;
  grub_efi_boolean_t left;
  grub_efi_boolean_t right;
} grub_efi_mouse_state;

grub_efi_mouse_state no_move = {0, 0, 0, 0, 0};

typedef struct
{
  grub_efi_uint64_t x;
  grub_efi_uint64_t y;
  grub_efi_uint64_t z;
  grub_efi_boolean_t left;
  grub_efi_boolean_t right;
} grub_efi_mouse_mode;

struct grub_efi_simple_pointer_protocol
{
  grub_efi_status_t (*reset) (struct grub_efi_simple_pointer_protocol *this,
                              grub_efi_boolean_t extended_verification);
  grub_efi_status_t (*get_state) (struct grub_efi_simple_pointer_protocol *this,
                                  grub_efi_mouse_state *state);
  grub_efi_event_t *wait_for_input;
  grub_efi_mouse_mode *mode;
};
typedef struct grub_efi_simple_pointer_protocol grub_efi_simple_pointer_protocol_t;

typedef struct
{
  grub_efi_uintn_t count;
  grub_efi_simple_pointer_protocol_t **mouse;
} grub_efi_mouse_prot_t;

static grub_int32_t
mouse_div (grub_int32_t a, grub_uint64_t b)
{
  grub_int32_t s = 1, q, ret;
  grub_uint64_t n = a;
  if (!b)
    return 0;
  if (a < 0)
  {
    s = -1;
    n = -a;
  }
  q = grub_divmod64 (n, b, NULL);
  ret = s * (q > 0 ? q : -q);
  return ret;
}

static grub_efi_mouse_prot_t *
grub_efi_mouse_prot_init (void)
{
  grub_efi_status_t status;
  grub_efi_guid_t mouse_guid = GRUB_EFI_SIMPLE_POINTER_GUID;
  grub_efi_mouse_prot_t *mouse_input = NULL;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  grub_efi_handle_t *buf;
  grub_efi_uintn_t count;
  grub_efi_uintn_t i;

  status = efi_call_5 (b->locate_handle_buffer, GRUB_EFI_BY_PROTOCOL,
                       &mouse_guid, NULL, &count, &buf);
  if (status != GRUB_EFI_SUCCESS)
  {
#ifdef MOUSE_DEBUG
    grub_printf ("ERROR: SimplePointerProtocol not found.\n");
#endif
    return NULL;
  }

  mouse_input = grub_malloc (sizeof (grub_efi_mouse_prot_t));
  if (!mouse_input)
    goto end;
  mouse_input->mouse = grub_malloc (count
            * sizeof (grub_efi_simple_pointer_protocol_t *));
  if (!mouse_input->mouse)
  {
    grub_free (mouse_input);
    mouse_input = NULL;
    goto end;
  }
  mouse_input->count = count;
  for (i = 0; i < count; i++)
  {
    efi_call_3 (b->handle_protocol,
                buf[i], &mouse_guid, (void **)&mouse_input->mouse[i]);
#ifdef MOUSE_DEBUG
    grub_printf ("%d %p ", (int)i, mouse_input->mouse[i]);
#endif
    efi_call_2 (mouse_input->mouse[i]->reset, mouse_input->mouse[i], 1);
#ifdef MOUSE_DEBUG
    grub_printf
      ("[%"PRIuGRUB_UINT64_T"] [%"PRIuGRUB_UINT64_T"] [%"PRIuGRUB_UINT64_T"]\n",
       mouse_input->mouse[i]->mode->x,
       mouse_input->mouse[i]->mode->y, mouse_input->mouse[i]->mode->z);
#endif
  }
  
end:  
  efi_call_1(b->free_pool, buf);

  return mouse_input;
}

static grub_err_t
grub_efi_mouse_input_init (struct grub_term_input *term)
{
  grub_efi_mouse_prot_t *mouse_input = NULL;
  if (term->data)
    return 0;
  mouse_input = grub_efi_mouse_prot_init ();
  if (!mouse_input)
    return GRUB_ERR_BAD_OS;

  term->data = (void *)mouse_input;

  return 0;
}

static int
grub_mouse_getkey (struct grub_term_input *term)
{
  grub_efi_mouse_state cur;
  grub_efi_mouse_prot_t *mouse = term->data;
  //int x;
  int y;
  int delta = 0;
  const char *env;
  grub_efi_uintn_t i;
  if (!mouse)
    return GRUB_TERM_NO_KEY;

  env = grub_env_get("mouse_delta");
  if (env)
    delta = (int)grub_strtol(env, NULL, 10);
  
  for (i = 0; i < mouse->count; i++)
  {
    efi_call_2 (mouse->mouse[i]->get_state, mouse->mouse[i], &cur);
    if (grub_memcmp (&cur, &no_move, sizeof (grub_efi_mouse_state)) != 0)
    {
      y = mouse_div (cur.y, mouse->mouse[i]->mode->y);
      if (cur.left)
        return 0x0d;
      if (cur.right)
        return GRUB_TERM_ESC;
      if (y > delta)
        return GRUB_TERM_KEY_DOWN;
      if (y < -delta)
        return GRUB_TERM_KEY_UP;
    }
  }
  return GRUB_TERM_NO_KEY;
}

#ifdef MOUSE_DEBUG
static grub_err_t
grub_cmd_mouse_test (grub_command_t cmd __attribute__ ((unused)),
                    int argc __attribute__ ((unused)),
                    char **args __attribute__ ((unused)))

{
  grub_efi_mouse_state cur;
  int x = 0, y = 0, z = 0;
  grub_efi_uintn_t i;
  grub_efi_mouse_prot_t *mouse = NULL;

  mouse = grub_efi_mouse_prot_init ();
  if (!mouse)
    return grub_error (GRUB_ERR_BAD_OS, "mouse not found.\n");
  grub_printf ("Press [1] to exit.\n");
  while (1)
  {
    if (grub_getkey_noblock () == '1')
      break;
    for (i = 0; i < mouse->count; i++)
    {
      efi_call_2 (mouse->mouse[i]->get_state, mouse->mouse[i], &cur);
      if (grub_memcmp (&cur, &no_move, sizeof (grub_efi_mouse_state)) != 0)
      {
        x = mouse_div (cur.x, mouse->mouse[i]->mode->x);
        y = mouse_div (cur.y, mouse->mouse[i]->mode->y);
        z = mouse_div (cur.z, mouse->mouse[i]->mode->z);
        grub_printf ("[ID=%d] X=%d Y=%d Z=%d L=%d R=%d\n",
                     (int)i, x, y, z, cur.left, cur.right);
      }
    }
    grub_refresh ();
  }
  grub_free (mouse->mouse);
  grub_free (mouse);
  return GRUB_ERR_NONE;
}
static grub_command_t cmd;
#endif

static struct grub_term_input grub_mouse_term_input =
{
  .name = "mouse",
  .getkey = grub_mouse_getkey,
  .init = grub_efi_mouse_input_init,
};

GRUB_MOD_INIT(mouse)
{
  grub_term_register_input ("mouse", &grub_mouse_term_input);
#ifdef MOUSE_DEBUG
  cmd = grub_register_command ("mouse_test", grub_cmd_mouse_test, 0,
                               N_("UEFI mouse test."));
#endif
}

GRUB_MOD_FINI(mouse)
{
  grub_term_unregister_input (&grub_mouse_term_input);
#ifdef MOUSE_DEBUG
  grub_unregister_command (cmd);
#endif
}
