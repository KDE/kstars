#!/usr/bin/env python3
"""
KStars Release Blog Post Generator
====================================
Generates a conversational blog post draft from the KStars git history
between two release branches (or date range).

Usage examples:
  # Between two stable branches — default LLM (Claude sonnet), Markdown output
  python generate_blog_post.py --from-ref stable-3.8.2 --to-ref stable-3.8.3 --version 3.8.3

  # From last stable to current master (upcoming release preview)
  python generate_blog_post.py --from-ref stable-3.8.2 --to-ref master --version 3.8.3

  # Between two dates instead of refs
  python generate_blog_post.py --from-date 2026-04-01 --to-date 2026-05-31 --version 3.8.3

  # List available stable branches
  python generate_blog_post.py --version 3.8.3 --list-branches

  # LLM backend selection
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --llm claude
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --llm claude --model claude-opus-4-5
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --llm ollama --ollama-model llama3.2
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --llm none

  # Output format: HTML (ready to paste into Blogger) or Markdown (default)
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --format html
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 --format html --output blog-3.8.3.html

  # Custom instructions for the LLM (inline string or path to a .txt file)
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 \
      --instructions "Focus more on the Alignment improvements."
  python generate_blog_post.py --from-ref stable-3.8.2 --version 3.8.3 \
      --instructions ~/notes-3.8.3.txt

  # All options combined
  python generate_blog_post.py \
      --from-ref stable-3.8.2 --to-ref stable-3.8.3 --version 3.8.3 \
      --llm claude --model claude-sonnet-4-5 \
      --format html \
      --instructions "Keep intro under 3 sentences. Highlight Christian Kemper's Mount Modeler work." \
      --output blog-3.8.3.html

Dependencies:
  - anthropic  (for --llm claude):  pip install anthropic
  - ollama     (for --llm ollama):  pip install ollama
"""

import argparse
import subprocess
import sys
import os
import re
from datetime import datetime
from collections import defaultdict

# ---------------------------------------------------------------------------
# Noise patterns — commits matching these are silently dropped
# ---------------------------------------------------------------------------
SKIP_PATTERNS = [
    re.compile(r"^GIT_SILENT", re.IGNORECASE),
    re.compile(r"^SVN_SILENT", re.IGNORECASE),
    re.compile(r"Sync po/docbooks", re.IGNORECASE),
    re.compile(r"made messages", re.IGNORECASE),
    re.compile(r"Starting KStars \d+\.\d+\.\d+ Development Cycle", re.IGNORECASE),
    re.compile(r"Bump to \d+\.\d+\.\d+", re.IGNORECASE),
    re.compile(r"Mark KStars .* as stable", re.IGNORECASE),
    re.compile(r"^fix astyle$", re.IGNORECASE),
    re.compile(r"^Fix astyle$", re.IGNORECASE),
    re.compile(r"^astyle$", re.IGNORECASE),
    re.compile(r"fix all warnings$", re.IGNORECASE),
    re.compile(r"Fix all warnings$", re.IGNORECASE),
    re.compile(r"Use Qt6-compatible", re.IGNORECASE),
    re.compile(r"Fix deprecated.*Qt6", re.IGNORECASE),
    re.compile(r"fix warnings", re.IGNORECASE),
    re.compile(r"Fix warnings", re.IGNORECASE),
    re.compile(r"Fix compile", re.IGNORECASE),    
    re.compile(r"always resolve ours", re.IGNORECASE),
]

# ---------------------------------------------------------------------------
# Component categories — ordered by priority (first match wins)
# ---------------------------------------------------------------------------
CATEGORIES = [
    ("Scheduler & Observatory Automation", [
        r"scheduler", r"observatory", r"shutdown", r"startup",
        r"weather alert", r"dawn", r"pre-shutdown", r"post-shutdown",
        r"startup queue", r"esq file", r"esl file", r"autopark",
        r"observatory.*start", r"skip shutdown",
    ]),
    ("Live Stacking", [
        r"live.?stack", r"livestack", r"gradient removal", r"seestar",
        r"alt/az data", r"stacking.*signal", r"stacking.*buffer",
        r"stack.*mutex",
    ]),
    ("Polar Alignment", [
        r"polar.?align", r"paa", r"polar alignment corrector",
        r"auto.*switch direction.*polar", r"corrector commands",
    ]),
    ("Alignment & Mount Modeler", [
        r"align", r"plate.?solv", r"mount model", r"astrometry",
        r"artificial horizon.*mount", r"horizon.*model",
        r"rotator.*align", r"focal length",
    ]),
    ("Guide", [
        r"\bguide\b", r"\bguiding\b", r"guide.*stream", r"guide.*pipeline",
        r"16-bit.*stream.*guide", r"stream.*binning.*gain",
    ]),
    ("Camera & Capture", [
        r"\bcapture\b", r"\bcamera\b", r"\bccd\b", r"stream frame",
        r"stream.*full_depth", r"16-bit stream", r"passive.*warm",
        r"camera.*warm", r"debayer", r"opencv.*debayer",
        r"roi.*bayer", r"capture.*action", r"capture.*timeout",
    ]),
    ("FITS Viewer & File Handling", [
        r"\bfits\b", r"\bxisf\b", r"cfitsio", r"compressed.*fits",
        r"\.fits\.gz", r"\.xisf\.gz", r"fptr", r"gzip compress",
        r"fits.*data", r"fits.*viewer",
    ]),
    ("Optical Trains & DBus API", [
        r"optical train", r"train id", r"32bit.*train", r"train.*dbus",
        r"\bdbus\b", r"d-bus", r"dbus.*profile", r"dbus.*pictures",
        r"dbus.*ekos", r"optical trains dbus",
    ]),
    ("Artificial Horizon", [
        r"artificial horizon", r"artficial horizon",
        r"horizon.*import", r"horizon.*toggle",
    ]),
    ("Video Streaming", [
        r"video stream", r"stream.*toggle", r"stream.*train",
        r"stream.*window", r"guide.*stream", r"nan.*stream",
        r"stream.*artifact", r"stream.*crash",
    ]),
    ("Ekos Profiles & Connection", [
        r"profile.*script", r"creating.*profile", r"delete.*profile",
        r"ekos profile", r"indi.*web manager", r"dns resolution",
        r"remote host", r"device token", r"token file",
        r"indi.*driver", r"clientmanager",
    ]),
    ("Rotator", [
        r"rotator", r"preserve rotator", r"reversed rotator",
        r"rotator.*state", r"rotator.*button",
    ]),
    ("Stability & Bug Fixes", [
        r"fix crash", r"fix.*crash", r"crash.*fix", r"dangling.*pointer",
        r"segv", r"memory", r"nan protection", r"rare crash",
        r"invalid.*device", r"crash-reports",
    ]),
    ("Build & Infrastructure", [
        r"docker", r"flatpak", r"cmake", r"ubuntu.*qt6",
        r"arm64", r"lldb", r"appimage",
    ]),
]

# ---------------------------------------------------------------------------
# Style reference — used as few-shot example for the LLM prompt
# ---------------------------------------------------------------------------
BLOG_STYLE_EXAMPLE = """
KStars v3.8.2 is released on 2026.04.08 for Windows, Linux, and MacOS.

For Linux users, it's highly recommended to use the official KStars Flatpak hosted at Flathub.

This release brings reliability improvements across the scheduler, live stacking, polar alignment, along with new features like passive camera warming, a startup queue failure popup, and preliminary automatic polar alignment correction. Here are some highlights:

Scheduler & Observatory Automation

  - Fixed Ekos not shutting down after preemptive shutdown; task executor no longer stalls when a park command is issued
  - New startup queue failure popup with state reset for clean retries
  - Scheduler restart from error state now always resets startup to IDLE
  - Fixed crashes caused by improper use of QSharedPointer; simulator-based regression tests expanded
  - Added ability to wait indefinitely for weather to improve
  - Reset scheduler pre/post-shutdown scripts to safe defaults inside Flatpak (filesystem access restrictions)
  - Ensures scheduler target names are now always unique

Live Stacking

  - Gradient removal support added — cleaner backgrounds for stacked frames
  - Alt/Az coordinate data support for Seestar integration
  - Mutex lock on image buffer updates prevents rare SEGV crashes
  - Fixed livestacking frames stacking signal

Polar Alignment

  - New option to automatically switch direction when applying corrector commands — less manual fiddling during PAA sessions

Camera & Capture

  - Passive camera warming support added — camera sensors can now pre-warm before a session starts
  - 16-bit stream frame support for the guide pipeline (courtesy of Andreas R.)
  - Fixed crash and distorted artifacts when streaming
  - Fixed switching video stream between optical trains

FITS Viewer

  - Fixed crash when closing compressed FITS files
  - Fixed opening of compressed FITS files (patch by Umangjeet Singh Pahwa)
  - Fixed compressed buffer memory issues

DBus & Scripting API

  - Added Optical Trains DBus interface
  - Added support for creating and getting Ekos profiles via DBus

Artificial Horizon

  - Added DBus commands for artificial horizon import and toggle
  - Fixed artificial horizon import

This release also includes train ID expansion from 8-bit to 32-bit to prevent running out of IDs over time, better logging for diagnosing capture action timeouts, and a fix for the Preserve Rotator Angle not being honored during load-and-slew operations.

Many thanks to John Evans, Eric Dejouhanet, Wolfgang Reissenberger, Umangjeet Singh Pahwa, and all others who contributed fixes and improvements to this release!
""".strip()


def run_git(args, repo_path="."):
    """Run a git command and return its stdout as a string."""
    result = subprocess.run(
        ["git"] + args,
        cwd=repo_path,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"[ERROR] git {' '.join(args)} failed:\n{result.stderr}", file=sys.stderr)
        sys.exit(1)
    return result.stdout.strip()


# Use ASCII record separator (\x1e) between commits and unit separator (\x1f) between fields.
# This survives newlines inside commit bodies cleanly.
_GIT_FMT = "%H\x1f%an\x1f%ad\x1f%s\x1f%b\x1e"


def get_commits_by_ref(from_ref, to_ref, repo_path="."):
    """Return list of commit dicts between two git refs."""
    log = run_git(
        ["log", f"{from_ref}..{to_ref}",
         f"--pretty=format:{_GIT_FMT}", "--date=short"],
        repo_path,
    )
    return _parse_commits(log)


def get_commits_by_date(from_date, to_date, branch="master", repo_path="."):
    """Return list of commit dicts within a date range."""
    log = run_git(
        ["log", branch,
         f"--after={from_date}", f"--before={to_date}",
         f"--pretty=format:{_GIT_FMT}", "--date=short"],
        repo_path,
    )
    return _parse_commits(log)


def _parse_commits(raw):
    """Parse git log output that uses \x1e as record sep and \x1f as field sep."""
    commits = []
    for record in raw.split("\x1e"):
        record = record.strip()
        if not record:
            continue
        parts = record.split("\x1f", 4)
        if len(parts) < 4:
            continue
        body = parts[4].strip() if len(parts) > 4 else ""
        commits.append({
            "hash": parts[0].strip(),
            "author": parts[1].strip(),
            "date": parts[2].strip(),
            "subject": parts[3].strip(),
            "body": body,
        })
    return commits


def filter_commits(commits):
    """Remove noise commits that add no value to the blog post."""
    kept = []
    for c in commits:
        subject = c["subject"]
        skip = False
        for pattern in SKIP_PATTERNS:
            if pattern.search(subject):
                skip = True
                break
        if not skip:
            kept.append(c)
    return kept


def categorize_commits(commits):
    """Group commits into component buckets. Returns dict[category] -> list[commit]."""
    categorized = defaultdict(list)
    uncategorized = []

    for c in commits:
        subject_lower = c["subject"].lower()
        matched = False
        for category, patterns in CATEGORIES:
            for pat in patterns:
                if re.search(pat, subject_lower):
                    categorized[category].append(c)
                    matched = True
                    break
            if matched:
                break
        if not matched:
            uncategorized.append(c)

    if uncategorized:
        categorized["Other Improvements"] = uncategorized

    return categorized


def get_contributor_stats(commits):
    """Return sorted list of (author, count) excluding bots."""
    BOT_NAMES = {"l10n daemon script", "scripty"}
    counts = defaultdict(int)
    for c in commits:
        name = c["author"].strip()
        if name.lower() not in BOT_NAMES:
            counts[name] += 1
    return sorted(counts.items(), key=lambda x: -x[1])


def get_date_range(commits):
    """Return (earliest_date, latest_date) strings from commit list."""
    dates = [c["date"] for c in commits if c["date"]]
    if not dates:
        return "unknown", "unknown"
    return min(dates), max(dates)


def _escape_html(text):
    """Escape special HTML characters in plain text."""
    return (
        text.replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;")
    )


def _linkify(text):
    """Turn bare http(s) URLs in text into <a href> links."""
    return re.sub(
        r"(https?://[^\s\)\]\"'<>]+)",
        r'<a href="\1">\1</a>',
        text,
    )


def _body_to_html(body):
    """Convert a plain-text commit body into minimal HTML paragraphs."""
    if not body:
        return ""
    paragraphs = []
    current = []
    for line in body.splitlines():
        stripped = line.strip()
        if not stripped:
            if current:
                paragraphs.append(" ".join(current))
                current = []
        else:
            # Strip leading ## / # (markdown headings in commit bodies)
            stripped = re.sub(r"^#{1,3}\s*", "", stripped)
            current.append(_escape_html(stripped))
    if current:
        paragraphs.append(" ".join(current))
    if not paragraphs:
        return ""
    return "\n".join(f"<p>{_linkify(p)}</p>" for p in paragraphs)


def _format_commit_entry(c, indent="  "):
    """Format a single commit as subject + indented body for Markdown display."""
    lines = [f"- {c['subject']}"]
    body = c.get("body", "").strip()
    if body:
        for line in body.splitlines():
            lines.append(f"{indent}  {line}" if line.strip() else "")
    return "\n".join(lines)


def build_structured_summary(version, commits, categorized, contributors, date_range, fmt="markdown"):
    """Build a structured summary in the requested format (no LLM needed)."""
    if fmt == "html":
        return _build_html_summary(version, commits, categorized, contributors, date_range)
    return _build_markdown_summary(version, commits, categorized, contributors, date_range)


def _build_markdown_summary(version, commits, categorized, contributors, date_range):
    """Build a clean Markdown structured summary."""
    lines = []
    today = datetime.now().strftime("%Y.%m.%d")

    lines.append(f"# KStars {version} Released\n")
    lines.append(f"KStars v{version} is released on {today} for Windows, Linux, and MacOS.\n")
    lines.append(
        "For Linux users, it's highly recommended to use the official "
        "[KStars Flatpak](https://flathub.org/apps/org.kde.kstars) hosted at Flathub.\n"
    )
    lines.append(
        f"This release covers changes from {date_range[0]} to {date_range[1]} "
        f"with {len(commits)} commits across {len(contributors)} contributors.\n"
    )
    lines.append("## Highlights\n")

    for category, cat_commits in categorized.items():
        if not cat_commits:
            continue
        lines.append(f"### {category}\n")
        for c in cat_commits:
            lines.append(_format_commit_entry(c))
        lines.append("")

    non_main = [name for name, _ in contributors if name != "Jasem Mutlaq"]
    if non_main:
        names_str = ", ".join(non_main[:8])
        if len(non_main) > 8:
            names_str += f", and {len(non_main) - 8} others"
        lines.append(
            f"\nMany thanks to {names_str}, and everyone else who contributed "
            f"to making this release possible!"
        )

    return "\n".join(lines)


def _build_html_summary(version, commits, categorized, contributors, date_range):
    """Build a structured HTML summary ready to paste into Blogger."""
    today = datetime.now().strftime("%Y.%m.%d")
    flatpak_url = "https://flathub.org/apps/org.kde.kstars"
    windows_url  = "https://edu.kde.org/kstars/#download"
    linux_url    = "https://flathub.org/apps/org.kde.kstars"
    macos_url    = "https://edu.kde.org/kstars/#download"

    parts = []
    parts.append(
        f'<p>KStars v{_escape_html(version)} is released on {today} for '
        f'<a href="{windows_url}">Windows</a>, '
        f'<a href="{linux_url}">Linux</a>, and '
        f'<a href="{macos_url}">MacOS</a>.</p>'
    )
    parts.append(
        f'<p>For Linux users, it\'s highly recommended to use the official '
        f'<a href="{flatpak_url}">KStars Flatpak</a> hosted at Flathub.</p>'
    )
    parts.append(
        f'<p>This release covers changes from <em>{date_range[0]}</em> to '
        f'<em>{date_range[1]}</em> with <strong>{len(commits)}</strong> commits '
        f'across <strong>{len(contributors)}</strong> contributors.</p>'
    )

    for category, cat_commits in categorized.items():
        if not cat_commits:
            continue
        parts.append(f"\n<h2>{_escape_html(category)}</h2>")
        parts.append("<ul>")
        for c in cat_commits:
            subject_html = _linkify(_escape_html(c["subject"]))
            body_html = _body_to_html(c.get("body", ""))
            # Attribution: show author in bold when it's not the lead developer
            author = c.get("author", "").strip()
            attribution = (
                f' <i>(by <b>{_escape_html(author)}</b>)</i>'
                if author and author != "Jasem Mutlaq"
                else ""
            )
            if body_html:
                parts.append(f"  <li><b>{subject_html}</b>{attribution}\n  {body_html}</li>")
            else:
                parts.append(f"  <li>{subject_html}{attribution}</li>")
        parts.append("</ul>")

    non_main = [name for name, _ in contributors if name != "Jasem Mutlaq"]
    if non_main:
        names_html = ", ".join(f"<b>{_escape_html(n)}</b>" for n in non_main[:8])
        if len(non_main) > 8:
            names_html += f", and {len(non_main) - 8} others"
        parts.append(
            f'\n<p>Many thanks to {names_html}, and everyone else who contributed '
            f'to making this release possible!</p>'
        )

    return "\n".join(parts)


def _format_commit_for_prompt(c):
    """Format a commit as 'subject [Author]' + body for inclusion in the LLM prompt."""
    subject = c["subject"]
    author = c.get("author", "").strip()
    body = c.get("body", "").strip()
    # Always include author so the LLM can attribute correctly
    author_tag = f" [by {author}]" if author else ""
    if body:
        return f"  - {subject}{author_tag}\n    Details: {body}"
    return f"  - {subject}{author_tag}"


def resolve_instructions(value):
    """
    Resolve --instructions value to a string.
    If value is a path to an existing file, read and return its contents.
    Otherwise treat it as a literal instruction string.
    Returns None if value is None/empty.
    """
    if not value:
        return None
    stripped = value.strip()
    if os.path.isfile(stripped):
        with open(stripped, "r", encoding="utf-8") as f:
            content = f.read().strip()
        print(f"[INFO] Loaded extra instructions from file: {stripped}", file=sys.stderr)
        return content
    return stripped


def build_llm_prompt(version, commits, categorized, contributors, date_range, fmt="markdown", extra_instructions=None):
    """Build the prompt to send to the LLM."""

    # Flatten categorized commits into a readable list per section,
    # including the full commit body so the LLM has all the context it needs.
    sections_text = []
    for category, cat_commits in categorized.items():
        if not cat_commits:
            continue
        items = "\n".join(_format_commit_for_prompt(c) for c in cat_commits)
        sections_text.append(f"{category}:\n{items}")

    all_sections = "\n\n".join(sections_text)

    contributor_list = ", ".join(
        f"{name} ({count} commit{'s' if count > 1 else ''})"
        for name, count in contributors
        if name != "Jasem Mutlaq"
    )

    today = datetime.now().strftime("%Y.%m.%d")
    flatpak_url = "https://flathub.org/apps/org.kde.kstars"
    download_url = "https://edu.kde.org/kstars/#download"

    if fmt == "html":
        format_instructions = f"""\
Output the blog post as clean HTML suitable for pasting directly into Blogger.
Formatting rules:
- Use <h2> for each section heading (e.g. <h2>Scheduler &amp; Observatory Automation</h2>)
- Use <ul> and <li> for bullet points
- Use <b> for contributor/author names — ALWAYS bold them, everywhere they appear
- When describing a feature contributed by someone other than Jasem Mutlaq, attribute it
  naturally in the sentence, e.g. "<b>Christian Kemper</b> added Artificial Horizon filtering
  to the Mount Modeler" or "Thanks to <b>Andreas Ruthner</b>, guide streaming now correctly
  syncs frame, binning, gain and exposure". Never just drop a bare name.
- Use <b> or <strong> for other emphasis within a bullet
- Use <a href="URL">text</a> for any links
- Use <p> for intro and closing paragraphs
- Include these links where appropriate:
    KStars Flatpak: <a href="{flatpak_url}">KStars Flatpak</a>
    Download page: <a href="{download_url}">Windows, Linux, and MacOS</a>
- Do NOT output markdown, code fences, or any non-HTML formatting
- Do NOT include <html>, <head>, or <body> tags — just the content fragment"""
    else:
        format_instructions = """\
Output the blog post as clean Markdown.
Use ## for section headings, bullet points with -, and **bold** / *italic* where helpful.
Contributor/author names must always be **bold** wherever they appear.
When describing a feature contributed by someone other than Jasem Mutlaq, attribute it
naturally in the sentence, e.g. "**Christian Kemper** added Artificial Horizon filtering
to the Mount Modeler" or "Thanks to **Andreas Ruthner**, guide streaming now correctly
syncs frame, binning, gain and exposure". Never just drop a bare name without context.
Include links as [text](url) where relevant."""

    extra_section = (
        f"\nAdditional instructions from the author:\n{extra_instructions}\n"
        if extra_instructions else ""
    )

    prompt = f"""You are helping write a release blog post for KStars, the free astronomy software.
The blog is written by Jasem Mutlaq (the lead developer) at https://knro.blogspot.com/

Here is a real example of a previous blog post to use as a style reference:

---
{BLOG_STYLE_EXAMPLE}
---

Now write a similar blog post for KStars {version}, released on {today}.

Use the same conversational, enthusiastic tone. Write in first-person where natural.
Group changes under clear section headings.
Use bullet points under each heading.
Keep the intro paragraph short and punchy — mention the biggest themes of this release.
Mention contributors by name where appropriate (especially non-lead contributors).
End with a warm thank-you to contributors and a call to download/try the release.
Do NOT add placeholder text like "[insert image here]" — the author will handle images separately.

{format_instructions}
{extra_section}
Here are the categorized changes from the git history ({date_range[0]} to {date_range[1]}, {len(commits)} meaningful commits):

{all_sections}

Other contributors (besides Jasem Mutlaq): {contributor_list or "none listed"}

Write the full blog post now:
"""
    return prompt


def generate_with_claude(prompt, model="claude-sonnet-4-5"):
    """Generate text using the Anthropic Claude API."""
    try:
        import anthropic
    except ImportError:
        print(
            "[ERROR] The 'anthropic' package is not installed.\n"
            "        Run: pip install anthropic",
            file=sys.stderr,
        )
        sys.exit(1)

    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print(
            "[ERROR] ANTHROPIC_API_KEY environment variable is not set.",
            file=sys.stderr,
        )
        sys.exit(1)

    client = anthropic.Anthropic(api_key=api_key)
    print(f"[INFO] Sending request to Claude ({model})...", file=sys.stderr)

    message = client.messages.create(
        model=model,
        max_tokens=4096,
        messages=[{"role": "user", "content": prompt}],
    )
    return message.content[0].text


def generate_with_ollama(prompt, model="llama3.2"):
    """Generate text using a local Ollama instance."""
    try:
        import ollama
    except ImportError:
        print(
            "[ERROR] The 'ollama' package is not installed.\n"
            "        Run: pip install ollama",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"[INFO] Sending request to Ollama ({model})...", file=sys.stderr)
    response = ollama.chat(
        model=model,
        messages=[{"role": "user", "content": prompt}],
    )
    return response["message"]["content"]


def resolve_repo_path(repo_path):
    """Resolve repository path; default to current directory."""
    if repo_path:
        return os.path.abspath(repo_path)
    # Try to detect from script location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Script lives in tools/ so the repo root is one level up
    candidate = os.path.dirname(script_dir)
    if os.path.isdir(os.path.join(candidate, ".git")):
        return candidate
    # Fallback to cwd
    return os.getcwd()


def list_stable_branches(repo_path):
    """Return a sorted list of local stable-* branches."""
    output = run_git(["branch", "--list", "stable-*"], repo_path)
    branches = [b.strip().lstrip("* ") for b in output.splitlines() if b.strip()]
    branches.sort()
    return branches


def auto_detect_from_ref(repo_path):
    """Try to auto-detect the previous stable branch."""
    branches = list_stable_branches(repo_path)
    if len(branches) >= 2:
        return branches[-2]  # second-to-last stable branch
    if branches:
        return branches[-1]
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Generate a KStars release blog post from git history.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # Reference-based range
    ref_group = parser.add_argument_group("Git ref range (recommended)")
    ref_group.add_argument(
        "--from-ref",
        metavar="REF",
        help="Starting git ref (e.g., stable-3.8.2). Auto-detected if omitted.",
    )
    ref_group.add_argument(
        "--to-ref",
        metavar="REF",
        default="master",
        help="Ending git ref (e.g., stable-3.8.3 or master). Default: master",
    )

    # Date-based range (alternative)
    date_group = parser.add_argument_group("Date range (alternative)")
    date_group.add_argument(
        "--from-date",
        metavar="YYYY-MM-DD",
        help="Start date (inclusive). Requires --to-ref or --to-date.",
    )
    date_group.add_argument(
        "--to-date",
        metavar="YYYY-MM-DD",
        help="End date (inclusive). Default: today.",
    )

    # Release info
    parser.add_argument(
        "--version",
        metavar="X.Y.Z",
        required=True,
        help="KStars version being released (e.g., 3.8.3).",
    )

    # LLM options
    parser.add_argument(
        "--llm",
        choices=["claude", "ollama", "none"],
        default="claude",
        help="LLM backend to use for text generation. Default: claude",
    )
    parser.add_argument(
        "--model",
        default="claude-sonnet-4-5",
        help="Claude model name. Default: claude-sonnet-4-5",
    )
    parser.add_argument(
        "--ollama-model",
        default="llama3.2",
        metavar="MODEL",
        help="Ollama model name. Default: llama3.2",
    )

    # Format
    parser.add_argument(
        "--format",
        choices=["markdown", "html"],
        default="markdown",
        help="Output format: markdown (default) or html (uses h2, ul/li, b, em, a).",
    )

    # Extra instructions for the LLM
    parser.add_argument(
        "--instructions",
        metavar="TEXT_OR_FILE",
        help=(
            "Extra instructions appended to the LLM prompt. "
            "Can be a plain string or a path to a .txt file. "
            "Example: --instructions 'Focus more on the Alignment improvements.' "
            "         --instructions my_notes.txt"
        ),
    )

    # Output
    parser.add_argument(
        "--output",
        metavar="FILE",
        help="Save output to this file (in addition to printing to stdout).",
    )
    parser.add_argument(
        "--repo",
        metavar="PATH",
        help="Path to the KStars git repository. Default: auto-detect from script location.",
    )
    parser.add_argument(
        "--list-branches",
        action="store_true",
        help="List available stable branches and exit.",
    )

    args = parser.parse_args()

    repo_path = resolve_repo_path(args.repo)
    print(f"[INFO] Repository: {repo_path}", file=sys.stderr)

    if args.list_branches:
        print("Available stable branches:")
        for b in list_stable_branches(repo_path):
            print(f"  {b}")
        sys.exit(0)

    # -----------------------------------------------------------------------
    # Determine commit range
    # -----------------------------------------------------------------------
    if args.from_date:
        to_date = args.to_date or datetime.now().strftime("%Y-%m-%d")
        branch = args.to_ref if args.to_ref != "master" else "master"
        print(
            f"[INFO] Collecting commits on '{branch}' "
            f"from {args.from_date} to {to_date}...",
            file=sys.stderr,
        )
        commits = get_commits_by_date(args.from_date, to_date, branch, repo_path)
    else:
        from_ref = args.from_ref
        if not from_ref:
            from_ref = auto_detect_from_ref(repo_path)
            if not from_ref:
                print(
                    "[ERROR] Could not auto-detect --from-ref. "
                    "Please specify it explicitly.",
                    file=sys.stderr,
                )
                sys.exit(1)
            print(f"[INFO] Auto-detected --from-ref: {from_ref}", file=sys.stderr)

        print(
            f"[INFO] Collecting commits from '{from_ref}' to '{args.to_ref}'...",
            file=sys.stderr,
        )
        commits = get_commits_by_ref(from_ref, args.to_ref, repo_path)

    print(f"[INFO] Total raw commits: {len(commits)}", file=sys.stderr)

    # -----------------------------------------------------------------------
    # Filter and categorize
    # -----------------------------------------------------------------------
    commits = filter_commits(commits)
    print(f"[INFO] Commits after filtering: {len(commits)}", file=sys.stderr)

    if not commits:
        print("[WARNING] No meaningful commits found in the specified range.", file=sys.stderr)
        sys.exit(0)

    categorized = categorize_commits(commits)
    contributors = get_contributor_stats(commits)
    date_range = get_date_range(commits)

    print(f"[INFO] Categories found: {', '.join(categorized.keys())}", file=sys.stderr)
    print(
        f"[INFO] Contributors: {', '.join(name for name, _ in contributors[:5])}{'...' if len(contributors) > 5 else ''}",
        file=sys.stderr,
    )

    # -----------------------------------------------------------------------
    # Generate blog text
    # -----------------------------------------------------------------------
    out_format = args.format  # "markdown" or "html"
    print(f"[INFO] Output format: {out_format}", file=sys.stderr)

    extra_instructions = resolve_instructions(args.instructions)
    if extra_instructions:
        print(f"[INFO] Extra instructions: {extra_instructions[:80]}{'...' if len(extra_instructions) > 80 else ''}", file=sys.stderr)

    if args.llm == "none":
        print("[INFO] Generating structured summary (no LLM)...", file=sys.stderr)
        if extra_instructions:
            print("[INFO] Note: --instructions is ignored in --llm none mode.", file=sys.stderr)
        output_text = build_structured_summary(
            args.version, commits, categorized, contributors, date_range, fmt=out_format
        )
    else:
        prompt = build_llm_prompt(
            args.version, commits, categorized, contributors, date_range,
            fmt=out_format, extra_instructions=extra_instructions
        )

        if args.llm == "claude":
            output_text = generate_with_claude(prompt, model=args.model)
        elif args.llm == "ollama":
            output_text = generate_with_ollama(prompt, model=args.ollama_model)

    # -----------------------------------------------------------------------
    # Output
    # -----------------------------------------------------------------------
    print("\n" + "=" * 70)
    print(output_text)
    print("=" * 70 + "\n")

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(output_text)
        print(f"[INFO] Blog post saved to: {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
