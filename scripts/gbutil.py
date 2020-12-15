#!/usr/bin/env python3

import os
import sys
from manifesto import *


def get_string_descriptors(defines, iface):
    sd = {}
    for key in defines:
        val = defines[key]
        if key.endswith('_P_compatible_IDX_0') and val == '"zephyr,greybus-string"':
            node = key[:-len('_P_compatible_IDX_0')]
            id_ = int(defines[node + '_P_id'])
            string = defines[node + '_P_greybus_string']
            sd[id_] = StringDescriptor(id_, string, None)
            if id_ == iface.vsid or id_ == iface.psid:
                sd[id_].parent = iface
    return sd


def get_interface_descriptor(defines):
    iface = None
    for key in defines:
        val = defines[key]
        if key.endswith('_P_compatible_IDX_0') and val == '"zephyr,greybus-interface"':
            node = key[:-len('_P_compatible_IDX_0')]
            vph = defines[node + '_P_vendor_string_id_IDX_0_PH']
            pph = defines[node + '_P_product_string_id_IDX_0_PH']
            vsid = int(defines[vph + '_P_id'])
            psid = int(defines[pph + '_P_id'])
            iface = InterfaceDescriptor(vsid, psid, None)
            break
    return iface


def get_bundle_descriptors(defines):
    bd = {}
    for key in defines:
        val = defines[key]
        if key.endswith('_P_compatible_IDX_0') and val == '"zephyr,greybus-bundle"':
            node = key[:-len('_P_compatible_IDX_0')]
            id_ = int(defines[node + '_P_id'])
            class_ = int(defines[node + '_P_bundle_class'])
            bd[id_] = BundleDescriptor(id_, class_, None)
    return bd


def get_cport_descriptors(defines):
    # add keys as necessary
    cport_keys = ['"zephyr,greybus-control"', '"zephyr,greybus-gpio-controller"',
                  '"zephyr,greybus-i2c-controller"', '"zephyr,greybus-spi-controller"']
    cd = {}
    for key in defines:
        val = defines[key]
        if key.endswith('_P_compatible_IDX_0') and val in cport_keys:
            node = key[:-len('_P_compatible_IDX_0')]
            id_ = int(defines[node + '_P_id'])
            bid = int(defines[defines[node + '_PARENT'] + '_P_id'])
            proto = int(defines[node + '_P_cport_protocol'])
            cd[id_] = CPortDescriptor(id_, bid, proto, None)
    return cd


def dt2mnfs(fn):

    # extract defines
    defines = {}
    with open(fn) as f:
        for line in f:
            line = line.strip()
            if line.startswith('#define '):
                line = line[len('#define '):]
                subs = line.split()
                key = subs[0]
                val = line[len(key + ' '):]
                val = val.strip()
                defines[key] = val

    interface_desc = get_interface_descriptor(defines)
    string_descs = get_string_descriptors(defines, interface_desc)
    bundle_descs = get_bundle_descriptors(defines)
    cport_descs = get_cport_descriptors(defines)

    m = Manifest()
    m.add_header(ManifestHeader(0, 1))

    for d in string_descs:
        m.add_string_desc(string_descs[d])
    m.add_interface_desc(interface_desc)
    for d in bundle_descs:
        m.add_bundle_desc(bundle_descs[d])
    for d in cport_descs:
        m.add_cport_desc(cport_descs[d])

    return m


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: {} <input> <output>'.format(sys.argv[0]))
        sys.exit(1)
    mnfs = dt2mnfs(sys.argv[1])
    with open(sys.argv[2], 'w') as f:
        f.write(str(mnfs))
    sys.exit(0)
