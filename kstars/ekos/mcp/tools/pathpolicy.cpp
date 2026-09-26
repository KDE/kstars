/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pathpolicy.h"

#include "Options.h"

#include <QDir>
#include <QFileInfo>

namespace MCP::Tools
{

// True when `candidate` lies outside `root`. QDir::relativeFilePath is
// lexical and platform-aware: it handles the root "/" case (where a manual
// prefix check would test against "//"), separator differences, and
// cross-drive paths on Windows (absolute result).
static bool isOutsideRoot(const QString &root, const QString &candidate)
{
    const QString rel = QDir(root).relativeFilePath(candidate);
    return rel == QLatin1String("..") || rel.startsWith(QLatin1String("../"))
           || QDir::isAbsolutePath(rel);
}

QString validatedPath(const QString &path, QString &error, bool requireExistingFile)
{
    const QString rootSetting = Options::mCPFileAccessRoot();
    if (rootSetting.isEmpty())
    {
        error = QStringLiteral("MCP file access is disabled (no file access root configured)");
        return QString();
    }

    const QString root = QFileInfo(rootSetting).canonicalFilePath();
    if (root.isEmpty())
    {
        error = QStringLiteral("The configured MCP file access root does not exist");
        return QString();
    }

    const QFileInfo info(path);
    if (!info.isAbsolute())
    {
        error = QStringLiteral("Path '%1' must be absolute").arg(path);
        return QString();
    }

    // Lexical containment first, before any filesystem resolution: paths
    // outside the root are all rejected with this same message whether they
    // exist or not, so the error text can't be used to probe the filesystem
    // beyond the root.
    const QString lexical = QDir::cleanPath(info.absoluteFilePath());
    if (isOutsideRoot(root, lexical))
    {
        error = QStringLiteral("Path '%1' is outside the configured MCP file access root").arg(path);
        return QString();
    }

    // Canonicalize the target so symlinks are resolved before the containment
    // re-check. canonicalFilePath() returns empty for nonexistent files, so
    // fall back to canonicalizing the parent directory and re-appending the
    // file name — this keeps future write tools (target file not created yet)
    // usable under the policy.
    QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty())
    {
        const QString parent = info.absoluteDir().canonicalPath();
        if (parent.isEmpty())
        {
            error = QStringLiteral("Path '%1' cannot be resolved").arg(path);
            return QString();
        }
        canonical = parent + QLatin1Char('/') + info.fileName();
    }

    // Reaching here lexically inside the root but canonically outside means a
    // symlink escape. Symlinked data directories (USB/NAS mounts) are the
    // common innocent case, so name the resolved target in the error to make
    // the failure self-explanatory.
    if (isOutsideRoot(root, canonical))
    {
        error = QStringLiteral("Path '%1' is outside the configured MCP file access root "
                               "(symlinks are resolved before the check; it resolves to '%2')")
                .arg(path, canonical);
        return QString();
    }

    if (requireExistingFile && !QFileInfo(canonical).isFile())
    {
        error = QStringLiteral("Path '%1' is not an existing file").arg(path);
        return QString();
    }
    return canonical;
}

} // namespace MCP::Tools
