// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for the device-tree overlay handling of the PXE/extlinux label boot
 *
 * Copyright 2026 Thomas Perrot <thomas.perrot@bootlin.com>
 */

#include <env.h>
#include <image.h>
#include <mapmem.h>
#include <pxe_utils.h>
#include <string.h>
#include <linux/libfdt.h>
#include <test/ut.h>
#include "bootstd_common.h"

#define PXE_TEST_KERNEL_ADDR	0x1000000
#define PXE_TEST_FDT_ADDR	0x2000000
#define PXE_TEST_OVERLAY_ADDR	0x3000000

#define PXE_TEST_KERNEL_SIZE	64
#define PXE_TEST_BLOB_SIZE	1024
#define PXE_TEST_MAX_FILES	8
#define PXE_TEST_MAX_PATH	32

/**
 * struct pxe_test_info - files requested by the label boot
 *
 * @count: Number of files requested so far
 * @path: Path of each requested file, in request order
 */
struct pxe_test_info {
	int count;
	char path[PXE_TEST_MAX_FILES][PXE_TEST_MAX_PATH];
};

/**
 * pxe_test_blob() - Build an empty device tree, or an overlay
 *
 * @buf: Buffer to write to
 * @prop: Name of the property which the overlay adds to the root node, NULL
 *	to build an empty device tree instead
 * Return: 0 if OK, -ve on error
 */
static int pxe_test_blob(void *buf, const char *prop)
{
	int fragment, overlay, ret;

	ret = fdt_create_empty_tree(buf, PXE_TEST_BLOB_SIZE);
	if (ret || !prop)
		return ret;

	fragment = fdt_add_subnode(buf, 0, "fragment");
	if (fragment < 0)
		return fragment;
	ret = fdt_setprop_string(buf, fragment, "target-path", "/");
	if (ret)
		return ret;
	overlay = fdt_add_subnode(buf, fragment, "__overlay__");
	if (overlay < 0)
		return overlay;

	return fdt_setprop_string(buf, overlay, prop, "yes");
}

/**
 * pxe_test_getfile() - Provide the files of a label from memory
 *
 * The kernel is a blank buffer and the device tree is empty. Any other file is
 * an overlay adding a "yes" property, named after the file, to the root node.
 */
static int pxe_test_getfile(struct pxe_context *ctx, const char *file_path,
			    char *file_addr, enum bootflow_img_t type,
			    ulong *filesizep)
{
	struct pxe_test_info *info = ctx->userdata;
	void *buf = map_sysmem(hextoul(file_addr, NULL), 0);
	int ret;

	if (info->count == PXE_TEST_MAX_FILES)
		return -ENOSPC;
	strlcpy(info->path[info->count++], file_path, PXE_TEST_MAX_PATH);

	if (!strcmp(file_path, "kernel")) {
		memset(buf, '\0', PXE_TEST_KERNEL_SIZE);
		*filesizep = PXE_TEST_KERNEL_SIZE;
		return 0;
	}

	ret = pxe_test_blob(buf, strcmp(file_path, "board.dtb") ? file_path :
			    NULL);
	if (ret)
		return -EINVAL;
	*filesizep = fdt_totalsize(buf);

	return 0;
}

/* Environment variables which pxe_test_run() sets */
static const char *const pxe_test_vars[] = {
	"kernel_addr_r",
	"fdt_addr_r",
	"fdtoverlay_addr_r",
	"fdtoverlays",
};

/* Do the work of pxe_test_boot(), which restores the environment afterwards */
static int pxe_test_run(struct unit_test_state *uts,
			struct pxe_test_info *info, const char *env_overlays,
			const char *overlays)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	struct pxe_label *label;

	memset(info, '\0', sizeof(*info));
	ut_assertok(env_set("fdtoverlays", env_overlays));
	ut_assertok(env_set_hex("kernel_addr_r", PXE_TEST_KERNEL_ADDR));
	ut_assertok(env_set_hex("fdt_addr_r", PXE_TEST_FDT_ADDR));
	ut_assertok(env_set_hex("fdtoverlay_addr_r", PXE_TEST_OVERLAY_ADDR));

	ut_assertok(pxe_setup_ctx(&ctx, &cmdtp, pxe_test_getfile, info, false,
				  NULL, false, false));
	label = label_create();
	ut_assertnonnull(label);
	label->name = strdup("test");
	label->kernel_label = strdup("test");
	label->kernel = strdup("kernel");
	label->fdt = strdup("board.dtb");
	if (overlays)
		label->fdtoverlays = strdup(overlays);

	ut_asserteq(1, label_boot(&ctx, label));
	label_destroy(label);
	pxe_destroy_ctx(&ctx);

	return 0;
}

/**
 * pxe_test_boot() - Boot a label which has a kernel and a device tree
 *
 * The kernel is blank, so the boot fails once the device tree has been
 * prepared, which leaves it in memory for the caller to check. The
 * environment is restored afterwards.
 *
 * @uts: Test state
 * @info: Returns the files requested
 * @env_overlays: Value of the 'fdtoverlays' environment variable, NULL if
 *	unset
 * @overlays: Value of the label 'fdtoverlays' option, NULL if none
 * Return: 0 if OK, -ve on error
 */
static int pxe_test_boot(struct unit_test_state *uts,
			 struct pxe_test_info *info, const char *env_overlays,
			 const char *overlays)
{
	char *saved[ARRAY_SIZE(pxe_test_vars)] = {};
	const char *val;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(saved); i++) {
		val = env_get(pxe_test_vars[i]);
		if (val)
			saved[i] = strdup(val);
	}

	ret = pxe_test_run(uts, info, env_overlays, overlays);

	for (i = 0; i < ARRAY_SIZE(saved); i++) {
		env_set(pxe_test_vars[i], saved[i]);
		free(saved[i]);
	}

	return ret;
}

/* Check whether the overlay of a given file was applied to the device tree */
static bool pxe_test_applied(const char *name)
{
	return fdt_getprop(map_sysmem(PXE_TEST_FDT_ADDR, 0), 0, name, NULL);
}

/* Test that the 'fdtoverlays' environment variable is used without a label */
static int pxe_test_env_overlays(struct unit_test_state *uts)
{
	struct pxe_test_info info;

	ut_assertok(pxe_test_boot(uts, &info, "env-a.dtbo env-b.dtbo", NULL));
	ut_asserteq(4, info.count);
	ut_asserteq_str("env-a.dtbo", info.path[2]);
	ut_asserteq_str("env-b.dtbo", info.path[3]);
	ut_assert(pxe_test_applied("env-a.dtbo"));
	ut_assert(pxe_test_applied("env-b.dtbo"));

	return 0;
}
BOOTSTD_TEST(pxe_test_env_overlays, UTF_CONSOLE);

/* Test that the overlays of a label take precedence over the environment */
static int pxe_test_label_overlays(struct unit_test_state *uts)
{
	struct pxe_test_info info;

	ut_assertok(pxe_test_boot(uts, &info, "env-a.dtbo env-b.dtbo",
				  "label.dtbo"));
	ut_asserteq(3, info.count);
	ut_asserteq_str("label.dtbo", info.path[2]);
	ut_assert(pxe_test_applied("label.dtbo"));
	ut_assert(!pxe_test_applied("env-a.dtbo"));
	ut_assert(!pxe_test_applied("env-b.dtbo"));

	return 0;
}
BOOTSTD_TEST(pxe_test_label_overlays, UTF_CONSOLE);

/* Test that no overlay is loaded without a label option or a variable */
static int pxe_test_no_overlays(struct unit_test_state *uts)
{
	struct pxe_test_info info;

	ut_assertok(pxe_test_boot(uts, &info, NULL, NULL));
	ut_asserteq(2, info.count);

	return 0;
}
BOOTSTD_TEST(pxe_test_no_overlays, UTF_CONSOLE);
