#!/bin/bash
# SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copy the Exposure Calculator camera data from https://github.com/indilib/camera-data into
# kstars/data/cameradata. Run before a release so that the bundled data matches the repository.
#
# Usage: tools/sync_camera_data.sh [path to a camera-data checkout]
# Without an argument, the repository is cloned into a temporary directory.

set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
target="${repo_root}/kstars/data/cameradata"
source_repo="${1:-}"

if [[ -z "${source_repo}" ]]; then
    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "${tmp_dir}"' EXIT
    git clone --quiet --depth 1 https://github.com/indilib/camera-data.git "${tmp_dir}/camera-data"
    source_repo="${tmp_dir}/camera-data"
fi

if [[ ! -d "${source_repo}/cameras" ]]; then
    echo "ERROR: ${source_repo} has no cameras directory" >&2
    exit 1
fi

if command -v python3 >/dev/null && [[ -f "${source_repo}/tools/camera_data.py" ]]; then
    python3 "${source_repo}/tools/camera_data.py" validate "${source_repo}"/cameras/*.xml
fi

rm -f "${target}"/*.xml
cp "${source_repo}"/cameras/*.xml "${target}/"

echo "Synced $(ls "${target}"/*.xml | wc -l) camera data files into ${target}"
git -C "${repo_root}" status --short -- "${target}"
