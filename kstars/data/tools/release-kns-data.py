#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Prepare a release of the KStars KNewStuff data files (star catalogs, terrains, ...) on the KDE CDN.

KStars finds its downloadable data through kstars/data/kstars.knsrc, whose providers
(https://cdn.kde.org/edu/kstars/providers.xml) list main.xml and dso.xml. Every file those
lists point to must be hosted on the KDE CDN under https://cdn.kde.org/edu/kstars/.

Like releaseme's tarme.rb does for the source tarball, this script does not publish anything
itself: it checks and signs the files, then prints the upload commands and a prefilled
sysadmin ticket asking to place them on the CDN.

  fetch    Download the live main.xml and dso.xml into the release folder, to be edited there
           (new versions, payload names, checksums, sizes).

  mirror   Download every payload and preview that the XML files in the release folder point to
           outside the KDE CDN, and rewrite those URLs to the CDN (payloads in edu/kstars/,
           previews in edu/kstars/previews/).

  prepare  Check the release folder and print what to upload:
           - every payload and preview URL is on the KDE CDN;
           - every file of the folder is referenced by an XML file, and every referenced file is
             either in the folder (to upload) or already on the CDN;
           - the md5 checksum and size given in the XML match the local payloads;
           then sign each file to upload with gpg2 (as releaseme does) and print the kdecp
           commands and the sysadmin ticket link (SHA-256 and SHA-1 of every file).

Typical use:
    kstars/data/tools/release-kns-data.py fetch   /tmp/kns
    # edit /tmp/kns/main.xml, copy the new payloads into /tmp/kns (previews into /tmp/kns/previews)
    kstars/data/tools/release-kns-data.py mirror  /tmp/kns
    kstars/data/tools/release-kns-data.py prepare /tmp/kns
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

CDN = 'https://cdn.kde.org/edu/kstars/'
CDN_TARGET = 'edu/kstars'  # the CDN path, as the sysadmin ticket names it
CATALOGS = ('main.xml', 'dso.xml')
UPLOAD = 'ftp://upload.kde.org/incoming'
TICKET_FORM = 'https://phabricator.kde.org/maniphest/task/edit/form/2'
USER_AGENT = 'kstars-release-kns-data'

URL_TAGS = re.compile(r'(<(payload|preview)\b[^>]*>)\s*([^<\s]+)\s*(</\2>)')
STUFF = re.compile(r'<stuff\b.*?</stuff>', re.S)


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def write(path, text):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)


def catalogs(folder):
    return [name for name in CATALOGS if os.path.exists(os.path.join(folder, name))]


def download(url, path):
    request = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()
        kind = response.headers.get('Content-Type', '')
    if 'text/html' in kind:
        raise RuntimeError('%s returned an HTML page, not a file' % url)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)
    return len(data)


def exists_on_cdn(url):
    request = urllib.request.Request(url, method='HEAD', headers={'User-Agent': USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.status == 200
    except urllib.error.HTTPError:
        return False


def local_path(folder, url):
    """The release-folder path of a CDN URL (edu/kstars/previews/a.jpg -> folder/previews/a.jpg)."""
    return os.path.join(folder, *url[len(CDN):].split('/'))


def digest(path, algorithm):
    h = hashlib.new(algorithm)
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def cmd_fetch(args):
    os.makedirs(args.folder, exist_ok=True)
    for name in CATALOGS:
        path = os.path.join(args.folder, name)
        if os.path.exists(path) and not args.force:
            sys.exit('%s exists, pass --force to overwrite it' % path)
        print('%s: %d bytes' % (path, download(CDN + name, path)))


def cmd_mirror(args):
    for name in catalogs(args.folder):
        path = os.path.join(args.folder, name)
        text = read(path)

        def rewrite(match):
            opening, tag, url, closing = match.groups()
            if url.startswith(CDN):
                return match.group(0)
            filename = os.path.basename(urllib.parse.urlparse(url).path)
            new_url = CDN + ('previews/' if tag == 'preview' else '') + filename
            target = local_path(args.folder, new_url)
            if os.path.exists(target):
                print('  %s already in the release folder' % target)
            elif args.dry_run:
                print('  would download %s' % url)
            else:
                print('  %s -> %s (%d bytes)' % (url, target, download(url, target)))
            return opening + new_url + closing

        new_text = URL_TAGS.sub(rewrite, text)
        changed = sum(1 for a, b in zip(URL_TAGS.findall(text), URL_TAGS.findall(new_text)) if a[2] != b[2])
        print('%s: %d URLs moved to the KDE CDN' % (name, changed))
        if changed and not args.dry_run:
            write(path, new_text)


def cmd_prepare(args):
    folder = args.folder
    names = catalogs(folder)
    if not names:
        sys.exit('No %s in %s' % (' or '.join(CATALOGS), folder))

    problems, referenced, uploads, unchecked = [], set(), [], []
    for name in names:
        for entry in STUFF.findall(read(os.path.join(folder, name))):
            title = re.search(r'<name>(.*?)</name>', entry, re.S)
            title = title.group(1).strip() if title else '?'
            checksum = re.search(r'<checksum type="md5">\s*([0-9a-f]{32})\s*</checksum>', entry)
            size = re.search(r'<downloadsize1>\s*([\d.]+)\s*</downloadsize1>', entry)
            for _, tag, url, _ in URL_TAGS.findall(entry):
                if not url.startswith(CDN):
                    problems.append('%s, "%s": %s is not on the KDE CDN' % (name, title, url))
                    continue
                path = local_path(folder, url)
                referenced.add(os.path.normpath(path))
                if not os.path.exists(path):
                    if args.offline:
                        unchecked.append(url)
                    elif not exists_on_cdn(url):
                        problems.append('%s, "%s": %s is neither in the release folder nor on the CDN'
                                        % (name, title, url))
                    continue
                uploads.append(path)
                if tag == 'payload' and checksum and digest(path, 'md5') != checksum.group(1):
                    problems.append('%s, "%s": md5 of %s does not match the checksum' % (name, title, path))
                if tag == 'payload' and size:
                    # KNewStuff sizes are in KiB; old entries are rounded, so allow 2%
                    kib = os.path.getsize(path) / 1024.0
                    if abs(kib - float(size.group(1))) > 0.02 * kib:
                        problems.append('%s, "%s": downloadsize1 %s KiB, but %s is %.1f KiB'
                                        % (name, title, size.group(1), path, kib))

    for root, _, files in os.walk(folder):
        for filename in files:
            path = os.path.normpath(os.path.join(root, filename))
            if filename.endswith('.sig') or os.path.relpath(path, folder) in CATALOGS:
                continue
            if path not in referenced:
                problems.append('%s is not referenced by %s' % (path, ' or '.join(names)))

    if unchecked:
        print('Offline: not checked that these are on the CDN:')
        for url in sorted(set(unchecked)):
            print('  ' + url)
    if problems:
        print('Not ready:')
        for problem in problems:
            print('  ' + problem)
        return 1

    # Catalog lists are uploaded only when they differ from the live ones
    for name in names:
        path = os.path.join(folder, name)
        if not args.offline:
            live = os.path.join(folder, '.live-' + name)
            try:
                download(CDN + name, live)
                with open(live, 'rb') as a, open(path, 'rb') as b:
                    unchanged = a.read() == b.read()
            finally:
                if os.path.exists(live):
                    os.remove(live)
            if unchanged:
                print('%s is the same as the live one, not uploading it' % name)
                continue
        uploads.append(path)
    uploads = sorted(set(uploads), key=lambda p: (os.path.basename(p) in CATALOGS, p))

    # Everything goes into one flat incoming folder
    basenames = [os.path.basename(p) for p in uploads]
    clashes = sorted({b for b in basenames if basenames.count(b) > 1})
    if clashes:
        print('Not ready: several files are named %s; upload.kde.org/incoming is flat' % ', '.join(clashes))
        return 1
    if not uploads:
        print('Nothing to publish.')
        return 0

    if not args.no_sign:
        for path in uploads:
            subprocess.run(['gpg2', '--yes', '--armor', '--detach-sign', '-o', path + '.sig', path], check=True)

    files = []
    for path in uploads:
        files.append(path)
        if not args.no_sign:
            files.append(path + '.sig')

    def target(path):
        return '%s/%s' % (CDN_TARGET, os.path.relpath(path, folder).replace(os.sep, '/'))

    lists = [os.path.basename(p) for p in uploads if os.path.basename(p) in CATALOGS]
    description = '\n'.join(
        ['Please publish these KStars KNewStuff data files on the KDE CDN '
         '(https://cdn.kde.org/%s/), replacing the files of the same name.' % CDN_TARGET]
        + (['Please put %s in place last: KStars lists the downloads from it '
            '(https://cdn.kde.org/%s/providers.xml), so the other files must be there first.'
            % (' and '.join(lists), CDN_TARGET)] if lists else [])
        + ['', 'Targets:', '', '```']
        + ['%s -> %s' % (os.path.basename(p), target(p)) for p in files]
        + ['```', '', 'SHA-256:', '', '```']
        + ['%s  %s' % (digest(p, 'sha256'), os.path.basename(p)) for p in files]
        + ['```', '', 'SHA-1:', '', '```']
        + ['%s  %s' % (digest(p, 'sha1'), os.path.basename(p)) for p in files]
        + ['```'])
    title = 'Publish KStars KNewStuff data (%d files)' % len(uploads)
    ticket = TICKET_FORM + '?' + urllib.parse.urlencode({'title': title, 'description': description})

    print('#' * 80)
    print('KStars KNewStuff data ready!')
    print()
    print('# Publish')
    print()
    print('Read readme: ftp://upload.kde.org/README')
    print('Upload: %s/' % UPLOAD.rsplit('/', 1)[0])
    for path in files:
        print('    kdecp %s %s' % (path, UPLOAD))
    print()
    print('File a sysadmin ticket: https://go.kde.org/u/systickets')
    print()
    print('# Ticket Template Link')
    print(ticket)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)

    command = commands.add_parser('fetch', help='download the live main.xml and dso.xml')
    command.add_argument('--force', action='store_true', help='overwrite existing files')
    command.add_argument('folder', help='release folder')
    command.set_defaults(run=cmd_fetch)

    command = commands.add_parser('mirror', help='move payloads and previews hosted elsewhere to the CDN')
    command.add_argument('--dry-run', action='store_true')
    command.add_argument('folder', help='release folder')
    command.set_defaults(run=cmd_mirror)

    command = commands.add_parser('prepare', help='check, sign and print the upload commands and ticket')
    command.add_argument('--no-sign', action='store_true', help='do not sign the files (for a trial run)')
    command.add_argument('--offline', action='store_true', help='do not check that the other files are on the CDN')
    command.add_argument('folder', help='release folder')
    command.set_defaults(run=cmd_prepare)

    args = parser.parse_args()
    return args.run(args) or 0


if __name__ == '__main__':
    sys.exit(main())
