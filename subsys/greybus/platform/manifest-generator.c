/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "manifest_.h"

static void manifest_pack_header(manifest_t manifest);
static int manifest_pack_interface_desc(manifest_t manifest, size_t index);
static int manifest_pack_string_desc(manifest_t manifest, size_t index);
static int manifest_pack_bundle_desc(manifest_t manifest, size_t index);
static int manifest_pack_cport_desc(manifest_t manifest, size_t index);

int manifest_mnfb_gen(manifest_t manifest) {

  int r;
  uint8_t *tmp;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  tmp = realloc(man->mnfb, MNFS_HEADER_SIZE);
  if (NULL == tmp) {
    return -ENOMEM;
  }
  man->mnfb = tmp;
  man->mnfb_size = MNFS_HEADER_SIZE;

  for (size_t i = 0; i < man->num_descriptors; ++i) {
    switch (man->descriptors[i]->type) {
    case DESC_TYPE_INTERFACE:
      r = manifest_pack_interface_desc(manifest, i);
      break;
    case DESC_TYPE_STRING:
      r = manifest_pack_string_desc(manifest, i);
      break;
    case DESC_TYPE_BUNDLE:
      r = manifest_pack_bundle_desc(manifest, i);
      break;
    case DESC_TYPE_CPORT:
      r = manifest_pack_cport_desc(manifest, i);
      break;
    default:
      r = -EINVAL;
      break;
    }

    if (r < 0) {
      return r;
    }
  }

  manifest_pack_header(manifest);

  return 0;
}

size_t manifest_mnfb_size(manifest_t manifest) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  return (NULL == man) ? 0 : man->mnfb_size;
}

uint8_t *manifest_mnfb_data(manifest_t manifest) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  return (NULL == man) ? NULL : man->mnfb;
}

int manifest_mnfb_give(manifest_t manifest, uint8_t **mnfb, size_t *mnfb_size) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  if (NULL == man || NULL == mnfb || NULL == mnfb_size) {
    return -EINVAL;
  }
  *mnfb = man->mnfb;
  *mnfb_size = man->mnfb_size;
  man->mnfb = NULL;
  man->mnfb_size = 0;
  return 0;
}

static void manifest_pack_header(manifest_t manifest) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  // MNFS_HEADER_FMT = '<HBB' (le u16, u8, u8)
  man->mnfb[0] = (man->mnfb_size & 0x00ff) >> 0;
  man->mnfb[1] = (man->mnfb_size & 0xff00) >> 1;
  man->mnfb[2] = man->header.version_major;
  man->mnfb[3] = man->header.version_minor;
}

static int manifest_pack_desc(manifest_t manifest, uint16_t desc_size,
                              DescriptorType type, const void *data1,
                              uint16_t len1, const void *data2, uint16_t len2) {
  // BASE_DESC_FMT = '<HBx' (le u16, u8, pad byte)
  uint8_t *x;
  struct manifest_ *const man = (struct manifest_ *)manifest;
  x = realloc(man->mnfb, man->mnfb_size + desc_size);
  if (NULL == x) {
    return -ENOMEM;
  }
  man->mnfb = x;
  x = &man->mnfb[man->mnfb_size];
  man->mnfb_size += desc_size;
  *x++ = (desc_size & 0x00ff) >> 0;
  *x++ = (desc_size & 0xff00) >> 8;
  *x++ = (uint8_t)type;
  *x++ = 0;
  memcpy(x, data1, len1);
  x += len1;
  memcpy(x, data2, len2);
  x += len2;
  len2 += len1 + 4;
  for (; len2 < desc_size; ++len2) {
    *x++ = 0;
  }
  return 0;
}

static int manifest_pack_interface_desc(manifest_t manifest, size_t index) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  struct interface_descriptor *desc =
      (struct interface_descriptor *)man->descriptors[index];
  const uint8_t data[] = {
      desc->vendor_string_id,
      desc->product_string_id,
  };
  return manifest_pack_desc(manifest, INTERFACE_DESC_SIZE, DESC_TYPE_INTERFACE,
                            data, sizeof(data), NULL, 0);
}

static uint16_t string_desc_size(uint8_t string_size) {
  uint16_t base_size = STRING_DESC_BASE_SIZE + string_size;
  uint16_t mod = base_size % 4;
  uint16_t pad_bytes = mod ? (4 - mod) : 0;
  return base_size + pad_bytes;
}

static int manifest_pack_string_desc(manifest_t manifest, size_t index) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  struct string_descriptor *desc =
      (struct string_descriptor *)man->descriptors[index];
  uint8_t string_size = strlen(desc->string_);
  uint16_t desc_size = string_desc_size(string_size);
  if (desc_size > STRING_DESC_MAX_SIZE) {
    return -EINVAL;
  }
  uint8_t data[] = {
      string_size,
      desc->id,
  };
  return manifest_pack_desc(manifest, desc_size, DESC_TYPE_STRING, data,
                            sizeof(data), desc->string_, string_size);
}

static int manifest_pack_bundle_desc(manifest_t manifest, size_t index) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  struct bundle_descriptor *desc =
      (struct bundle_descriptor *)man->descriptors[index];
  const uint8_t data[] = {desc->id, desc->class_};
  return manifest_pack_desc(manifest, BUNDLE_DESC_SIZE, DESC_TYPE_BUNDLE, data,
                            sizeof(data), NULL, 0);
}

static int manifest_pack_cport_desc(manifest_t manifest, size_t index) {
  struct manifest_ *const man = (struct manifest_ *)manifest;
  struct cport_descriptor *desc =
      (struct cport_descriptor *)man->descriptors[index];
  const uint8_t data[] = {
      desc->id >> 0,
      desc->id >> 8,
      desc->class_,
      desc->protocol,
  };
  return manifest_pack_desc(manifest, CPORT_DESC_SIZE, DESC_TYPE_CPORT, data,
                            sizeof(data), NULL, 0);
}
