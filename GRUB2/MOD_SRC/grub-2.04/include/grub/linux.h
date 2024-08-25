#include <grub/file.h>

struct grub_linux_initrd_component;

struct grub_linux_initrd_context
{
  int nfiles;
  struct grub_linux_initrd_component *components;
  grub_size_t size;
};

grub_err_t
grub_initrd_init (int argc, char *argv[],
		  struct grub_linux_initrd_context *ctx);

grub_size_t
grub_get_initrd_size (struct grub_linux_initrd_context *ctx);

void
grub_initrd_close (struct grub_linux_initrd_context *initrd_ctx);

grub_err_t
grub_initrd_load (struct grub_linux_initrd_context *initrd_ctx,
		  char *argv[], void *target);
