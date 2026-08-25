#!/usr/bin/env python3
"""Render the hand-authored Gherkin manual test plan under features/*.feature
into an accessible, static HTML site under features/html/.

This is a small, PURPOSE-BUILT parser for the specific writing style used in
this repository's .feature files, not a general Gherkin implementation. It
relies on the following conventions, all of which are consistent across the
9 files at the time of writing (verified by inspection, not assumed):

  * One `Feature:` per file, at column 0, optionally preceded by a single
    line of `@tag @tag2` feature-level tags (also column 0).
  * A free-text description block (2-space indented) between `Feature:` and
    the first `Background:`.
  * Exactly one `Background:` per file (2-space indented), whose body is a
    flat list of `Given`/`And` steps (4-space indented).
  * Zero or more `Scenario:` / `Scenario Outline:` blocks (2-space indented),
    each optionally preceded by one `@tag @tag2` line (2-space indented).
  * Step lines (`Given`/`When`/`Then`/`And`/`But`, 4-space indented).
  * `Examples:` blocks (4-space indented) under a `Scenario Outline`, each
    with an optional label after the colon, holding a pipe table.
  * Occasional plain data tables directly under a step (no `Examples:`
    keyword) -- these are Gherkin step arguments, e.g. an ordered list of
    menu items a screen reader should announce.
  * Comments (`#`). Two distinct kinds, disambiguated purely by indentation,
    which is consistent across all 9 files (verified with a script before
    writing this parser):
      - 2-space indented comment blocks, always opened and closed by a
        divider line of 5+ dashes (`  # ---------`). If the block has no
        text between the dividers it is pure decoration and is discarded.
        If it has text, the first line becomes a subsection heading and the
        rest becomes a subsection description, grouping every scenario that
        follows until the next such block.
      - 4-space indented comment lines, found inside a scenario. A line of
        the form `# ---- Some Phase ----` (dashes then text then dashes) is
        a "phase marker" that breaks a long scenario's step list into
        labelled stretches (used only by blind_dj_workflow.feature's
        end-to-end scenarios). Any other 4-space comment is a free-text note
        attached to the step that immediately follows it (or, if none
        follows, to the scenario as a closing note).

Re-run this script whenever the .feature files change; it always regenerates
every output file from scratch. No output file should ever be hand-edited.
"""

from __future__ import annotations

import html
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
FEATURES_DIR = REPO_ROOT / "features"
OUT_DIR = FEATURES_DIR / "html"

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class Table:
    header: Optional[list[str]]  # None for a single-column, header-less list
    rows: list[list[str]]


@dataclass
class ExamplesTable:
    label: Optional[str]
    header: list[str]
    rows: list[list[str]]


@dataclass
class NoteItem:
    kind: str  # 'note' or 'phase'
    text: str


@dataclass
class Step:
    keyword: str
    text: str
    table: Optional[Table] = None
    notes_before: list[NoteItem] = field(default_factory=list)


@dataclass
class Scenario:
    kind: str  # 'scenario' or 'outline'
    name: str
    tags: list[str]
    steps: list[Step] = field(default_factory=list)
    examples: list[ExamplesTable] = field(default_factory=list)
    closing_notes: list[NoteItem] = field(default_factory=list)
    source_line: int = 0


@dataclass
class Section:
    title: Optional[str]
    description: list[tuple[str, object]]  # blocks, see md_paragraph_blocks
    scenarios: list[Scenario] = field(default_factory=list)


@dataclass
class Feature:
    file: str
    tags: list[str]
    name: str
    description: list[tuple[str, object]]
    background: list[Step]
    sections: list[Section]  # flat scenarios live in a Section with title=None

    def all_scenarios(self):
        for sec in self.sections:
            for sc in sec.scenarios:
                yield sec, sc


# ---------------------------------------------------------------------------
# .feature parsing
# ---------------------------------------------------------------------------

STEP_KEYWORDS = ("Given", "When", "Then", "And", "But")
STEP_RE = re.compile(r"^(Given|When|Then|And|But)\s+(.*)$")
TABLE_ROW_RE = re.compile(r"^\|(.*)\|\s*$")
DIVIDER_RE = re.compile(r"^#\s*-{5,}\s*$")
PHASE_RE = re.compile(r"^#\s*-{2,}\s*(.+?)\s*-{2,}\s*$")


def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def split_table_row(line: str) -> list[str]:
    m = TABLE_ROW_RE.match(line.strip())
    assert m, f"not a table row: {line!r}"
    return [cell.strip() for cell in m.group(1).split("|")]


class Cursor:
    """A simple index into a list of raw lines, with lookahead helpers."""

    def __init__(self, lines: list[str]):
        self.lines = lines
        self.i = 0

    def peek(self) -> Optional[str]:
        while self.i < len(self.lines) and self.lines[self.i].strip() == "":
            self.i += 1
        return self.lines[self.i] if self.i < len(self.lines) else None

    def take(self) -> str:
        line = self.peek()
        assert line is not None
        self.i += 1
        return line


def parse_comment_block(cur: Cursor, indent: int) -> list[str]:
    """Consume a contiguous run of comment lines at the given indent and
    return their text content (leading '#' and indentation stripped, but
    blank '#' separator lines kept as empty strings so callers can find
    paragraph breaks)."""
    out = []
    while cur.i < len(cur.lines):
        line = cur.lines[cur.i]
        if line.strip() == "" or indent_of(line) != indent or not line.lstrip().startswith("#"):
            break
        stripped = line.strip()[1:]  # drop '#'
        if stripped.startswith(" "):
            stripped = stripped[1:]
        out.append(stripped)
        cur.i += 1
    return out


def md_paragraph_blocks(lines: list[str]) -> list[tuple[str, object]]:
    """Turn a list of already-unindented text lines into ('p', text) /
    ('ul', [items]) / ('ol', [items]) blocks, splitting on blank lines."""
    blocks: list[tuple[str, object]] = []
    para: list[str] = []

    def flush():
        if not para:
            return
        if all(re.match(r"^[-*]\s+", ln) for ln in para):
            blocks.append(("ul", [re.sub(r"^[-*]\s+", "", ln) for ln in para]))
        elif all(re.match(r"^\d+\.\s+", ln) for ln in para):
            blocks.append(("ol", [re.sub(r"^\d+\.\s+", "", ln) for ln in para]))
        else:
            blocks.append(("p", " ".join(para)))
        para.clear()

    for ln in lines:
        if ln.strip() == "":
            flush()
        else:
            para.append(ln.strip())
    flush()
    return blocks


def parse_top_comment_block(cur: Cursor) -> Optional[Section]:
    """Consume a 2-space-indented, divider-bounded comment block. Returns a
    new Section if the block carries a title, or None if it was pure
    decoration (no text between the dividers)."""
    text_lines = parse_comment_block(cur, 2)
    assert text_lines, "empty top-level comment block"
    assert DIVIDER_RE.match("#" + text_lines[0]) or text_lines[0].strip("-") == "", (
        f"top comment block does not open with a divider: {text_lines[0]!r}"
    )
    assert DIVIDER_RE.match("#" + text_lines[-1]) or text_lines[-1].strip("-") == "", (
        f"top comment block does not close with a divider: {text_lines[-1]!r}"
    )
    interior = text_lines[1:-1]
    # Trim leading/trailing blank interior lines.
    while interior and interior[0] == "":
        interior.pop(0)
    while interior and interior[-1] == "":
        interior.pop()
    if not interior:
        return None
    title = interior[0]
    body = interior[1:]
    while body and body[0] == "":
        body.pop(0)
    description = md_paragraph_blocks(body)
    return Section(title=title, description=description)


def parse_note_run(cur: Cursor) -> list[NoteItem]:
    """Consume a contiguous run of 4-space-indented comment lines and turn
    them into NoteItems: individual phase markers, and paragraph(s) of plain
    note text."""
    raw = parse_comment_block(cur, 4)
    items: list[NoteItem] = []
    para: list[str] = []

    def flush_para():
        if para:
            items.append(NoteItem(kind="note", text=" ".join(para)))
            para.clear()

    for ln in raw:
        # PHASE_RE expects the leading '#'; reconstruct it since parse_comment_block stripped it.
        m = PHASE_RE.match("#" + ln)
        if m:
            flush_para()
            items.append(NoteItem(kind="phase", text=m.group(1)))
        elif ln.strip() == "":
            flush_para()
        else:
            para.append(ln.strip())
    flush_para()
    return items


def parse_step(cur: Cursor, notes_before: list[NoteItem]) -> Step:
    line = cur.take()
    m = STEP_RE.match(line.strip())
    assert m, f"expected a step, got {line!r}"
    keyword, text = m.group(1), m.group(2)
    table = None
    nxt = cur.peek()
    if nxt is not None and TABLE_ROW_RE.match(nxt.strip()) and indent_of(nxt) == 6:
        rows = []
        while True:
            nxt = cur.peek()
            if nxt is None or not TABLE_ROW_RE.match(nxt.strip()) or indent_of(nxt) != 6:
                break
            rows.append(split_table_row(cur.take()))
        ncols = len(rows[0])
        if ncols == 1:
            table = Table(header=None, rows=rows)
        else:
            table = Table(header=rows[0], rows=rows[1:])
    return Step(keyword=keyword, text=text, table=table, notes_before=notes_before)


def parse_examples(cur: Cursor) -> ExamplesTable:
    line = cur.take()
    stripped = line.strip()
    assert stripped.startswith("Examples:")
    label = stripped[len("Examples:") :].strip() or None
    rows = []
    while True:
        nxt = cur.peek()
        if nxt is None or not TABLE_ROW_RE.match(nxt.strip()) or indent_of(nxt) != 6:
            break
        rows.append(split_table_row(cur.take()))
    assert rows, f"Examples with no table: {line!r}"
    return ExamplesTable(label=label, header=rows[0], rows=rows[1:])


def parse_scenario(cur: Cursor, tags: list[str]) -> Scenario:
    line = cur.take()
    stripped = line.strip()
    source_line = None  # not tracked precisely; fine for this use
    if stripped.startswith("Scenario Outline:"):
        kind = "outline"
        name = stripped[len("Scenario Outline:") :].strip()
    else:
        kind = "scenario"
        name = stripped[len("Scenario:") :].strip()

    scenario = Scenario(kind=kind, name=name, tags=tags)
    pending_notes: list[NoteItem] = []

    while True:
        nxt = cur.peek()
        if nxt is None:
            break
        ind = indent_of(nxt)
        s = nxt.strip()
        if ind == 4 and s.startswith("#"):
            pending_notes.extend(parse_note_run(cur))
            continue
        if ind == 4 and STEP_RE.match(s):
            step = parse_step(cur, notes_before=pending_notes)
            pending_notes = []
            scenario.steps.append(step)
            continue
        if ind == 4 and s.startswith("Examples:"):
            scenario.examples.append(parse_examples(cur))
            continue
        # Anything else (indent 2 tag/Scenario/comment, or indent 0) ends
        # this scenario.
        break

    scenario.closing_notes = pending_notes
    return scenario


def parse_feature_file(path: Path) -> Feature:
    raw_lines = path.read_text(encoding="utf-8").splitlines()
    cur = Cursor(raw_lines)

    feature_tags: list[str] = []
    nxt = cur.peek()
    assert nxt is not None, f"empty file {path}"
    if indent_of(nxt) == 0 and nxt.strip().startswith("@"):
        feature_tags = cur.take().strip().split()

    nxt = cur.take()
    assert nxt.strip().startswith("Feature:"), f"expected Feature: in {path}, got {nxt!r}"
    feature_name = nxt.strip()[len("Feature:") :].strip()

    # Description: raw (still indented) lines up to Background:
    desc_raw: list[str] = []
    while True:
        nxt = cur.peek()
        assert nxt is not None, f"no Background: found in {path}"
        s = nxt.strip()
        if s.startswith("Background:"):
            break
        desc_raw.append(cur.take().strip())
    description = md_paragraph_blocks(desc_raw)

    cur.take()  # Background:
    background: list[Step] = []
    while True:
        nxt = cur.peek()
        if nxt is None:
            break
        if indent_of(nxt) == 4 and STEP_RE.match(nxt.strip()):
            background.append(parse_step(cur, notes_before=[]))
        else:
            break

    sections: list[Section] = [Section(title=None, description=[])]

    while True:
        nxt = cur.peek()
        if nxt is None:
            break
        ind = indent_of(nxt)
        s = nxt.strip()
        if ind == 2 and s.startswith("#"):
            new_section = parse_top_comment_block(cur)
            if new_section is not None:
                sections.append(new_section)
            continue
        if ind == 2 and s.startswith("@"):
            tags = cur.take().strip().split()
            nxt2 = cur.peek()
            assert nxt2 is not None and (
                nxt2.strip().startswith("Scenario:") or nxt2.strip().startswith("Scenario Outline:")
            ), f"tag line not followed by Scenario in {path}: {nxt2!r}"
            sections[-1].scenarios.append(parse_scenario(cur, tags))
            continue
        if ind == 2 and (s.startswith("Scenario:") or s.startswith("Scenario Outline:")):
            sections[-1].scenarios.append(parse_scenario(cur, tags=[]))
            continue
        raise AssertionError(f"unexpected line in {path} at indent {ind}: {nxt!r}")

    sections = [sec for sec in sections if sec.scenarios]

    return Feature(
        file=path.name,
        tags=feature_tags,
        name=feature_name,
        description=description,
        background=background,
        sections=sections,
    )


# ---------------------------------------------------------------------------
# Minimal Markdown -> HTML (just enough for README.md / COVERAGE.md)
# ---------------------------------------------------------------------------


def md_inline(text: str) -> str:
    text = html.escape(text, quote=False)
    # inline code `...`
    text = re.sub(r"`([^`]+)`", lambda m: f"<code>{m.group(1)}</code>", text)
    # bold **...**
    text = re.sub(r"\*\*([^*]+)\*\*", lambda m: f"<strong>{m.group(1)}</strong>", text)
    # italic *...*
    text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", lambda m: f"<em>{m.group(1)}</em>", text)
    return text


def markdown_to_html(md_text: str, heading_offset: int = 0) -> str:
    lines = md_text.splitlines()
    out: list[str] = []
    i = 0
    n = len(lines)

    def heading_tag(level: int) -> str:
        return f"h{min(level + heading_offset, 6)}"

    while i < n:
        line = lines[i]
        if line.strip() == "":
            i += 1
            continue

        # Fenced code block
        if line.strip().startswith("```"):
            i += 1
            code_lines = []
            while i < n and not lines[i].strip().startswith("```"):
                code_lines.append(lines[i])
                i += 1
            i += 1  # closing fence
            code = html.escape("\n".join(code_lines))
            out.append(f"<pre><code>{code}</code></pre>")
            continue

        # Heading. Every heading gets a stable id (slugified from its text)
        # so other pages/sections can deep-link into README.md / COVERAGE.md
        # content instead of duplicating it.
        m = re.match(r"^(#{1,6})\s+(.*)$", line)
        if m:
            level = len(m.group(1))
            hid = slugify(m.group(2))
            out.append(f"<{heading_tag(level)} id='{hid}'>{md_inline(m.group(2))}</{heading_tag(level)}>")
            i += 1
            continue

        # Table (GFM pipe table): header row, separator row, data rows
        if line.strip().startswith("|") and i + 1 < n and re.match(r"^\|?[\s:|-]+\|?$", lines[i + 1].strip()):
            header = [c.strip() for c in line.strip().strip("|").split("|")]
            i += 2
            rows = []
            while i < n and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            out.append('<table class="md-table">')
            out.append("<thead><tr>" + "".join(f'<th scope="col">{md_inline(h)}</th>' for h in header) + "</tr></thead>")
            out.append("<tbody>")
            for row in rows:
                out.append("<tr>" + "".join(f"<td>{md_inline(c)}</td>" for c in row) + "</tr>")
            out.append("</tbody></table>")
            continue

        # Unordered list
        if re.match(r"^[-*]\s+", line.strip()):
            items = []
            while i < n and (re.match(r"^[-*]\s+", lines[i].strip()) or (lines[i].strip() and not re.match(r"^[-*]\s+|^\d+\.\s+|^#{1,6}\s+|^\|", lines[i].strip()) and items)):
                text = lines[i].strip()
                if re.match(r"^[-*]\s+", text):
                    items.append(re.sub(r"^[-*]\s+", "", text))
                else:
                    items[-1] += " " + text
                i += 1
            out.append("<ul>" + "".join(f"<li>{md_inline(it)}</li>" for it in items) + "</ul>")
            continue

        # Ordered list
        if re.match(r"^\d+\.\s+", line.strip()):
            items = []
            while i < n and (re.match(r"^\d+\.\s+", lines[i].strip()) or (lines[i].strip() and not re.match(r"^[-*]\s+|^\d+\.\s+|^#{1,6}\s+|^\|", lines[i].strip()) and items)):
                text = lines[i].strip()
                if re.match(r"^\d+\.\s+", text):
                    items.append(re.sub(r"^\d+\.\s+", "", text))
                else:
                    items[-1] += " " + text
                i += 1
            out.append("<ol>" + "".join(f"<li>{md_inline(it)}</li>" for it in items) + "</ol>")
            continue

        # Paragraph: consume until blank line / block-starting line
        para = [line.strip()]
        i += 1
        while i < n and lines[i].strip() != "" and not re.match(r"^(#{1,6}\s+|[-*]\s+|\d+\.\s+|\|)", lines[i].strip()) and not lines[i].strip().startswith("```"):
            para.append(lines[i].strip())
            i += 1
        out.append(f"<p>{md_inline(' '.join(para))}</p>")

    return "\n".join(out)


# ---------------------------------------------------------------------------
# HTML rendering helpers
# ---------------------------------------------------------------------------


def slugify(text: str) -> str:
    s = text.lower()
    s = re.sub(r"[^a-z0-9]+", "-", s)
    return s.strip("-")


def e(text: str) -> str:
    return html.escape(text, quote=False)


def render_blocks(blocks: list[tuple[str, object]]) -> str:
    out = []
    for kind, payload in blocks:
        if kind == "p":
            out.append(f"<p>{e(payload)}</p>")
        elif kind == "ul":
            out.append("<ul>" + "".join(f"<li>{e(i)}</li>" for i in payload) + "</ul>")
        elif kind == "ol":
            out.append("<ol>" + "".join(f"<li>{e(i)}</li>" for i in payload) + "</ol>")
    return "\n".join(out)


def render_tags(tags: list[str]) -> str:
    if not tags:
        return ""
    items = "".join(f'<li class="tag">{e(t)}</li>' for t in tags)
    return f'<ul class="tags" aria-label="Tags">{items}</ul>'


def render_note(note: NoteItem) -> str:
    if note.kind == "phase":
        return f'<p class="step-phase">Section: {e(note.text)}</p>'
    return f'<p class="step-note">Note: {e(note.text)}</p>'


def render_table(table: Table, caption: Optional[str] = None) -> str:
    if table.header is None:
        items = "".join(f"<li>{e(r[0])}</li>" for r in table.rows)
        cap = f"<p class='table-caption'>{e(caption)}</p>" if caption else ""
        return f"{cap}<ol class='data-list'>{items}</ol>"
    out = ["<table>"]
    if caption:
        out.append(f"<caption>{e(caption)}</caption>")
    out.append("<thead><tr>" + "".join(f'<th scope="col">{e(h)}</th>' for h in table.header) + "</tr></thead>")
    out.append("<tbody>")
    for row in table.rows:
        out.append("<tr>" + "".join(f"<td>{e(c)}</td>" for c in row) + "</tr>")
    out.append("</tbody></table>")
    return "\n".join(out)


def render_examples(ex: ExamplesTable) -> str:
    caption = "Examples: " + ex.label if ex.label else "Examples"
    out = ["<table>", f"<caption>{e(caption)}</caption>"]
    out.append("<thead><tr>" + "".join(f'<th scope="col">{e(h)}</th>' for h in ex.header) + "</tr></thead>")
    out.append("<tbody>")
    for row in ex.rows:
        out.append("<tr>" + "".join(f"<td>{e(c)}</td>" for c in row) + "</tr>")
    out.append("</tbody></table>")
    return "\n".join(out)


def render_step_list(steps: list[Step]) -> str:
    """Render steps as one or more <ol> lists, split wherever a note or
    phase marker needs to appear between them (notes are never list items
    themselves, so the <ol> only ever contains real Given/When/Then steps)."""
    out = []
    open_list: list[str] = []

    def flush_list():
        if open_list:
            out.append("<ol class='steps'>" + "".join(open_list) + "</ol>")
            open_list.clear()

    for step in steps:
        if step.notes_before:
            flush_list()
            for note in step.notes_before:
                out.append(render_note(note))
        table_html = render_table(step.table) if step.table else ""
        open_list.append(f"<li><span class='keyword'>{e(step.keyword)}</span> {e(step.text)}{table_html}</li>")
    flush_list()
    return "\n".join(out)


def render_scenario(feature_slug: str, scenario: Scenario, index: int, heading_level: int) -> str:
    sid = f"{feature_slug}-s{index}-{slugify(scenario.name)}"
    result_name = f"{sid}-result"
    key = f"{feature_slug}::{sid}"

    kind_label = "Scenario Outline" if scenario.kind == "outline" else "Scenario"
    out = [f"<article class='scenario' id='{sid}' data-tags='{e(' '.join(scenario.tags))}'>"]
    out.append(f"<h{heading_level}>{e(kind_label)}: {e(scenario.name)}</h{heading_level}>")
    out.append(render_tags(scenario.tags))
    out.append(render_step_list(scenario.steps))
    for ex in scenario.examples:
        out.append(render_examples(ex))
    for note in scenario.closing_notes:
        out.append(render_note(note))

    out.append("<fieldset class='result'>")
    out.append(f"<legend>Result for &ldquo;{e(scenario.name)}&rdquo;</legend>")
    for value, label in (("pass", "Pass"), ("fail", "Fail"), ("skip", "Skip"), ("not-tested", "Not tested")):
        rid = f"{sid}-{value}"
        checked = " checked" if value == "not-tested" else ""
        out.append(
            f"<span class='radio-option'><input type='radio' id='{rid}' name='{result_name}' "
            f"value='{value}' data-persist-key='{key}::result'{checked}>"
            f"<label for='{rid}'>{e(label)}</label></span>"
        )
    notes_id = f"{sid}-notes"
    out.append(f"<div class='notes-field'><label for='{notes_id}'>Notes for &ldquo;{e(scenario.name)}&rdquo;</label>")
    out.append(f"<textarea id='{notes_id}' data-persist-key='{key}::notes' rows='3'></textarea></div>")
    out.append("</fieldset>")
    out.append("</article>")
    return "\n".join(out)


PAGE_ORDER = [
    "ddj400_hardware.feature",
    "macos_voiceover.feature",
    "windows_screenreader.feature",
    "audio_path.feature",
    "first_run_boot.feature",
    "keyboard_only.feature",
    "destructive_actions.feature",
    "library_and_dialogs.feature",
    "blind_dj_workflow.feature",
]


def site_nav(current: Optional[str]) -> str:
    items = []

    def link(href, label, key):
        cur = " aria-current='page'" if key == current else ""
        return f"<li><a href='{href}'{cur}>{label}</a></li>"

    items.append(link("index.html", "Overview &amp; tag legend", "index"))
    items.append(link("coverage.html", "Coverage (what is NOT tested)", "coverage"))
    for fname in PAGE_ORDER:
        slug = fname.replace(".feature", "")
        items.append(link(f"{slug}.html", e(fname), slug))
    # No heading inside this nav: it is identified by its landmark role and
    # aria-label alone, so it does not inject a heading before the page's
    # own <h1> (which must be the first heading a screen reader encounters).
    return (
        "<nav aria-label='Test plan navigation'>"
        f"<ul>{''.join(items)}</ul>"
        "</nav>"
    )


def page_shell(title: str, current: str, main_html: str, extra_head: str = "") -> str:
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{e(title)} — Mixxx accessibility test plan</title>
<link rel="stylesheet" href="style.css">
{extra_head}
</head>
<body>
<a class="skip-link" href="#main">Skip to main content</a>
<header>
<p class="site-title"><a href="index.html">Mixxx accessibility manual test plan</a></p>
{site_nav(current)}
</header>
<main id="main">
{main_html}
</main>
<footer>
<p>Generated from the <code>.feature</code> files under <code>features/</code> by
<code>tools/gherkin_to_html.py</code>. Do not hand-edit these HTML files — re-run the
script after changing a <code>.feature</code> file, <code>README.md</code> or
<code>COVERAGE.md</code>.</p>
</footer>
<script src="progress.js"></script>
</body>
</html>
"""


def render_feature_page(feature: Feature, prev_slug: Optional[str], next_slug: Optional[str]) -> str:
    slug = feature.file.replace(".feature", "")
    out = [f"<h1>{e(feature.name)}</h1>"]
    out.append(f"<p class='file-name'>File: <code>features/{e(feature.file)}</code></p>")
    out.append(render_tags(feature.tags))
    out.append(render_blocks(feature.description))

    scenario_count = sum(len(sec.scenarios) for sec in feature.sections)

    # In-page contents list.
    out.append("<nav aria-label='Scenarios in this file'>")
    out.append(f"<h2>Contents ({scenario_count} scenarios)</h2>")
    out.append("<ol>")
    idx = 0
    for sec in feature.sections:
        for sc in sec.scenarios:
            idx += 1
            sid = f"{slug}-s{idx}-{slugify(sc.name)}"
            kind_label = "Scenario Outline" if sc.kind == "outline" else "Scenario"
            out.append(f"<li><a href='#{sid}'>{e(kind_label)}: {e(sc.name)}</a></li>")
    out.append("</ol>")
    out.append("</nav>")

    if feature.background:
        out.append("<section aria-labelledby='background-heading'>")
        out.append("<h2 id='background-heading'>Background</h2>")
        out.append("<p>Every scenario below assumes these steps have already been done.</p>")
        out.append(
            "<ol class='steps'>"
            + "".join(f"<li><span class='keyword'>{e(s.keyword)}</span> {e(s.text)}</li>" for s in feature.background)
            + "</ol>"
        )
        out.append("</section>")

    idx = 0
    for sec in feature.sections:
        if sec.title:
            sec_id = f"{slug}-section-{slugify(sec.title)}"
            out.append(f"<section aria-labelledby='{sec_id}'>")
            out.append(f"<h2 id='{sec_id}'>{e(sec.title)}</h2>")
            out.append(render_blocks(sec.description))
            heading_level = 3
        else:
            out.append("<section>")
            heading_level = 2
        for sc in sec.scenarios:
            idx += 1
            out.append(render_scenario(slug, sc, idx, heading_level))
        out.append("</section>")

    nav_links = ["<nav aria-label='Adjacent feature files'><ul class='prev-next'>"]
    if prev_slug:
        nav_links.append(f"<li><a href='{prev_slug}.html'>&larr; Previous file: {e(prev_slug)}.feature</a></li>")
    nav_links.append("<li><a href='index.html'>Back to overview</a></li>")
    if next_slug:
        nav_links.append(f"<li><a href='{next_slug}.html'>Next file: {e(next_slug)}.feature &rarr;</a></li>")
    nav_links.append("</ul></nav>")
    out.append("".join(nav_links))

    return page_shell(feature.name, slug, "\n".join(out))


def render_index_page(features: list[Feature], readme_text: str) -> str:
    total_scenarios = sum(sum(len(sec.scenarios) for sec in f.sections) for f in features)

    out = ["<h1>Mixxx accessibility manual test plan</h1>"]
    out.append(
        "<p>This is an HTML rendering of the hand-written Gherkin manual test plan in "
        "<code>features/*.feature</code>, built for a tester who is using a screen reader "
        "to work through it. Every scenario has real Pass / Fail / Skip / Not tested controls "
        "and a notes box; your answers are saved to this browser's local storage as you go, "
        "so you can close the tab and come back later on the same machine.</p>"
    )
    out.append(
        f"<p><strong>{total_scenarios} scenarios</strong> across {len(features)} feature files. "
        "<a href='coverage.html'>See what is still <strong>not</strong> tested</a> before assuming a clean run means full coverage.</p>"
    )

    out.append("<h2>Files</h2>")
    out.append("<p>Jump directly to any feature file's scenarios:</p>")
    out.append("<table>")
    out.append("<thead><tr><th scope='col'>File</th><th scope='col'>Feature</th><th scope='col'>Scenarios</th></tr></thead>")
    out.append("<tbody>")
    for f in features:
        slug = f.file.replace(".feature", "")
        count = sum(len(sec.scenarios) for sec in f.sections)
        out.append(f"<tr><td><a href='{slug}.html'><code>{e(f.file)}</code></a></td><td>{e(f.name)}</td><td>{count}</td></tr>")
    out.append("</tbody></table>")

    # The tag legend, setup instructions and recording-results guidance are
    # not duplicated here by hand: they are rendered from features/README.md
    # itself a few paragraphs down (so that editing README.md is the only
    # thing anyone ever needs to do to keep this page in sync). These are
    # just quick jump links into that rendering.
    out.append("<h2>Quick links</h2>")
    out.append(
        "<ul>"
        "<li><a href='#tags'>Tag legend</a> — what each <code>@tag</code> means and what "
        "equipment it requires</li>"
        "<li><a href='#setting-up'>Setting up</a> — throwaway profiles, MIDI monitoring, locales</li>"
        "<li><a href='#recording-results'>Recording results</a></li>"
        "<li><a href='#when-a-scenario-fails'>When a scenario fails</a></li>"
        "</ul>"
    )

    out.append("<h2>About this plan</h2>")
    out.append(
        "<p>The full text below is rendered directly from <code>features/README.md</code>, "
        "which covers setup (throwaway profiles, MIDI monitoring, locales), the tag legend, "
        "how to record results, and how to triage a failing scenario.</p>"
    )
    # README.md's own "# ..." top heading becomes an <h2> here (offset +1), its "##"
    # become <h3>, and so on, so the whole document stays inside this page's h1.
    out.append(markdown_to_html(readme_text, heading_offset=1))

    return page_shell("Overview", "index", "\n".join(out))


def render_coverage_page(coverage_text: str) -> str:
    # COVERAGE.md's own leading "# ..." heading becomes this page's own h1
    # (offset 0), so no separate title is added above it.
    out = [markdown_to_html(coverage_text, heading_offset=0)]
    return page_shell("Coverage", "coverage", "\n".join(out))


CSS = """
:root {
  --bg: #ffffff;
  --fg: #101418;
  --muted: #45525e;
  --link: #0b4f8a;
  --link-visited: #5a3a91;
  --border: #7c8896;
  --accent-bg: #eef3f7;
  --focus: #b5480a;
  --pass: #0b6b2f;
  --fail: #a3140c;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #12161a;
    --fg: #eef1f4;
    --muted: #c3ccd4;
    --link: #7bb7ff;
    --link-visited: #c9a6f5;
    --border: #6d7a87;
    --accent-bg: #1d242b;
    --focus: #ffb066;
    --pass: #5fd88a;
    --fail: #ff8f86;
  }
}
* { box-sizing: border-box; }
html { font-size: 100%; }
body {
  background: var(--bg);
  color: var(--fg);
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  line-height: 1.6;
  font-size: 1.125rem;
  margin: 0;
  padding: 0 1rem 3rem;
  max-width: 62rem;
  margin-inline: auto;
}
h1, h2, h3, h4, h5 { line-height: 1.25; }
h1 { font-size: 1.9rem; margin-top: 1.5rem; }
h2 { font-size: 1.5rem; margin-top: 2.5rem; border-bottom: 1px solid var(--border); padding-bottom: 0.25rem; }
h3 { font-size: 1.25rem; margin-top: 2rem; }
h4, h5 { font-size: 1.05rem; }
a { color: var(--link); }
a:visited { color: var(--link-visited); }
a:focus, input:focus, textarea:focus, button:focus {
  outline: 3px solid var(--focus);
  outline-offset: 2px;
}
.skip-link {
  position: absolute;
  left: -999px;
  top: 0;
  background: var(--accent-bg);
  color: var(--fg);
  padding: 0.75rem 1rem;
  z-index: 10;
  border: 2px solid var(--fg);
}
.skip-link:focus {
  left: 0.5rem;
  top: 0.5rem;
}
header { border-bottom: 3px solid var(--border); padding-bottom: 0.5rem; margin-bottom: 1rem; }
.site-title { font-size: 1.1rem; font-weight: bold; margin: 0.5rem 0; }
header nav ul {
  list-style: none;
  display: flex;
  flex-wrap: wrap;
  gap: 0.25rem 1rem;
  padding: 0;
  margin: 0.5rem 0;
  font-size: 0.95rem;
}
header nav a[aria-current="page"] { font-weight: bold; text-decoration-thickness: 3px; }
.visually-hidden {
  position: absolute;
  width: 1px; height: 1px;
  overflow: hidden;
  clip: rect(0 0 0 0);
  white-space: nowrap;
}
table { border-collapse: collapse; margin: 1rem 0; width: 100%; }
caption { text-align: left; font-weight: bold; padding-bottom: 0.35rem; }
th, td { border: 1px solid var(--border); padding: 0.4rem 0.6rem; text-align: left; vertical-align: top; }
thead th { background: var(--accent-bg); }
.tags { list-style: none; display: flex; flex-wrap: wrap; gap: 0.4rem; padding: 0; margin: 0.5rem 0; }
.tags .tag {
  border: 1px solid var(--border);
  background: var(--accent-bg);
  border-radius: 0.3rem;
  padding: 0.1rem 0.5rem;
  font-family: ui-monospace, Consolas, monospace;
  font-size: 0.9rem;
}
.scenario { border-top: 3px double var(--border); padding-top: 1rem; margin-top: 2rem; }
ol.steps { padding-left: 1.5rem; }
ol.steps > li { margin: 0.4rem 0; }
ol.steps .keyword { font-weight: bold; text-transform: uppercase; font-size: 0.85em; color: var(--muted); }
ol.data-list { margin: 0.4rem 0 0.4rem 1.5rem; }
.step-note, .step-phase {
  background: var(--accent-bg);
  border-left: 4px solid var(--border);
  padding: 0.4rem 0.75rem;
  margin: 0.6rem 0;
}
.step-phase { font-weight: bold; border-left-color: var(--link); }
.table-caption { font-weight: bold; margin-bottom: 0.25rem; }
fieldset.result {
  border: 2px solid var(--border);
  border-radius: 0.4rem;
  padding: 0.75rem 1rem 1rem;
  margin-top: 1.25rem;
  background: var(--accent-bg);
}
fieldset.result legend { font-weight: bold; padding: 0 0.4rem; }
.radio-option { display: inline-flex; align-items: center; gap: 0.3rem; margin: 0.3rem 1.2rem 0.3rem 0; }
.radio-option input[type="radio"] { width: 1.2em; height: 1.2em; }
.notes-field { margin-top: 0.75rem; display: flex; flex-direction: column; gap: 0.25rem; max-width: 40rem; }
.notes-field textarea { font: inherit; padding: 0.4rem; }
footer { margin-top: 3rem; border-top: 1px solid var(--border); padding-top: 1rem; font-size: 0.9rem; color: var(--muted); }
ul.prev-next { list-style: none; display: flex; justify-content: space-between; flex-wrap: wrap; gap: 0.5rem; padding: 0; margin-top: 2rem; border-top: 1px solid var(--border); padding-top: 1rem; }
.filter-box { margin: 1rem 0; padding: 0.75rem; border: 1px solid var(--border); border-radius: 0.4rem; background: var(--accent-bg); }
.filter-box label { font-weight: bold; display: block; margin-bottom: 0.3rem; }
.filter-box input[type="text"] { font: inherit; padding: 0.3rem 0.5rem; width: 100%; max-width: 24rem; }
.filter-status { font-size: 0.9rem; color: var(--muted); margin-top: 0.4rem; }

@media print {
  header nav, .skip-link, .filter-box, footer { display: none; }
  fieldset.result { border: 1px solid #000; background: none; }
  a[href]::after { content: " (" attr(href) ")"; font-size: 0.75em; }
}
"""

JS = """
(function () {
  "use strict";
  var PREFIX = "mixxx-a11y-testplan:";

  function restore() {
    document.querySelectorAll("[data-persist-key]").forEach(function (el) {
      var key = PREFIX + el.getAttribute("data-persist-key");
      var stored = localStorage.getItem(key);
      if (stored === null) return;
      if (el.type === "radio") {
        if (el.value === stored) el.checked = true;
      } else {
        el.value = stored;
      }
    });
  }

  function persistOne(el) {
    var key = PREFIX + el.getAttribute("data-persist-key");
    if (el.type === "radio") {
      if (el.checked) localStorage.setItem(key, el.value);
    } else {
      localStorage.setItem(key, el.value);
    }
  }

  function wire() {
    document.querySelectorAll("[data-persist-key]").forEach(function (el) {
      var evt = el.tagName === "TEXTAREA" ? "input" : "change";
      el.addEventListener(evt, function () { persistOne(el); });
    });
  }

  function addFilter() {
    var scenarios = document.querySelectorAll(".scenario");
    if (scenarios.length < 2) return;
    var container = document.querySelector("nav[aria-label='Scenarios in this file']");
    if (!container) return;
    var box = document.createElement("div");
    box.className = "filter-box";
    box.innerHTML =
      '<label for="tag-filter">Filter scenarios by tag (e.g. "@blocking"). Leave blank to show all.</label>' +
      '<input type="text" id="tag-filter" autocomplete="off">' +
      '<p class="filter-status" id="filter-status" role="status"></p>';
    container.parentNode.insertBefore(box, container.nextSibling);
    var input = box.querySelector("#tag-filter");
    var status = box.querySelector("#filter-status");
    input.addEventListener("input", function () {
      var q = input.value.trim().toLowerCase();
      var shown = 0;
      scenarios.forEach(function (el) {
        var tags = (el.getAttribute("data-tags") || "").toLowerCase();
        var match = q === "" || tags.indexOf(q) !== -1;
        el.hidden = !match;
        if (match) shown++;
      });
      status.textContent = q === "" ? "" : (shown + " of " + scenarios.length + " scenarios match.");
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    restore();
    wire();
    addFilter();
  });
})();
"""


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------


def verify(out_dir: Path, features: list[Feature]) -> list[str]:
    from html.parser import HTMLParser

    problems: list[str] = []

    class Checker(HTMLParser):
        VOID = {
            "area", "base", "br", "col", "embed", "hr", "img", "input",
            "link", "meta", "param", "source", "track", "wbr",
        }

        def __init__(self):
            super().__init__()
            self.stack = []
            self.label_fors = []
            self.ids = []
            self.control_ids = []
            self.headings = []

        def handle_starttag(self, tag, attrs):
            d = dict(attrs)
            if "id" in d:
                self.ids.append(d["id"])
            if tag in ("input", "textarea", "select") and "id" in d:
                self.control_ids.append(d["id"])
            if tag == "label" and "for" in d:
                self.label_fors.append(d["for"])
            if re.match(r"^h[1-6]$", tag):
                self.headings.append(int(tag[1]))
            if tag not in self.VOID:
                self.stack.append(tag)

        def handle_endtag(self, tag):
            if tag in self.VOID:
                return
            if not self.stack or self.stack[-1] != tag:
                problems.append(f"{fname}: mismatched close tag </{tag}>, stack was {self.stack}")
                return
            self.stack.pop()

    for html_path in sorted(out_dir.glob("*.html")):
        fname = html_path.name
        text = html_path.read_text(encoding="utf-8")
        checker = Checker()
        checker.feed(text)
        checker.close()
        if checker.stack:
            problems.append(f"{fname}: unclosed tags at EOF: {checker.stack}")

        ids = checker.ids
        id_counts = {}
        for i in ids:
            id_counts[i] = id_counts.get(i, 0) + 1
        dupes = [i for i, c in id_counts.items() if c > 1]
        if dupes:
            problems.append(f"{fname}: duplicate id attributes: {dupes[:10]}")

        idset = set(ids)
        label_for_set = set(checker.label_fors)
        for f in checker.label_fors:
            if f not in idset:
                problems.append(f"{fname}: <label for='{f}'> has no matching id")
        # Reverse direction: every real form control (input/textarea/select)
        # must itself be the target of some <label for="...">, per the
        # requirement that every control has a real, associated visible
        # label -- not just a placeholder or aria-label doing the work alone.
        for cid in checker.control_ids:
            if cid not in label_for_set:
                problems.append(f"{fname}: form control id='{cid}' has no <label for='{cid}'>")

        prev = 0
        for lvl in checker.headings:
            if prev != 0 and lvl > prev + 1:
                problems.append(f"{fname}: heading level jumps from h{prev} to h{lvl}")
            prev = lvl
        if checker.headings and checker.headings[0] != 1:
            problems.append(f"{fname}: first heading on the page is h{checker.headings[0]}, not h1")
        if checker.headings.count(1) != 1:
            problems.append(f"{fname}: page has {checker.headings.count(1)} h1 elements, expected exactly 1")

    # Scenario count check.
    src_total = 0
    for feat_path in sorted(FEATURES_DIR.glob("*.feature")):
        text = feat_path.read_text(encoding="utf-8")
        src_total += len(re.findall(r"^\s*Scenario(?: Outline)?:", text, re.M))
    rendered_total = sum(sum(len(sec.scenarios) for sec in f.sections) for f in features)
    if src_total != rendered_total:
        problems.append(f"scenario count mismatch: source has {src_total}, rendered {rendered_total}")
    else:
        print(f"OK: scenario count matches source and rendering: {src_total}")

    return problems


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    features = []
    for fname in PAGE_ORDER:
        path = FEATURES_DIR / fname
        features.append(parse_feature_file(path))

    (OUT_DIR / "style.css").write_text(CSS, encoding="utf-8")
    (OUT_DIR / "progress.js").write_text(JS, encoding="utf-8")

    for idx, feature in enumerate(features):
        slug = feature.file.replace(".feature", "")
        prev_slug = features[idx - 1].file.replace(".feature", "") if idx > 0 else None
        next_slug = features[idx + 1].file.replace(".feature", "") if idx < len(features) - 1 else None
        html_out = render_feature_page(feature, prev_slug, next_slug)
        (OUT_DIR / f"{slug}.html").write_text(html_out, encoding="utf-8")

    readme_text = (FEATURES_DIR / "README.md").read_text(encoding="utf-8")
    coverage_text = (FEATURES_DIR / "COVERAGE.md").read_text(encoding="utf-8")
    (OUT_DIR / "index.html").write_text(render_index_page(features, readme_text), encoding="utf-8")
    (OUT_DIR / "coverage.html").write_text(render_coverage_page(coverage_text), encoding="utf-8")

    total = sum(sum(len(sec.scenarios) for sec in f.sections) for f in features)
    print(f"Wrote {len(features)} feature pages + index.html + coverage.html to {OUT_DIR}")
    print(f"Total scenarios rendered: {total}")

    problems = verify(OUT_DIR, features)
    if problems:
        print("\nVERIFICATION PROBLEMS:")
        for p in problems:
            print(" -", p)
        return 1
    print("\nVerification passed: well-formed HTML, no heading-level skips, all label/id pairs match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
