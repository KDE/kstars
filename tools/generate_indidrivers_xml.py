#!/usr/bin/env python3
"""
Regenerate kstars/data/indidrivers.xml from the locally installed INDI driver
XML files (mirrors the file selection logic in DriverManager::readXMLDrivers()).

Unlike a plain `cat` of every driver XML file, this script:

  * Pretty-prints every block with a consistent 4-space indent, regardless of
    how the source file was formatted.
  * Never silently drops a driver. Entries that already exist in the current
    indidrivers.xml but are not found on the local machine (e.g. arm64-only
    drivers such as libcamera, or externally maintained drivers such as
    Cam90 that aren't installed here) are kept as-is, appended at the end.
    Pass --no-preserve-missing to disable this and do a hard refresh instead.

Usage:
    tools/generate_indidrivers_xml.py [--indi-dir DIR] [--output FILE]
                                       [--no-preserve-missing] [--dry-run]
"""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

XML_DECLARATION = '<?xml version="1.0" encoding="UTF-8"?>\n'
INDENT = "    "

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "kstars" / "data" / "indidrivers.xml"
DEFAULT_INDI_DIRS = ["/usr/share/indi", "/usr/local/share/indi"]


def find_indi_dir(explicit):
    if explicit:
        p = Path(explicit)
        if not (p / "drivers.xml").is_file():
            sys.exit(f"error: {p} does not contain a drivers.xml")
        return p
    for candidate in DEFAULT_INDI_DIRS:
        p = Path(candidate)
        if (p / "drivers.xml").is_file():
            return p
    sys.exit("error: could not find an INDI drivers directory; pass --indi-dir")


def device_key(group, device_el):
    driver_el = device_el.find("driver")
    driver_name = driver_el.text.strip() if driver_el is not None and driver_el.text else ""
    return (group, device_el.get("label", ""), driver_name)


def load_devgroups(xml_path):
    """Parse a <driversList> XML file and return its <devGroup> children."""
    root = ET.parse(xml_path).getroot()
    if root.tag != "driversList":
        raise ValueError(f"unexpected root <{root.tag}>, expected <driversList>")
    return list(root.findall("devGroup"))


def collect_local_driver_files(indi_dir):
    """Same file selection as DriverManager::readXMLDrivers(): drivers.xml plus
    every indi_*.xml file, skipping the *_sk.xml skeleton files."""
    primary = indi_dir / "drivers.xml"
    others = sorted(
        p for p in indi_dir.glob("indi_*.xml") if not p.name.endswith("_sk.xml")
    )
    return [primary] + others


def load_existing_devices(existing_path):
    """Return {key: (group, device_element)} for every device currently in
    indidrivers.xml, in encounter order. The file is a concatenation of
    multiple <driversList> roots (that's the format DriverManager streams
    in), so it isn't well-formed standalone XML and needs wrapping first."""
    if not existing_path.is_file():
        return {}
    raw = existing_path.read_text(encoding="utf-8")
    raw = re.sub(r"<\?xml[^>]*\?>", "", raw)
    wrapped = ET.fromstring(f"<root>{raw}</root>")
    devices = {}
    for drivers_list in wrapped.findall("driversList"):
        for devgroup in drivers_list.findall("devGroup"):
            group = devgroup.get("group")
            for device in devgroup.findall("device"):
                devices.setdefault(device_key(group, device), (group, device))
    return devices


def build_blocks(local_files, existing_devices, preserve_missing):
    """Returns (blocks, num_local_devices, num_preserved) where blocks is a
    list of lists of <devGroup> elements, one list per output <driversList>."""
    blocks = []
    found = set()
    for path in local_files:
        if not path.is_file():
            continue
        try:
            devgroups = load_devgroups(path)
        except (ET.ParseError, ValueError) as exc:
            print(f"warning: skipping {path}: {exc}", file=sys.stderr)
            continue
        for dg in devgroups:
            group = dg.get("group")
            for device in dg.findall("device"):
                found.add(device_key(group, device))
        blocks.append(devgroups)

    preserved = 0
    if preserve_missing:
        leftover_by_group = {}
        for key, (group, device) in existing_devices.items():
            if key in found:
                continue
            leftover_by_group.setdefault(group, []).append(device)
            preserved += 1
        if leftover_by_group:
            leftover_devgroups = []
            for group in sorted(leftover_by_group):
                dg = ET.Element("devGroup", {"group": group})
                for device in leftover_by_group[group]:
                    dg.append(device)
                leftover_devgroups.append(dg)
            blocks.append(leftover_devgroups)

    return blocks, len(found), preserved


def serialize(blocks):
    parts = []
    for devgroups in blocks:
        root = ET.Element("driversList")
        root.extend(devgroups)
        ET.indent(root, space=INDENT)
        parts.append(ET.tostring(root, encoding="unicode"))
    return XML_DECLARATION + "\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--indi-dir", help="Directory containing the local INDI driver XML files "
                                            f"(default: first of {DEFAULT_INDI_DIRS} that exists)")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Output path (default: %(default)s)")
    parser.add_argument("--no-preserve-missing", action="store_true",
                         help="Do not keep drivers that are in the current output file but not "
                              "found locally (hard refresh instead of a safe merge)")
    parser.add_argument("--dry-run", action="store_true", help="Print a summary without writing the file")
    args = parser.parse_args()

    indi_dir = find_indi_dir(args.indi_dir)
    output_path = Path(args.output)

    local_files = collect_local_driver_files(indi_dir)
    existing_devices = load_existing_devices(output_path)
    blocks, num_local, num_preserved = build_blocks(
        local_files, existing_devices, preserve_missing=not args.no_preserve_missing
    )

    print(f"INDI driver dir : {indi_dir}")
    print(f"Source files    : {len(local_files)}")
    print(f"Local devices   : {num_local}")
    print(f"Preserved       : {num_preserved} (in current file, not found locally)")
    print(f"Total devices   : {num_local + num_preserved}")

    if args.dry_run:
        print("(dry run, nothing written)")
        return

    output_path.write_text(serialize(blocks), encoding="utf-8")
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
