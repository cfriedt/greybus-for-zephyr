/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANIFECTO_MANIFEST_H_
#define MANIFECTO_MANIFEST_H_

#include <stddef.h>
#include <stdint.h>

#include <dt-bindings/greybus/greybus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *manifest_t;

typedef uint8_t BundleClass;
typedef uint8_t CPortProtocol;
typedef uint8_t DescriptorType;

manifest_t manifest_new(void);
void manifest_fini(manifest_t *manifest);

int manifest_mnfs_parse(manifest_t manifest, const char *mnfs,
                        size_t mnfs_size);
int manifest_mnfs_parse_file(manifest_t manifest, const char *file);

int manifest_mnfb_gen(manifest_t manifest);
size_t manifest_mnfb_size(manifest_t manifest);
uint8_t *manifest_mnfb_data(manifest_t manifest);
int manifest_mnfb_give(manifest_t manifest, uint8_t **mnfb, size_t *mnfb_size);

int manifest_add_section(manifest_t manifest, const char *section_name);
int manifest_add_option(manifest_t manifest, const char *section_name,
                        const char *option_name, const char *option_value);

int manifest_add_header(manifest_t manifest, uint8_t major_, uint8_t minor_);
int manifest_add_interface_desc(manifest_t manifest, uint16_t vendor_string_id,
                                uint16_t product_string_id);
int manifest_add_string_desc(manifest_t manifest, uint8_t id,
                             const char *string_);
int manifest_add_bundle_desc(manifest_t manifest, uint8_t id,
                             BundleClass class_);
int manifest_add_cport_desc(manifest_t manifest, uint8_t id, BundleClass class_,
                            CPortProtocol protocol);
int manifest_get_cports(manifest_t manifest, unsigned int **cports, size_t *num_cports);


#ifdef __cplusplus
}
#endif

#endif /* MANIFECTO_MANIFEST_H_ */
