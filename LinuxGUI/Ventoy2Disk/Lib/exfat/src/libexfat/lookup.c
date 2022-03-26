/*
	lookup.c (02.09.09)
	exFAT file system implementation library.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "exfat.h"
#include <string.h>
#include <errno.h>
#include <inttypes.h>

int exfat_opendir(struct exfat* ef, struct exfat_node* dir,
		struct exfat_iterator* it)
{
	int rc;

	exfat_get_node(dir);
	it->parent = dir;
	it->current = NULL;
	rc = exfat_cache_directory(ef, dir);
	if (rc != 0)
		exfat_put_node(ef, dir);
	return rc;
}

void exfat_closedir(struct exfat* ef, struct exfat_iterator* it)
{
	exfat_put_node(ef, it->parent);
	it->parent = NULL;
	it->current = NULL;
}

struct exfat_node* exfat_readdir(struct exfat_iterator* it)
{
	if (it->current == NULL)
		it->current = it->parent->child;
	else
		it->current = it->current->next;

	if (it->current != NULL)
		return exfat_get_node(it->current);
	else
		return NULL;
}

static int compare_char(struct exfat* ef, uint16_t a, uint16_t b)
{
	return (int) ef->upcase[a] - (int) ef->upcase[b];
}

static int compare_name(struct exfat* ef, const le16_t* a, const le16_t* b)
{
	while (le16_to_cpu(*a) && le16_to_cpu(*b))
	{
		int rc = compare_char(ef, le16_to_cpu(*a), le16_to_cpu(*b));
		if (rc != 0)
			return rc;
		a++;
		b++;
	}
	return compare_char(ef, le16_to_cpu(*a), le16_to_cpu(*b));
}

static int lookup_name(struct exfat* ef, struct exfat_node* parent,
		struct exfat_node** node, const char* name, size_t n)
{
	struct exfat_iterator it;
	le16_t buffer[EXFAT_NAME_MAX + 1];
	int rc;

	*node = NULL;

	rc = utf8_to_utf16(buffer, name, EXFAT_NAME_MAX + 1, n);
	if (rc != 0)
		return rc;

	rc = exfat_opendir(ef, parent, &it);
	if (rc != 0)
		return rc;
	while ((*node = exfat_readdir(&it)))
	{
		if (compare_name(ef, buffer, (*node)->name) == 0)
		{
			exfat_closedir(ef, &it);
			return 0;
		}
		exfat_put_node(ef, *node);
	}
	exfat_closedir(ef, &it);
	return -ENOENT;
}

static size_t get_comp(const char* path, const char** comp)
{
	const char* end;

	*comp = path + strspn(path, "/");				/* skip leading slashes */
	end = strchr(*comp, '/');
	if (end == NULL)
		return strlen(*comp);
	else
		return end - *comp;
}

int exfat_lookup(struct exfat* ef, struct exfat_node** node,
		const char* path)
{
	struct exfat_node* parent;
	const char* p;
	size_t n;
	int rc;

	/* start from the root directory */
	parent = *node = exfat_get_node(ef->root);
	for (p = path; (n = get_comp(p, &p)); p += n)
	{
		if (n == 1 && *p == '.')				/* skip "." component */
			continue;
		rc = lookup_name(ef, parent, node, p, n);
		if (rc != 0)
		{
			exfat_put_node(ef, parent);
			return rc;
		}
		exfat_put_node(ef, parent);
		parent = *node;
	}
	return 0;
}

static bool is_last_comp(const char* comp, size_t length)
{
	const char* p = comp + length;

	return get_comp(p, &p) == 0;
}

static bool is_allowed(const char* comp, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
		switch (comp[i])
		{
		case 0x01 ... 0x1f:
		case '/':
		case '\\':
		case ':':
		case '*':
		case '?':
		case '"':
		case '<':
		case '>':
		case '|':
			return false;
		}
	return true;
}

int exfat_split(struct exfat* ef, struct exfat_node** parent,
		struct exfat_node** node, le16_t* name, const char* path)
{
	const char* p;
	size_t n;
	int rc;

	memset(name, 0, (EXFAT_NAME_MAX + 1) * sizeof(le16_t));
	*parent = *node = exfat_get_node(ef->root);
	for (p = path; (n = get_comp(p, &p)); p += n)
	{
		if (n == 1 && *p == '.')
			continue;
		if (is_last_comp(p, n))
		{
			if (!is_allowed(p, n))
			{
				/* contains characters that are not allowed */
				exfat_put_node(ef, *parent);
				return -ENOENT;
			}
			rc = utf8_to_utf16(name, p, EXFAT_NAME_MAX + 1, n);
			if (rc != 0)
			{
				exfat_put_node(ef, *parent);
				return rc;
			}

			rc = lookup_name(ef, *parent, node, p, n);
			if (rc != 0 && rc != -ENOENT)
			{
				exfat_put_node(ef, *parent);
				return rc;
			}
			return 0;
		}
		rc = lookup_name(ef, *parent, node, p, n);
		if (rc != 0)
		{
			exfat_put_node(ef, *parent);
			return rc;
		}
		exfat_put_node(ef, *parent);
		*parent = *node;
	}
	exfat_bug("impossible");

    return 1;
}
