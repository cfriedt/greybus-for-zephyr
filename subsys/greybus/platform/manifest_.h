/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANIFECTO_MANIFEST__H_
#define MANIFECTO_MANIFEST__H_

#include <greybus/manifecto/manifest.h>

#define MNFS_HEADER_VERSION_SIZE 1
#define ID_DESC_SIZE 1
#define STRING_DESC_STRING_SIZE 255
#define BUNDLE_DESC_CLASS_SIZE 1
#define CPORT_ID_DESC_SIZE 2
#define CPORT_DESC_PROTOCOL_SIZE 1

#define MNFS_MAX_SIZE 0xffff
#define MNFS_HEADER_SIZE 0x4
#define BASE_DESC_SIZE 0x4
#define INTERFACE_DESC_SIZE (BASE_DESC_SIZE + 0x4)
#define STRING_DESC_BASE_SIZE (BASE_DESC_SIZE + 0x2)
#define BUNDLE_DESC_SIZE (BASE_DESC_SIZE + 0x4)
#define CPORT_DESC_SIZE (BASE_DESC_SIZE + 0x4)

#define STRING_MAX_SIZE 0xff
#define STRING_DESC_MAX_SIZE (STRING_DESC_BASE_SIZE + STRING_MAX_SIZE)

#ifdef __cplusplus
extern "C" {
#endif

struct manifest_option {
  char *name;
  char *value;
};

struct manifest_section {
  char *name;
  uint8_t num_options;
  struct manifest_option *options;
};

struct manifest_header {
  uint8_t version_major;
  uint8_t version_minor;
};

struct descriptor {
  DescriptorType type;
};

struct interface_descriptor {
  DescriptorType type;
  uint8_t vendor_string_id;
  uint8_t product_string_id;
};

struct bundle_descriptor {
  DescriptorType type;
  uint8_t id;
  BundleClass class_;
};

struct string_descriptor {
  DescriptorType type;
  uint8_t id;
  const char *string_;
};

struct cport_descriptor {
  DescriptorType type;
  uint8_t id;
  BundleClass class_;
  CPortProtocol protocol;
};

struct manifest_ {

  struct manifest_section *sections;
  size_t num_sections;

  struct manifest_header header;
  bool has_interface_desc;
  bool has_control_cport;

  struct descriptor **descriptors;
  size_t num_descriptors;

  uint8_t *mnfb;
  size_t mnfb_size;
};

int manifest_get_int_option(manifest_t manifest, const char *section_name,
                            const char *option_name, uint8_t num_bytes);
char *manifest_get_string_option(manifest_t manifest, const char *section_name,
                                 const char *option_name);

int manifest_check_int(int int_val, uint8_t num_bytes);
int manifest_parse_int(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* MANIFECTO_MANIFEST__H_ */
