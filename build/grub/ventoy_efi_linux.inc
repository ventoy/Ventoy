/* Ventoy Linux loader hooks.
 * Inserted into grub-core/loader/efi/linux.c, which GRUB 2.14 uses for
 * arm64-efi, loongarch64-efi, riscv64-efi and x86_64-efi.
 * grub_cmd_initrd is already defined above this block.
 */

static void
ventoy_debug_pause (void)
{
  char key;

  if (ventoy_debug == 0)
    return;

  grub_printf ("press Enter to continue ......\n");
  while (1)
    {
      key = grub_getkey ();
      if (key == '\n' || key == '\r')
	break;
    }
}

static int
ventoy_preboot (void)
{
  int i;
  const char *file;
  char buf[128];

  if (ventoy_debug)
    {
      grub_printf ("ventoy_preboot %d %d\n", ventoy_linux_argc, ventoy_initrd_called);
      ventoy_debug_pause ();
    }

  if (ventoy_linux_argc == 0)
    return 0;

  if (ventoy_initrd_called)
    {
      ventoy_initrd_called = 0;
      return 0;
    }

  grub_snprintf (buf, sizeof (buf), "mem:%s:size:%s",
		 grub_env_get ("ventoy_cpio_addr"),
		 grub_env_get ("ventoy_cpio_size"));
  ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (buf);

  file = grub_env_get ("vtoy_img_part_file");
  if (file)
    ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (file);

  if (ventoy_debug)
    {
      grub_printf ("========== initrd list ==========\n");
      for (i = 0; i < ventoy_extra_initrd_num; i++)
	grub_printf ("%s\n", ventoy_extra_initrd_list[i]);
      grub_printf ("=================================\n");
      ventoy_debug_pause ();
    }

  grub_cmd_initrd (NULL, ventoy_extra_initrd_num, ventoy_extra_initrd_list);
  return 0;
}

static int
ventoy_boot_opt_filter (char *opt)
{
  if (grub_strcmp (opt, "noinitrd") == 0)
    return 1;
  if (grub_strcmp (opt, "vga=current") == 0)
    return 1;

  if (grub_strncmp (opt, "rdinit=", 7) == 0)
    {
      if (grub_strcmp (opt, "rdinit=/vtoy/vtoy") != 0)
	{
	  opt[0] = 'v';
	  opt[1] = 't';
	}
      return 0;
    }

  if (grub_strncmp (opt, "init=", 5) == 0)
    {
      opt[0] = 'v';
      opt[1] = 't';
      return 0;
    }

  if (grub_strncmp (opt, "dm=", 3) == 0)
    {
      opt[0] = 'D';
      opt[1] = 'M';
      return 0;
    }

  if (ventoy_debug)
    {
      if (grub_strcmp (opt, "quiet") == 0)
	return 1;
      if (grub_strncmp (opt, "loglevel=", 9) == 0)
	return 1;
      if (grub_strcmp (opt, "splash") == 0)
	return 1;
    }

  return 0;
}

static int
ventoy_bootopt_hook (int argc, char *argv[])
{
  int i;
  int tmp_idx;
  int count = 0;
  const char *env;
  char c;
  char *newenv;
  char *last, *pos;

  if (ventoy_linux_argc == 0)
    return 0;

  for (tmp_idx = 0; tmp_idx < argc; tmp_idx++)
    {
      if (ventoy_boot_opt_filter (argv[tmp_idx]))
	continue;
      if (grub_strncmp (argv[tmp_idx], "--", 2) == 0)
	break;
      ventoy_linux_args[count++] = grub_strdup (argv[tmp_idx]);
    }

  for (i = 0; i < ventoy_linux_argc; i++)
    {
      ventoy_linux_args[count] = ventoy_linux_args[i + (LINUX_MAX_ARGC / 2)];
      ventoy_linux_args[i + (LINUX_MAX_ARGC / 2)] = NULL;

      if (ventoy_linux_args[count][0] == '@')
	{
	  env = grub_env_get (ventoy_linux_args[count] + 1);
	  if (env)
	    {
	      grub_free (ventoy_linux_args[count]);
	      newenv = grub_strdup (env);
	      last = newenv;
	      while (*last)
		{
		  while (*last && (*last == ' ' || *last == '\t'))
		    last++;
		  if (*last == 0)
		    break;
		  for (pos = last; *pos; pos++)
		    {
		      if (*pos == ' ' || *pos == '\t')
			{
			  c = *pos;
			  *pos = 0;
			  if (ventoy_boot_opt_filter (last) == 0)
			    ventoy_linux_args[count++] = grub_strdup (last);
			  *pos = c;
			  break;
			}
		    }
		  if (*pos == 0)
		    {
		      if (ventoy_boot_opt_filter (last) == 0)
			ventoy_linux_args[count++] = grub_strdup (last);
		      break;
		    }
		  last = pos + 1;
		}
	    }
	  else
	    count++;
	}
      else
	count++;
    }

  while (tmp_idx < argc)
    {
      if (ventoy_boot_opt_filter (argv[tmp_idx]))
	{
	  tmp_idx++;
	  continue;
	}
      ventoy_linux_args[count++] = grub_strdup (argv[tmp_idx]);
      tmp_idx++;
    }

  if (ventoy_debug)
    ventoy_linux_args[count++] = grub_strdup ("loglevel=7");

  ventoy_linux_argc = count;
  return 0;
}

static grub_err_t
grub_cmd_set_boot_opt (grub_command_t cmd __attribute__ ((unused)),
		       int argc, char *argv[])
{
  int i;
  const char *vtdebug;

  for (i = 0; i < argc; i++)
    {
      ventoy_linux_args[ventoy_linux_argc + (LINUX_MAX_ARGC / 2)] = grub_strdup (argv[i]);
      ventoy_linux_argc++;
    }

  vtdebug = grub_env_get ("vtdebug_flag");
  if (vtdebug && vtdebug[0])
    ventoy_debug = 1;
  return 0;
}

static grub_err_t
grub_cmd_unset_boot_opt (grub_command_t cmd __attribute__ ((unused)),
			 int argc __attribute__ ((unused)),
			 char *argv[] __attribute__ ((unused)))
{
  int i;

  for (i = 0; i < LINUX_MAX_ARGC; i++)
    {
      if (ventoy_linux_args[i])
	grub_free (ventoy_linux_args[i]);
    }

  ventoy_debug = 0;
  ventoy_linux_argc = 0;
  ventoy_initrd_called = 0;
  grub_memset (ventoy_linux_args, 0, sizeof (char *) * LINUX_MAX_ARGC);
  return 0;
}

static grub_err_t
grub_cmd_extra_initrd_append (grub_command_t cmd __attribute__ ((unused)),
			      int argc, char *argv[])
{
  grub_uint32_t i;
  int newclen = 0;
  char *pos = NULL;
  char *end = NULL;
  char buf[256] = { 0 };
  const char *suffix = NULL;
  const char *check_suffix[] = { ".ko", ".ko.zst", ".ko.xz", ".ko.gz" };

  if (argc != 1)
    return 1;

  if (grub_strstr (argv[0], ".ko"))
    {
      if (ventoy_check_file_exist ("%s", argv[0]))
	suffix = "";
    }
  else
    {
      for (i = 0; i < sizeof (check_suffix) / sizeof (check_suffix[0]); i++)
	{
	  if (ventoy_check_file_exist ("%s%s", argv[0], check_suffix[i]))
	    {
	      suffix = check_suffix[i];
	      break;
	    }
	}
    }

  if (!suffix)
    {
      grub_printf ("ko file <%s> not exist\n", argv[0]);
      return 0;
    }

  for (pos = argv[0]; *pos; pos++)
    {
      if (*pos == '/')
	end = pos;
    }

  if (end)
    {
      newclen = (int) grub_strlen (end + 1);
      if ((110 + newclen) % 4 == 0)
	grub_snprintf (buf, sizeof (buf), "newc:.%s%s:%s%s", end + 1, suffix, argv[0], suffix);
      else
	grub_snprintf (buf, sizeof (buf), "newc:%s%s:%s%s", end + 1, suffix, argv[0], suffix);

      if (ventoy_extra_initrd_num < 256)
	ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (buf);
    }

  return 0;
}

static grub_err_t
grub_cmd_extra_initrd_reset (grub_command_t cmd __attribute__ ((unused)),
			     int argc __attribute__ ((unused)),
			     char *argv[] __attribute__ ((unused)))
{
  int i;

  for (i = 0; i < ventoy_extra_initrd_num; i++)
    {
      if (ventoy_extra_initrd_list[i])
	grub_free (ventoy_extra_initrd_list[i]);
    }
  grub_memset (ventoy_extra_initrd_list, 0, sizeof (ventoy_extra_initrd_list));
  return 0;
}

static grub_err_t
ventoy_cmd_initrd (grub_command_t cmd, int argc, char *argv[])
{
  int i;
  const char *file;
  char buf[64];

  if (ventoy_linux_argc == 0)
    return grub_cmd_initrd (cmd, argc, argv);

  grub_snprintf (buf, sizeof (buf), "mem:%s:size:%s",
		 grub_env_get ("ventoy_cpio_addr"),
		 grub_env_get ("ventoy_cpio_size"));
  ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (buf);

  file = grub_env_get ("vtoy_img_part_file");
  if (file)
    ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (file);

  for (i = 0; i < argc; i++)
    ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup (argv[i]);

  ventoy_initrd_called = 1;
  return grub_cmd_initrd (cmd, ventoy_extra_initrd_num, ventoy_extra_initrd_list);
}
