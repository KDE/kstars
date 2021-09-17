# -*- Mode:Python; indent-tabs-mode:nil; tab-width:4 -*-
#
# SPDX-FileCopyrightText: 2017 Harald Sitter <sitter@kde.org>
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""woosh woosh

  Simple magic. debs property is an array of debs that get pulled via apt
  and unpacked into the installdir for staging. Key difference to builtin
  stage-packages is that this entirely disregards dependencies, so they
  need to be resolved another way.
"""

import logging
import glob
import os
import re
import shutil
import subprocess

import snapcraft.plugins.make

logger = logging.getLogger(__name__)

class StabeDebsPlugin(snapcraft.BasePlugin):

    @classmethod
    def schema(cls):
        schema = super().schema()
        schema['properties']['debs'] = {
            'type': 'array',
            'minitems': 0,
            'uniqueItems': True,
            'items': {
                'type': 'string',
            }
        }

        schema['properties']['exclude-debs'] = {
            'type': 'array',
            'minitems': 0,
            'uniqueItems': True,
            'items': {
                'type': 'string',
            }
        }

        return schema

    @classmethod
    def get_build_properties(cls):
        # Inform Snapcraft of the properties associated with building. If these
        # change in the YAML Snapcraft will consider the build step dirty.
        return ['debs', 'exclude-debs']

    def __init__(self, name, options, project):
        super().__init__(name, options, project)

    def exclude(self, file):
        basename = os.path.basename(file)
        name = re.split('^(.+)_([^_]+)_([^_]+)\.deb$', basename)[1]
        return name in (self.options.exclude_debs or [])

    def build(self):
        super().build()

        logger.debug(os.getcwd())
        if self.options.debs:
            cmd = ['apt-get',
                   '-y',
                   '-o', 'Debug::NoLocking=true',
                   '-o', 'Dir::Cache::Archives=' + self.builddir,
                   '--reinstall',
                   '--download-only', 'install'] + self.options.debs
            subprocess.check_call(cmd, cwd=self.builddir)

        pkgs_abs_path = glob.glob(os.path.join(self.builddir, '*.deb'))
        for pkg in pkgs_abs_path:
            logger.debug(pkg)
            if self.exclude(pkg):
                continue
            logger.debug('extract')
            subprocess.check_call(['dpkg-deb', '--extract', pkg, self.installdir])

        # # Non-recursive stage, not sure this ever has a use case with
        # # exclusion in the picture
        # for deb in self.options.debs:
        #     logger.debug(deb)
        #     subprocess.check_call(['apt', 'download', deb])
        #
        # pkgs_abs_path = glob.glob(os.path.join(self.builddir, '*.deb'))
        # for pkg in pkgs_abs_path:
        #     logger.debug(pkg)
        #     subprocess.check_call(['dpkg-deb', '--extract', pkg, self.installdir])
