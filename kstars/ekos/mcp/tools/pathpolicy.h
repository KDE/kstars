/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace MCP::Tools
{

// Validates a user-supplied filesystem path against the configured MCP file
// access root (Options::mCPFileAccessRoot()). Only that directory and its
// subfolders are accessible to MCP tools; an empty root denies all file
// access. The path must be absolute. Returns the canonicalized absolute path
// on success; on failure returns an empty QString and populates `error`.
// Symlinks are resolved before the containment check, so a link escaping the
// root is rejected. With `requireExistingFile` the path must also resolve to
// an existing regular file; without it, a nonexistent target is validated
// through its parent directory (for future write tools).
//
// The check is advisory against a cooperative filesystem, not a sandbox:
// a broken symlink inside the root canonicalizes through the parent-directory
// branch and is approved, so a target appearing between validation and open
// is followed; directory components swapped in that window have the same
// shape (TOCTOU).
QString validatedPath(const QString &path, QString &error, bool requireExistingFile = false);

} // namespace MCP::Tools
