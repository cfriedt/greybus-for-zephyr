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

manifest_t manifest_new(void) { return calloc(1, sizeof(struct manifest_)); }

void manifest_fini(manifest_t *const manifest) {

  struct manifest_ *const man = (struct manifest_ *)*manifest;
  if (NULL == man) {
    return;
  }

  if (NULL != man->mnfb) {
    free(man->mnfb);
    man->mnfb = NULL;
  }
  man->mnfb_size = 0;

  if (NULL != man->sections) {
    for (size_t i = 0; i < man->num_sections; ++i) {
      struct manifest_section *section = &man->sections[i];
      for (size_t j = 0; j < section->num_options; ++j) {
        struct manifest_option *option = &section->options[j];
        free(option->name);
        free(option->value);
      }
      free(section->options);
      section->options = NULL;
      section->num_options = 0;

      free(section->name);
      section->name = NULL;
    }
    free(man->sections);
    man->sections = NULL;
    man->num_sections = 0;
  }

  if (NULL != man->descriptors) {
    for (size_t i = 0; i < man->num_descriptors; ++i) {
      free(man->descriptors[i]);
    }
    free(man->descriptors);
    man->descriptors = NULL;
    man->num_descriptors = 0;
  }

  free(*manifest);
  *manifest = NULL;
}

int manifest_add_section(manifest_t manifest, const char *section_name) {

  struct manifest_ *const man = (struct manifest_ *)manifest;
  struct manifest_section *tmp;
  struct manifest_section *section;

  if (NULL == manifest || NULL == section_name || 0 == strlen(section_name)) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_sections; ++i) {
    section = &man->sections[i];
    if (0 == strcmp(section_name, section->name)) {
      return -EALREADY;
    }
  }

  tmp =
      realloc(man->sections, (man->num_sections + 1) * sizeof(*man->sections));
  if (NULL == tmp) {
    return -ENOMEM;
  }

  man->sections = tmp;
  section = &man->sections[man->num_sections];
  man->num_sections++;

  section->name = (char *)section_name;
  section->options = NULL;
  section->num_options = 0;

  return 0;
}

int manifest_add_option(manifest_t manifest, const char *section_name,
                        const char *option_name, const char *option_value) {

  const struct manifest_ *man = (const struct manifest_ *)manifest;
  struct manifest_option *tmp;
  struct manifest_option *option;
  struct manifest_section *section;

  if (NULL == manifest || NULL == section_name || 0 == strlen(section_name) ||
      NULL == option_name || 0 == strlen(option_name) || NULL == option_value ||
      0 == strlen(option_value)) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_sections; ++i) {
    section = &man->sections[i];
    if (0 == strcmp(section_name, section->name)) {
      for (size_t j = 0; j < section->num_options; ++j) {
        option = &section->options[j];
        if (0 == strcmp(option_name, option->name)) {
          return -EALREADY;
        }
      }
      break;
    }
    section = NULL;
  }

  if (NULL == section) {
    return -ENOENT;
  }

  tmp = realloc(section->options,
                (section->num_options + 1) * sizeof(*section->options));
  if (NULL == tmp) {
    return -ENOMEM;
  }

  section->options = tmp;
  option = &section->options[section->num_options];
  section->num_options++;

  option->name = (char *)option_name;
  option->value = (char *)option_value;

  return 0;
}

int manifest_add_header(manifest_t manifest, uint8_t major_, uint8_t minor_) {

  struct manifest_ *const man = (struct manifest_ *)manifest;

  if (NULL == man) {
    return -EINVAL;
  }

  man->header.version_major = major_;
  man->header.version_minor = minor_;

  return 0;
}

static int manifest_add_desc(manifest_t manifest, struct descriptor *desc) {

  struct descriptor **tmp;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  tmp = realloc(man->descriptors, (man->num_descriptors + 1) * sizeof(*tmp));
  if (NULL == tmp) {
    return -ENOMEM;
  }
  man->descriptors = tmp;

  man->descriptors[man->num_descriptors] = desc;
  man->num_descriptors++;

  return 0;
}

int manifest_add_interface_desc(manifest_t manifest, uint16_t vendor_id,
                                uint16_t product_id) {

  int r;
  struct interface_descriptor *desc;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  if (NULL == man) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_descriptors; ++i) {
    if (DESC_TYPE_INTERFACE == man->descriptors[i]->type) {
      return -EALREADY;
    }
  }

  desc = malloc(sizeof(*desc));
  if (NULL == desc) {
    return -ENOMEM;
  }

  desc->type = DESC_TYPE_INTERFACE;
  desc->vendor_string_id = vendor_id;
  desc->product_string_id = product_id;

  r = manifest_add_desc(manifest, (struct descriptor *)desc);
  if (r < 0) {
    free(desc);
    return r;
  }

  return 0;
}

int manifest_add_string_desc(manifest_t manifest, uint8_t id,
                             const char *string_) {

  int r;
  struct string_descriptor *desc;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  if (NULL == man || NULL == string_) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_descriptors; ++i) {
    if (DESC_TYPE_STRING != man->descriptors[i]->type) {
      continue;
    }
    desc = (struct string_descriptor *)man->descriptors[i];
    if (desc->id == id) {
      return -EALREADY;
    }
  }

  desc = malloc(sizeof(*desc));
  if (NULL == desc) {
    return -ENOMEM;
  }

  desc->type = DESC_TYPE_STRING;
  desc->id = id;
  desc->string_ = string_;

  r = manifest_add_desc(manifest, (struct descriptor *)desc);
  if (r < 0) {
    free(desc);
    return r;
  }

  return 0;
}

int manifest_add_bundle_desc(manifest_t manifest, uint8_t id,
                             BundleClass class_) {

  int r;
  struct bundle_descriptor *desc;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  if (NULL == man) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_descriptors; ++i) {
    if (DESC_TYPE_BUNDLE != man->descriptors[i]->type) {
      continue;
    }
    desc = (struct bundle_descriptor *)man->descriptors[i];
    if (desc->id == id) {
      return -EALREADY;
    }
  }

  desc = malloc(sizeof(*desc));
  if (NULL == desc) {
    return -ENOMEM;
  }

  desc->type = DESC_TYPE_BUNDLE;
  desc->id = id;
  desc->class_ = class_;

  r = manifest_add_desc(manifest, (struct descriptor *)desc);
  if (r < 0) {
    free(desc);
    return r;
  }

  return 0;
}

int manifest_add_cport_desc(manifest_t manifest, uint8_t id, BundleClass class_,
                            CPortProtocol protocol) {
  int r;
  struct cport_descriptor *desc;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  if (NULL == man) {
    return -EINVAL;
  }

  for (size_t i = 0; i < man->num_descriptors; ++i) {

    if (DESC_TYPE_CPORT != man->descriptors[i]->type) {
      continue;
    }

    desc = (struct cport_descriptor *)man->descriptors[i];
    if (desc->id == id) {
      return -EALREADY;
    }
  }

  desc = malloc(sizeof(*desc));
  if (NULL == desc) {
    return -ENOMEM;
  }

  desc->type = DESC_TYPE_CPORT;
  desc->id = id;
  desc->class_ = class_;
  desc->protocol = protocol;

  r = manifest_add_desc(manifest, (struct descriptor *)desc);
  if (r < 0) {
    free(desc);
    return r;
  }

  return 0;
}

static int cport_comparator(const int *a, const int *b) {
  
  if (*a < *b) {
    return -1;
  }

  if (*a > *b) {
    return +1;
  }

  return 0;
}

static int manifest_cports_valid(unsigned int *cports, size_t num_cports) {

  typedef int (*qsort_comparator)(const void *, const void *);

  qsort(cports, num_cports, sizeof(*cports), (qsort_comparator)&cport_comparator);

  for(size_t i = 1; i < num_cports; ++i) {
    if (cports[i] != cports[i-1] + 1) {
      return false;
    }
  }

  return true;
}

int manifest_get_cports(manifest_t manifest, unsigned int **cports, size_t *num_cports) {

  int r;
  struct manifest_ *const man = (struct manifest_ *)manifest;
  unsigned int *cports_;
  size_t num_cports_;

  if (NULL == man || NULL == num_cports || NULL == cports) {
    return -EINVAL;
  }

  r = 0;
  for (size_t i = 0; i < man->num_descriptors; ++i) {
    if (DESC_TYPE_CPORT != man->descriptors[i]->type) {
      continue;
    }
    ++r;
  }

  *num_cports = 0;
  *cports = NULL;

  if (r == 0) {
    goto out;
  }

  num_cports_ = r;
  cports_ = malloc(num_cports_ * sizeof(*cports_));
  if (NULL == cports_) {
    r = -ENOMEM;
    goto out;
  }

  for (size_t i = 0, j = 0; i < man->num_descriptors && j < num_cports_; ++i) {
    if (DESC_TYPE_CPORT != man->descriptors[i]->type) {
      continue;
    }
    const struct cport_descriptor *const desc = (struct cport_descriptor *)man->descriptors[i];
    cports_[j] = desc->id;
    ++j;
  }

  r = manifest_cports_valid(cports_, num_cports_);
  if (r < 0) {
    goto free_cports;
  }

  *num_cports = num_cports_;
  *cports = cports_;

  r = 0;
  goto out;

free_cports:
  free(cports_);

out:
  return r;
}

static char *manifest_get_option(manifest_t manifest, const char *section_name,
                                 const char *option_name) {
  struct manifest_section *section;
  struct manifest_option *option;
  struct manifest_ *const man = (struct manifest_ *)manifest;

  for (size_t i = 0; i < man->num_sections; ++i) {
    section = &man->sections[i];
    if (0 == strcmp(section_name, section->name)) {
      for (size_t j = 0; j < section->num_options; ++j) {
        option = &section->options[j];
        if (0 == strcmp(option->name, option_name)) {
          return option->value;
        }
      }
    }
  }

  return NULL;
}

int manifest_get_int_option(manifest_t manifest, const char *section_name,
                            const char *option_name, uint8_t num_bytes) {

  int int_value;
  char *string_value;

  string_value = manifest_get_option(manifest, section_name, option_name);

  if (NULL == string_value) {
    return -ENOENT;
  }

  int_value = manifest_parse_int(string_value);
  if (int_value < 0) {
    return int_value;
  }

  int_value = manifest_check_int(int_value, num_bytes);

  return int_value;
}

char *manifest_get_string_option(manifest_t manifest, const char *section_name,
                                 const char *option_name) {
  return manifest_get_option(manifest, section_name, option_name);
}
