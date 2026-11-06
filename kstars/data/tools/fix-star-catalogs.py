#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Repair the KStars star catalogs (bug 309164 and related data errors).

KStars treats every catalog position as J2000.0 and applies proper motion from
J2000.0 (StarObject::getIndexCoords). The commands below fix the data that breaks
that assumption. They change nothing else, and running them again is a no-op.

  stars-dat   Fix stars.dat, the ASCII master of the bright stars. Its 2012 conversion
              from J1991.25 to J2000.0 (commit cf2f4e250a) wrote Dec seconds of 59.05"
              and more as 00.0" of the next minute (errors up to 1"), and some RA
              seconds as 60.00. Those fields are recomputed from the J1991.25 file.

  epoch       Move the records of namedstars.dat / unnamedstars.dat that still hold
              the J1991.25 Hipparcos position to J2000.0 (bug 309164). A record is
              taken to be at J1991.25 when it matches a row of the pre-2012 stars.dat
              better than that row moved to J2000.0, and is moved +8.75 years along
              its own proper motion. The correction runs along the star's own track,
              which the generator already used to copy fast stars into every trixel
              they cross, so the trixel index and the record order (which
              starnames.dat relies on) stay valid. Shifts below 0.3" are left alone:
              the reference cannot tell the two epochs apart at that level.

  xhip        Fix records against the Extended Hipparcos Compilation (XHIP, Anderson &
              Francis 2012, Hipparcos-2 astrometry), as shipped with Skychart in its
              cat/xhip folder. Two kinds of records are fixed, both outside stars.dat:
              - records at the epoch of their Tycho-1 observation (1989-1993): the
                proper motion matches an XHIP star and the offset from its J2000.0
                position runs along that motion. They get the XHIP J2000.0 position.
              - records without proper motion at the J1991.25 position of an XHIP
                star with a measurable proper motion (Hipparcos entries that lost
                their motion). They get the XHIP proper motion and J2000.0 position.
              Close binaries are safe: a record is changed only when it describes
              the same XHIP entry; components resolved by Hipparcos are separate
              XHIP entries with their own positions.

  dedupe      Remove stars listed twice: the same star from both Hipparcos and Tycho
              (same position, proper motion and magnitude), and "ghosts", copies of a
              moving star without proper motion left at its J1991.25 position (for
              instance Barnard's Star in the Tycho-2 deepstars.dat). Of the copies of
              a star, the one with a proper motion is kept, then the one in the
              brightest catalog (files are given brightest first), then the one with
              a real spectral type, a parallax and an HD number. Files passed with
              --fixed are compared with but never modified: namedstars.dat must be,
              since starnames.dat follows its record order. Only whole stars are
              removed (all their trixel copies), the per-trixel magnitude order is
              kept and the trixel index is rebuilt.

Usage (the order matters: dedupe removes the ghosts that have a moving twin before xhip
restores the proper motion of the others, and runs again for the records xhip moved onto
their Tycho copies):
    git show cf2f4e250a^:kstars/data/stars.dat > /tmp/stars-J1991.25.dat
    XHIP=/path/to/skychart/cat/xhip
    cd kstars/data
    python3 tools/fix-star-catalogs.py stars-dat --reference /tmp/stars-J1991.25.dat stars.dat
    python3 tools/fix-star-catalogs.py epoch --reference /tmp/stars-J1991.25.dat \\
        namedstars.dat unnamedstars.dat
    python3 tools/fix-star-catalogs.py dedupe --fixed namedstars.dat unnamedstars.dat
    python3 tools/fix-star-catalogs.py xhip --xhip $XHIP namedstars.dat unnamedstars.dat
    python3 tools/fix-star-catalogs.py dedupe --fixed namedstars.dat unnamedstars.dat
    # The downloadable Tycho-2 catalog (KNewStuff), checked against the shipped ones:
    DEEP=deepstars.dat
    python3 tools/fix-star-catalogs.py dedupe --fixed namedstars.dat --fixed unnamedstars.dat $DEEP
    python3 tools/fix-star-catalogs.py xhip --xhip $XHIP $DEEP
    python3 tools/fix-star-catalogs.py dedupe --fixed namedstars.dat --fixed unnamedstars.dat $DEEP

Every command accepts --dry-run.
"""

import argparse
import math
import os
import struct
import sys
from collections import defaultdict

EPOCH_SHIFT_YEARS = 2000.0 - 1991.25
MATCH_TOLERANCE = 0.3  # arcseconds
RECORD_SIZE = 32
RECORD_FORMAT = '<iiiiiihh2sbb'  # StarData: RA, Dec, dRA, dDec, parallax, HD, mag, B-V, spectral type, flags, unused
FLAG_MULTIPLE = 0x02
ARCSEC = math.pi / (180.0 * 3600.0)
CELLS_RA = 360 * 60


# ---------------------------------------------------------------------------------------------
# Geometry


def separation(ra1, dec1, ra2, dec2):
    """Angular separation in arcseconds (haversine). RA in hours, Dec in degrees."""
    a1, d1, a2, d2 = math.radians(ra1 * 15), math.radians(dec1), math.radians(ra2 * 15), math.radians(dec2)
    h = math.sin((d2 - d1) / 2) ** 2 + math.cos(d1) * math.cos(d2) * math.sin((a2 - a1) / 2) ** 2
    return 2 * math.asin(min(1.0, math.sqrt(h))) / ARCSEC


def propagate(ra, dec, pm_ra, pm_dec, years):
    """Same model as StarObject::getIndexCoords(). pm in mas/yr, pm_ra includes cos(dec)."""
    scale = years / 1000.0 * ARCSEC
    sin_dec, cos_dec = math.sin(math.radians(dec)), math.cos(math.radians(dec))
    sin_ra, cos_ra = math.sin(math.radians(ra * 15)), math.cos(math.radians(ra * 15))
    net_ra, net_dec = pm_ra * scale, pm_dec * scale
    x = cos_dec * cos_ra - net_ra * sin_ra - net_dec * sin_dec * cos_ra
    y = cos_dec * sin_ra + net_ra * cos_ra - net_dec * sin_dec * sin_ra
    z = sin_dec + net_dec * cos_dec
    new_ra = math.degrees(math.atan2(y, x)) % 360.0
    new_dec = math.degrees(math.atan2(z, math.hypot(x, y)))
    return new_ra / 15.0, new_dec


class Grid:
    """Spatial lookup of (ra, dec, item) in 1 arcminute cells."""

    def __init__(self):
        self.cells = defaultdict(list)

    @staticmethod
    def _cell(ra, dec):
        return int(ra * 15 * 60) % CELLS_RA, int((dec + 90) * 60)

    def add(self, ra, dec, item):
        self.cells[self._cell(ra, dec)].append((ra, dec, item))

    def near(self, ra, dec, radius):
        """Yield (separation, item) for the items within radius (arcseconds, at most 60)."""
        cx, cy = self._cell(ra, dec)
        span = min(int(1.0 / max(math.cos(math.radians(dec)), 1e-3)) + 1, CELLS_RA // 2)
        for dx in range(-span, span + 1):
            for dy in (-1, 0, 1):
                for item_ra, item_dec, item in self.cells.get(((cx + dx) % CELLS_RA, cy + dy), ()):
                    s = separation(ra, dec, item_ra, item_dec)
                    if s <= radius:
                        yield s, item

    def nearest(self, ra, dec, radius=60.0):
        return min((s for s, _ in self.near(ra, dec, radius)), default=float('inf'))


# ---------------------------------------------------------------------------------------------
# stars.dat


def parse_position(line):
    """RA (hours), Dec (degrees) of a stars.dat row."""
    ra = int(line[0:2]) + int(line[2:4]) / 60.0 + float(line[4:9]) / 3600.0
    sign = -1.0 if line[10] == '-' else 1.0
    dec = sign * (int(line[11:13]) + int(line[13:15]) / 60.0 + float(line[15:19]) / 3600.0)
    return ra, dec


def parse_proper_motion(line):
    return float(line[20:29]), float(line[29:38])


def format_ra(ra):
    centiseconds = int(round(ra * 360000)) % (24 * 360000)
    h, rest = divmod(centiseconds, 360000)
    m, rest = divmod(rest, 6000)
    return '%02d%02d%05.2f' % (h, m, rest / 100.0)


def format_dec(dec):
    deciseconds = int(round(abs(dec) * 36000))
    d, rest = divmod(deciseconds, 36000)
    m, rest = divmod(rest, 600)
    return '%s%02d%02d%04.1f' % ('-' if dec < 0 else '+', d, m, rest / 10.0)


def data_rows(path):
    """(line index, line) of the data rows of a stars.dat file."""
    with open(path, encoding='latin-1', newline='') as f:
        lines = f.read().split('\n')
    return lines, [(i, l) for i, l in enumerate(lines) if l.strip() and not l.startswith('#')]


def reference_positions(path):
    _, rows = data_rows(path)
    return [parse_position(l) for _, l in rows]


def fix_stars_dat(args):
    _, reference = data_rows(args.reference)
    lines, rows = data_rows(args.file)
    if len(reference) != len(rows):
        sys.exit('%s has %d rows, the reference %d: they must correspond row by row'
                 % (args.file, len(rows), len(reference)))

    fixed_ra = fixed_dec = mismatched = 0
    for (index, line), (_, ref) in zip(rows, reference):
        if parse_proper_motion(line) != parse_proper_motion(ref):
            mismatched += 1
            continue
        ra, dec = propagate(*parse_position(ref), *parse_proper_motion(ref), EPOCH_SHIFT_YEARS)
        new_ra, new_dec = line[0:9], line[10:19]
        # Only the fields damaged by the 2012 conversion are rewritten
        if float(line[4:9]) >= 60.0 and format_ra(ra) != new_ra:
            new_ra = format_ra(ra)
            fixed_ra += 1
        if line[15:19] == '00.0' and format_dec(dec) != new_dec:
            new_dec = format_dec(dec)
            fixed_dec += 1
        lines[index] = new_ra + line[9] + new_dec + line[19:]

    print('%s: %d rows, %d RA and %d Dec fields fixed, %d rows skipped (proper motion differs from the reference)'
          % (args.file, len(rows), fixed_ra, fixed_dec, mismatched))
    if not args.dry_run and (fixed_ra or fixed_dec):
        with open(args.file, 'w', encoding='latin-1', newline='') as f:
            f.write('\n'.join(lines))


# ---------------------------------------------------------------------------------------------
# XHIP, in the Skychart (Cartes du Ciel) CDCSTAR1 format: star.hdr describes the fields of the
# fixed-size records of star01.dat ... starNN.dat (see skychart/library/catalog/gcatunit.pas)


def read_xhip(folder):
    """List of XHIP stars: dict(ra, dec at J1991.25, pm_ra, pm_dec in mas/yr, ra2000, dec2000, mag, hip)."""
    with open(os.path.join(folder, 'star.hdr'), 'rb') as f:
        header = f.read()
    if header[4:12] != b'CDCSTAR1':
        raise ValueError('%s is not a Skychart CDCSTAR1 catalog' % folder)
    # TFileHeader: hdrl, version[8], ShortName[4], LongName[50], reclen, FileNum, Equinox, Epoch,
    # MagMax, Size, Units, ObjType, LogSize, UsePrefix, IxKeylen, AltName[10], AltPrefix[10], IdFormat,
    # HighPrecPM, IdPrefix, StarType, Spare1[6], fpos[40], Spare2[15], flen[40], Spare3[15], flabel[35][11]
    reclen, = struct.unpack_from('<i', header, 66)
    epoch, = struct.unpack_from('<d', header, 82)
    offset = struct.calcsize('<i8s4s50siiddd4iBB10B10i4i6i')
    fpos = struct.unpack_from('<40i', header, offset)
    flen = struct.unpack_from('<40i', header, offset + 55 * 4)
    labels_offset = offset + 2 * 55 * 4
    labels = []
    for i in range(35):
        raw = header[labels_offset + 11 * i:labels_offset + 11 * (i + 1)]
        labels.append(raw.decode('latin-1').strip(' \0'))
    if epoch != 1991.25 or labels[:2] != ['RA', 'DEC']:
        raise ValueError('%s: unexpected XHIP layout (epoch %s, fields %s)' % (folder, epoch, labels[:2]))

    def field(record, label, fmt):
        i = labels.index(label)
        start = fpos[i] - 1
        if fmt == 's':
            return record[start:start + flen[i]].decode('latin-1').strip()
        return struct.unpack_from('<' + fmt, record, start)[0]

    stars = []
    for name in sorted(n for n in os.listdir(folder) if n.startswith('star') and n.endswith('.dat')):
        with open(os.path.join(folder, name), 'rb') as f:
            data = f.read()
        for start in range(0, len(data) - reclen + 1, reclen):
            record = data[start:start + reclen]
            ra = field(record, 'RA', 'I') / 3600000.0 / 15.0
            dec = field(record, 'DEC', 'I') / 3600000.0 - 90.0
            # Proper motions in arcsec/yr, the one in RA including cos(dec), like the KStars catalogs
            pm_ra, pm_dec = field(record, 'pmRA', 'f') * 1000.0, field(record, 'pmDE', 'f') * 1000.0
            ra2000, dec2000 = propagate(ra, dec, pm_ra, pm_dec, EPOCH_SHIFT_YEARS)
            stars.append(dict(ra=ra, dec=dec, pm_ra=pm_ra, pm_dec=pm_dec, ra2000=ra2000, dec2000=dec2000,
                              mag=field(record, 'mV', 'h') / 1000.0, hip=field(record, 'HIP', 's')))
    return stars


# ---------------------------------------------------------------------------------------------
# Binary star data files


class Catalog:
    """A KStars star data file made of 32-byte StarData records under a trixel index."""

    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            self.data = bytearray(f.read())
        if struct.unpack_from('<H', self.data, 124)[0] != 0x4B53:
            raise ValueError('%s: only little-endian KStars star data files are supported' % path)
        nfields = struct.unpack_from('<H', self.data, 127)[0]
        record_size = sum(struct.unpack_from('<10sbBi', self.data, 129 + 16 * i)[1] for i in range(nfields))
        if record_size != RECORD_SIZE:
            raise ValueError('%s: expected %d-byte StarData records, found %d' % (path, RECORD_SIZE, record_size))
        self.index_offset = 129 + 16 * nfields
        ntrixels = struct.unpack_from('<I', self.data, self.index_offset)[0]
        self.index = [struct.unpack_from('<III', self.data, self.index_offset + 4 + 12 * i) for i in range(ntrixels)]
        self.data_offset = self.index_offset + 4 + 12 * ntrixels
        expected = min(start for _, start, _ in self.index)
        for trixel, (tid, start, count) in enumerate(self.index):
            if tid != trixel or start != expected:
                raise ValueError('%s: trixel %d is not stored in index order' % (path, trixel))
            expected += RECORD_SIZE * count
        if expected != len(self.data):
            raise ValueError('%s: index does not cover the whole file' % path)

    def offsets(self):
        """(trixel, file offset) of every record."""
        for trixel, (_, start, count) in enumerate(self.index):
            for k in range(count):
                yield trixel, start + RECORD_SIZE * k

    def record(self, offset):
        return struct.unpack_from(RECORD_FORMAT, self.data, offset)

    def write(self, removed_offsets=frozenset()):
        """Write the file, leaving out the given records and rebuilding the trixel index."""
        out = bytearray(self.data[:self.data_offset])
        # Bytes between the index table and the first record (faint magnitude, HTM level, ...)
        out += self.data[self.data_offset:self.index[0][1]]
        index = []
        for trixel, (_, start, count) in enumerate(self.index):
            first, kept = len(out), 0
            for k in range(count):
                offset = start + RECORD_SIZE * k
                if offset not in removed_offsets:
                    out += self.data[offset:offset + RECORD_SIZE]
                    kept += 1
            index.append((trixel, first, kept))
        for trixel, entry in enumerate(index):
            struct.pack_into('<III', out, self.index_offset + 4 + 12 * trixel, *entry)
        with open(self.path, 'wb') as f:
            f.write(out)


def fix_epoch(args):
    grid = Grid()
    for ra, dec in reference_positions(args.reference):
        grid.add(ra, dec, None)

    for path in args.files:
        catalog = Catalog(path)
        patched, shifts = 0, {}
        for _, offset in catalog.offsets():
            raw_ra, raw_dec, raw_pm_ra, raw_pm_dec = struct.unpack_from('<iiii', catalog.data, offset)
            ra, dec = raw_ra / 1e6, raw_dec / 1e5
            pm_ra, pm_dec = raw_pm_ra / 10.0, raw_pm_dec / 10.0
            shift = math.hypot(pm_ra, pm_dec) * EPOCH_SHIFT_YEARS / 1000.0
            if shift < MATCH_TOLERANCE:
                continue

            # Hypothesis A: the record is at J1991.25, so it sits on a reference row.
            # Hypothesis B: the record is at J2000.0, so moving it back lands on a reference row.
            # When B matches too, require A to win clearly, so that a second run never moves a
            # record again. B landing near another star (a common proper motion companion) is no match.
            sep_epoch_1991 = grid.nearest(ra, dec)
            if sep_epoch_1991 > MATCH_TOLERANCE:
                continue
            sep_epoch_2000 = grid.nearest(*propagate(ra, dec, pm_ra, pm_dec, -EPOCH_SHIFT_YEARS))
            if sep_epoch_2000 <= MATCH_TOLERANCE and sep_epoch_2000 - sep_epoch_1991 < shift / 2:
                continue

            new_ra, new_dec = propagate(ra, dec, pm_ra, pm_dec, EPOCH_SHIFT_YEARS)
            new_raw = int(round(new_ra * 1e6)), int(round(new_dec * 1e5))
            if new_raw == (raw_ra, raw_dec):
                continue
            struct.pack_into('<ii', catalog.data, offset, *new_raw)
            patched += 1
            shifts[(raw_ra, raw_dec)] = separation(ra, dec, new_ra, new_dec)

        values = sorted(shifts.values())
        print('%s: %d records, %d moved to J2000.0 (%d stars); shift max %.1f", >1": %d, >10": %d'
              % (path, sum(c for _, _, c in catalog.index), patched, len(values), values[-1] if values else 0.0,
                 sum(s > 1 for s in values), sum(s > 10 for s in values)))
        if not args.dry_run and patched:
            catalog.write()


def fix_from_xhip(args):
    xhip = read_xhip(args.xhip)
    at_2000, at_1991 = Grid(), Grid()
    for x in xhip:
        at_2000.add(x['ra2000'], x['dec2000'], x)
        at_1991.add(x['ra'], x['dec'], x)

    for path in args.files:
        catalog = Catalog(path)
        fixed = {}  # distinct record -> (new raw RA, Dec, dRA, dDec, kind, shift, HIP)
        for _, offset in catalog.offsets():
            raw = catalog.record(offset)
            if raw in fixed:
                continue
            ra, dec = raw[0] / 1e6, raw[1] / 1e5
            pm_ra, pm_dec, mag = raw[2] / 10.0, raw[3] / 10.0, raw[6] / 100.0
            pm = math.hypot(pm_ra, pm_dec)
            if pm == 0:
                # A Hipparcos entry that lost its proper motion: on the J1991.25 position of a moving star
                match = min(((s, x) for s, x in at_1991.near(ra, dec, MATCH_TOLERANCE)
                             if abs(x['mag'] - mag) <= 0.75), default=None, key=lambda t: t[0])
                if match is None:
                    continue
                x = match[1]
                if math.hypot(x['pm_ra'], x['pm_dec']) * EPOCH_SHIFT_YEARS / 1000.0 < MATCH_TOLERANCE:
                    continue
                fixed[raw] = (x['ra2000'], x['dec2000'], x['pm_ra'], x['pm_dec'], 'lost proper motion',
                              separation(ra, dec, x['ra2000'], x['dec2000']), x['hip'])
                continue

            # A Tycho-1 epoch position: same motion as an XHIP star, offset along it by 7 to 11 years
            best = None
            for s, x in at_2000.near(ra, dec, 15.0):
                if abs(x['mag'] - mag) > 0.6 or math.hypot(x['pm_ra'], x['pm_dec']) == 0:
                    continue
                if math.hypot(x['pm_ra'] - pm_ra, x['pm_dec'] - pm_dec) > max(10.0, 0.1 * pm):
                    continue
                if best is None or s < best[0]:
                    best = (s, x)
            if best is None or best[0] <= MATCH_TOLERANCE:
                continue
            s, x = best
            east = (ra - x['ra2000']) * 15 * 3600 * math.cos(math.radians(dec))
            north = (dec - x['dec2000']) * 3600
            years = -(east * x['pm_ra'] + north * x['pm_dec']) / (x['pm_ra'] ** 2 + x['pm_dec'] ** 2) * 1000.0
            across = math.hypot(east + years * x['pm_ra'] / 1000.0, north + years * x['pm_dec'] / 1000.0)
            if 7.0 <= years <= 11.0 and across <= MATCH_TOLERANCE:
                fixed[raw] = (x['ra2000'], x['dec2000'], pm_ra, pm_dec, 'epoch %.2f' % (2000.0 - years), s, x['hip'])

        records = 0
        for _, offset in catalog.offsets():
            raw = catalog.record(offset)
            if raw in fixed:
                new_ra, new_dec, new_pm_ra, new_pm_dec = fixed[raw][:4]
                struct.pack_into('<iiii', catalog.data, offset, int(round(new_ra * 1e6)), int(round(new_dec * 1e5)),
                                 int(round(new_pm_ra * 10)), int(round(new_pm_dec * 10)))
                records += 1
        print('%s: %d stars (%d records) fixed from XHIP' % (path, len(fixed), records))
        for raw, (_, _, _, _, kind, shift, hip) in sorted(fixed.items(), key=lambda t: -t[1][5]):
            print('    HIP %-6s HD %-6d mag %5.2f  %-18s moved %.1f"' % (hip, raw[5], raw[6] / 100.0, kind, shift))
        if not args.dry_run and fixed:
            catalog.write()


class Star:
    """One star of a catalog: a distinct record, possibly copied into several trixels."""

    def __init__(self, file_rank, raw):
        self.file_rank = file_rank
        self.raw = raw
        ra, dec, pm_ra, pm_dec, parallax, hd, mag, _, spectral, flags, _ = raw
        self.ra, self.dec = ra / 1e6, dec / 1e5
        self.pm_ra, self.pm_dec = pm_ra / 10.0, pm_dec / 10.0
        self.pm = math.hypot(self.pm_ra, self.pm_dec)
        self.mag = mag / 100.0
        self.multiple = bool(flags & FLAG_MULTIPLE)
        self.offsets = []
        # Records converted from Tycho have a spectral class guessed from B-V ("G?") and no parallax
        self.score = (spectral[1:2] != b'?') + (parallax != 0) + (hd != 0)

    def keep_rank(self):
        """Lower is better: a proper motion (never keep a ghost), earlier file, more information, brighter."""
        return self.pm == 0, self.file_rank, -self.score, self.mag


def same_star(a, b, sep):
    """Two records of the same star: same position, proper motion and magnitude.

    Records with bit-identical astrometry (position, proper motion, parallax) are not taken as
    duplicates: two catalogs never agree to the last milliarcsecond, so such records come from one
    astrometric solution given to both components of a close double, which differ in magnitude."""
    if a.raw[:5] == b.raw[:5]:
        return False
    if sep > (1.0 if not (a.multiple or b.multiple) else 0.5):
        return False
    if math.hypot(a.pm_ra - b.pm_ra, a.pm_dec - b.pm_dec) > max(10.0, 0.1 * max(a.pm, b.pm)):
        return False
    return abs(a.mag - b.mag) <= 0.5


def ghost_of(ghost, star, sep_at_1991):
    """A copy of a moving star without proper motion, left at its J1991.25 position.

    In deepstars.dat these ghosts lie within 0.25" of that position; beyond it the zero proper
    motion records near moving stars are other stars (mostly double star components). The
    magnitude tolerance is wider than for duplicates, as faint Tycho photometry is noisy."""
    return (ghost.pm == 0 and star.pm * EPOCH_SHIFT_YEARS / 1000.0 >= MATCH_TOLERANCE
            and sep_at_1991 <= MATCH_TOLERANCE and abs(ghost.mag - star.mag) <= 0.75)


def fix_duplicates(args):
    paths = args.fixed + args.files
    for path in args.files:
        if os.path.basename(path) == 'namedstars.dat':
            sys.exit('%s: starnames.dat follows its record order, pass it with --fixed' % path)
    catalogs = [Catalog(path) for path in paths]
    fixed = set(range(len(args.fixed)))
    stars, grid = [], Grid()
    for rank, catalog in enumerate(catalogs):
        by_raw = {}
        for _, offset in catalog.offsets():
            raw = catalog.record(offset)
            star = by_raw.get(raw)
            if star is None:
                star = by_raw[raw] = Star(rank, raw)
                stars.append(star)
                grid.add(star.ra, star.dec, star)
            star.offsets.append(offset)

    # Group the copies of each star (union-find), then keep the best record of every group
    parent = {id(s): s for s in stars}

    def root(s):
        while parent[id(s)] is not s:
            parent[id(s)] = parent[id(parent[id(s)])]
            s = parent[id(s)]
        return s

    duplicates = ghosts = 0
    for star in stars:
        for sep, other in grid.near(star.ra, star.dec, 1.0):
            if other is not star and id(other) > id(star) and same_star(star, other, sep):
                parent[id(root(other))] = root(star)
                duplicates += 1
        if star.pm * EPOCH_SHIFT_YEARS / 1000.0 >= MATCH_TOLERANCE:
            back = propagate(star.ra, star.dec, star.pm_ra, star.pm_dec, -EPOCH_SHIFT_YEARS)
            for sep, other in grid.near(*back, MATCH_TOLERANCE):
                if other is not star and ghost_of(other, star, sep):
                    parent[id(root(other))] = root(star)
                    ghosts += 1

    groups = defaultdict(list)
    for star in stars:
        groups[id(root(star))].append(star)

    removed = [set() for _ in catalogs]
    patched_hd = [0] * len(catalogs)
    for members in groups.values():
        if len(members) == 1:
            continue
        # Records of the --fixed files are never removed
        members.sort(key=Star.keep_rank)
        keep = members[0]
        dropped = [s for s in members[1:] if s.file_rank not in fixed]
        for star in dropped:
            removed[star.file_rank].update(star.offsets)
        # Keep the HD number, which the Find dialog uses, when only a removed copy had it
        hd = next((s.raw[5] for s in dropped if s.raw[5]), 0)
        if keep.file_rank not in fixed and keep.raw[5] == 0 and hd:
            for offset in keep.offsets:
                struct.pack_into('<i', catalogs[keep.file_rank].data, offset + 20, hd)
            patched_hd[keep.file_rank] += 1

    print('%d pairs of the same star, %d ghosts' % (duplicates, ghosts))
    for rank, catalog in enumerate(catalogs):
        if rank in fixed:
            continue
        stars_removed = sum(1 for s in stars if s.file_rank == rank and s.offsets[0] in removed[rank])
        print('%s: %d records, %d removed (%d stars), %d HD numbers carried over'
              % (catalog.path, sum(c for _, _, c in catalog.index), len(removed[rank]), stars_removed,
                 patched_hd[rank]))
        if not args.dry_run and (removed[rank] or patched_hd[rank]):
            catalog.write(removed[rank])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)

    command = commands.add_parser('stars-dat', help='fix the rounding errors of stars.dat')
    command.add_argument('--reference', required=True, help='pre-2012 stars.dat (all rows at J1991.25)')
    command.add_argument('--dry-run', action='store_true')
    command.add_argument('file', help='stars.dat to fix in place')
    command.set_defaults(run=fix_stars_dat)

    command = commands.add_parser('epoch', help='move J1991.25 records to J2000.0')
    command.add_argument('--reference', required=True, help='pre-2012 stars.dat (all rows at J1991.25)')
    command.add_argument('--dry-run', action='store_true')
    command.add_argument('files', nargs='+', help='namedstars.dat / unnamedstars.dat to fix in place')
    command.set_defaults(run=fix_epoch)

    command = commands.add_parser('xhip', help='fix stale epochs and lost proper motions from XHIP')
    command.add_argument('--xhip', required=True, help='Skychart cat/xhip folder')
    command.add_argument('--dry-run', action='store_true')
    command.add_argument('files', nargs='+', help='star data files to fix in place')
    command.set_defaults(run=fix_from_xhip)

    command = commands.add_parser('dedupe', help='remove stars listed twice')
    command.add_argument('--dry-run', action='store_true')
    command.add_argument('--fixed', action='append', default=[], metavar='FILE',
                         help='star data file to compare with but never modify (namedstars.dat); may be repeated')
    command.add_argument('files', nargs='+', help='star data files to fix in place, brightest first')
    command.set_defaults(run=fix_duplicates)

    args = parser.parse_args()
    args.run(args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
