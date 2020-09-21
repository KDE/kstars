#!/usr/bin/python3
#
# Copyright (c) 2020 Akarsh Simha <akarsh@kde.org>
#
###########################################################################
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
###########################################################################

# Usage:
#
# First install the astroquery Python package and then provide the arguments
# (see --help for details)

import sys
import os
import argparse
import re
import logging
import datetime
import math

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger()

try:
    from tqdm import tqdm
except ModuleNotFoundError:
    logger.warning(f'To show progress bars, install the `tqdm` python package')
    def tqdm(*args, **kwargs):
        if len(args) == 1:
            return args[0]
        return args

from astroquery.vizier import Vizier

Vizier.ROW_LIMIT = -1 # Do not truncate catalogs

match_ra = re.compile(r' *([0-9][0-9]?)[:hH ] ?([0-9][0-9])[:mM ] ?([0-9][0-9](?:\.[0-9]+)?)[sS]? *')
match_dec = re.compile(r' *([+-]?)([0-9][0-9]?)[:dD ] ?([0-9][0-9])[:mM ] ?([0-9][0-9](?:\.[0-9]+)?)[sS]? *')
match_ra_hm = re.compile(r' *([0-9][0-9]?)[:hH ] ?([0-9][0-9](?:\.[0-9]+)?)[mM ]? *')
match_dec_hm = re.compile(r' *([+-]?)([0-9][0-9]?)[:dD ] ?([0-9][0-9](?:\.[0-9]+)?)[mM ]? *')

def convert_int(value):
    return int(value)

def convert_float(value):
    return f'{float(value):0.2f}'

def identity_converter(value):
    return value

def name_converter(name):
    name = name.strip(' ')
    if not (name.startswith('"') and name.endswith('"')):
        name = f'"{name}"'
    return name

def ra_converter(value):
    rah, ram, ras = match_ra.match(value).groups()
    return f'{int(rah):02}:{int(ram):02}:{float(ras):04.1f}'

def dec_converter(value):
    sgn, ded, dem, des = match_dec.match(value).groups()
    if sgn not in ('+', '-'):
        assert sgn == '', sgn
        sgn = '+'
    return f'{sgn}{int(ded):02}:{int(dem):02}:{float(des):04.1f}'

def ra_converter_hm(value):
    rah, ram = match_ra_hm.match(value).groups()
    ram = float(ram)
    ras = (ram - math.floor(ram)) * 60.0
    return f'{int(rah):02}:{int(ram):02}:{float(ras):04.1f}'

def dec_converter_hm(value):
    sgn, ded, dem = match_dec_hm.match(value).groups()
    dem = float(dem)
    des = (dem - math.floor(dem)) * 60.0
    if sgn not in ('+', '-'):
        assert sgn == '', sgn
        sgn = '+'
    return f'{sgn}{int(ded):02}:{int(dem):02}:{float(des):04.1f}'

def constant_converter(constant_value):
    # N.B. This function is different from the rest in that it returns a
    # function, i.e. is a factory
    return lambda *args, **kwargs: constant_value

KSTARS_TYPES = {
    # Copied from enum SkyObject::TYPE in skyobject.h
    'STAR':                   0,
    'CATALOG_STAR':           1,
    'PLANET':                 2,
    'OPEN_CLUSTER':           3,
    'GLOBULAR_CLUSTER':       4,
    'GASEOUS_NEBULA':         5,
    'PLANETARY_NEBULA':       6,
    'SUPERNOVA_REMNANT':      7,
    'GALAXY':                 8,
    'COMET':                  9,
    'ASTEROID':               10,
    'CONSTELLATION':          11,
    'MOON':                   12,
    'ASTERISM':               13,
    'GALAXY_CLUSTER':         14,
    'DARK_NEBULA':            15,
    'QUASAR':                 16,
    'MULT_STAR':              17,
    'RADIO_SOURCE':           18,
    'SATELLITE':              19,
    'SUPERNOVA':              20,
    'NUMBER_OF_KNOWN_TYPES':  21,
    'TYPE_UNKNOWN':           255,
}



CATALOG_CONVERSION_INFO = {
    #
    # This is a dictionary organized as follows:
    #
    # The `name` key maps to the name of the catalog in VizieR. To determine
    # this and the fields, use https://cdsarc.u-strasbg.fr/cgi-bin/cat
    #
    # The `tables` key (optional) maps to the names of VizieR tables to be
    # concatenated and used. The columns chosen must be present in all
    # tables. By default, only the first table is used
    #
    # The `header` key maps to the a dictionary with that must be written into
    # the KStars-readable header of the catalog
    #
    # The `columns` key maps to a dictionary mapping KStars column names to a
    # pair (VizieR-column-name, conversion function)
    #
    # The conversion function is applied to the data to get the KStars column
    # entry from the VizieR column entry
    #

    'Arp': {
        'name': 'VII/74AA',
        'header': {
            'Name': 'Atlas of Peculiar Galaxies',
            'Prefix': 'Arp',
            'Color': '#ffff99',
            'Epoch': 2000,
        },
        'columns': {
            'ID': ('APG', convert_int),
            'Nm': ('Name', name_converter),
            'RA': ('_RA.icrs', ra_converter),
            'Dc': ('_DE.icrs', dec_converter),
            'Tp': (None, constant_converter(KSTARS_TYPES['GALAXY'])),
        },
    },

    'ACO': {
        'name': 'VII/110A',
        'header': {
            'Name': 'Abell Clusters of Galaxies',
            'Prefix': 'ACO', # For Abell-Corwin-Orowin
            'Color': '#ffff00',
            'Epoch': 2000,
        },
        'tables': [
            'VII/110A/table3',
            'VII/110A/table4',
        ],
        'columns': {
            'ID': ('ACO', convert_int),
            'RA': ('_RA.icrs', ra_converter),
            'Dc': ('_DE.icrs', dec_converter),
            'Tp': (None, constant_converter(KSTARS_TYPES['GALAXY_CLUSTER'])),
            'Mg': ('m10', convert_float), # N.B. This is the magnitude of the 10th-ranked cluster member
        },
    },

    'UGC': {
        'name': 'VII/26D',
        'header': {
            'Name': 'Uppsala General Catalogue of Galaxies',
            'Prefix': 'UGC',
            'Color': '#ff3333',
            'Epoch': 1950,
        },
        'columns': {
            'ID': ('UGC', convert_int),
            'RA': ('RA1950', ra_converter_hm),
            'Dc': ('DE1950', dec_converter_hm),
            'Tp': (None, constant_converter(KSTARS_TYPES['GALAXY'])),
            'Mg': ('Pmag', convert_float),
            'Nm': ('MCG', name_converter),
        },
    },
}

def convert_catalog(conversion_info):
    logger.info(f'Fetching catalog {conversion_info["name"]} from VizieR')
    available_tables = Vizier.get_catalogs(conversion_info['name'])
    relevant_table_names = conversion_info.get('tables', [0])
    tables = [available_tables[tablename] for tablename in relevant_table_names]

    column_names = conversion_info['columns'].keys()
    rows = [
        '##',
        '## Data sourced from VizieR: https://vizier.u-strasbg.fr/viz-bin/VizieR',
        '## Free for scientific use provided credit is given in publications. NOT FOR COMMERCIAL USE!',
        '## See https://cds.u-strasbg.fr/vizier-org/licences_vizier.html for licensing',
        '## Converted for use in KStars on ' + datetime.datetime.utcnow().strftime('%Y-%m-%d'),
        '##',
    ] + [
        f'# {key}: {value}' for key, value in conversion_info['header'].items() # KStars-readable header
    ] + [
        '# ' + ' '.join(column_names)
    ]

    error_rows = 0
    total_rows = 0
    for idx, table in enumerate(tables):
        logger.info(f'Processing table #{idx+1}')
        for idx, row in enumerate(tqdm(table)):
            row_data = []
            for column in column_names:
                src_column, converter = conversion_info['columns'][column]
                try:
                    if src_column is not None:
                        row_data.append(converter(row[src_column]))
                    else:
                        row_data.append(converter()) # Constant-value columns
                except Exception as e:
                    error_rows += 1
                    row_data = None
                    logger.error(f'Error encountered while processing column {src_column} in row #{idx+1}: {e}\nRow contents: {row}. Will ignore this row.')
                    break
            if row_data is not None:
                assert len(row_data) == len(column_names), row_data
                rows.append(' '.join(map(str, row_data)))
            total_rows += 1

    logger.info(f'Encountered errors on {error_rows} out of {total_rows} ({round(error_rows * 100.0/total_rows, 2)}% errors)')

    return rows

def main(argv):
    parser = argparse.ArgumentParser(
        description=(
            'Download catalog data from VizieR (https://vizier.u-strasbg.fr/) and convert it into a format readable by KStars. '
            'Chances are this is a script that will generally be run very few times to generate the catalog data and upload to KNS3 servers. '
            'Note: As of this writing, VizieR data is licensed "free of usage in a scientific context". Commerical use requires clarification of the license. '
            'See https://cds.u-strasbg.fr/vizier-org/licences_vizier.html for details'
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        '--output-dir', '-o',
        help='Directory to which the output catalogs must be written.',
        required=True,
        type=str,
    )

    args = parser.parse_args(argv)
    output_dir = args.output_dir

    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    assert os.path.isdir(output_dir), output_dir

    for catalog, conversion_info in CATALOG_CONVERSION_INFO.items():
        rows = convert_catalog(conversion_info)
        with open(os.path.join(output_dir, f'{catalog}.cat'), 'w') as f:
            for row in rows:
                f.write(row + '\n')

if __name__ == "__main__":
    main(sys.argv[1:])
