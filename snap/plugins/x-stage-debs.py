# -*- Mode:Python; indent-tabs-mode:nil; tab-width:4 -*-
#
# Copyright (C) 2017 Harald Sitter <sitter@kde.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
