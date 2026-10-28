#!/usr/bin/env python3
"""Rebuild the documentation-only wxWidgets 3.2.12 declaration inventory.

Usage: python3 tools/coverage_inventory.py /path/to/interface
The directory must contain wx/*.h from the pinned upstream release.
No preprocessor branches are evaluated: platform/optional APIs stay visible.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

VERSION = '3.2.12'
ROOT = Path(__file__).resolve().parents[1]
PHASES = {
    1: 'Foundation, values, application, events, and base utilities',
    2: 'Windows, layout, basic controls, and input',
    3: 'Dialogs, navigation, menus, and application services',
    4: 'Models, complex controls, and document frameworks',
    5: 'Drawing, images, graphics, and printing',
    6: 'Optional libraries, rich content, and integration',
    7: 'Platform extensions and remaining portability APIs',
}
GROUPS = {
    2: '''window nonownedwnd toplevel frame panel control ctrlsub button commandlinkbutton anybutton bmpbuttn checkbox choice stattext textctrl textentry textcompleter textwrapper combobox combo odcombo bmpcbox radiobox radiobut tglbtn slider gauge spinbutt spinctrl scrolbar scrolwin vscroll vlbox listbox checklst statbmp statbox statline activityindicator bannerwindow infobar hyperlink srchctrl collpane collheaderctrl addremovectrl editlbox rearrangectrl pickerbase clrpicker filepicker fontpicker datectrl dateevt timectrl calctrl sizer gbsizer wrapsizer layout laywin splitter sashwin caret cursor accel kbdstate mousestate mousemanager tooltip tipwin validator validate valgen valnum valtext containr custombgwin systhemectrl withimages wupdlock windowid windowptr''',
    3: '''dialog msgdlg richmsgdlg textdlg numdlg choicdlg filedlg filedlgcustomize dirdlg colordlg colourdata fontdlg fontdata fdrepdlg progdlg busyinfo aboutdlg preferences propdlg wizard tipdlg creddlg modalhook menu menuitem toolbar statusbr notebook bookctrl choicebk listbook toolbook treebook simplebook popupwin minifram mdi taskbar taskbarbutton notifmsg splash settings display vidmode uiaction access clipbrd dataobj dnd dragimag stockitem artprov filectrl dirctrl filehistory help cshelp''',
    4: '''listctrl treectrl treebase treelist dataview grid headerctrl headercol itemattr imaglist docview docmdi cmdproc persist''',
    5: '''affinematrix2d affinematrix2dbase bitmap bmpbndl brush colour cmndata dc dcbuffer dcclient dcgraph dcmemory dcmirror dcprint dcps dcscreen dcsvg font fontenum fontmap fontutil gdicmn gdiobj geometry graphics icon iconbndl iconloc image anidecod animate animdecod gifdecod imaggif imagiff imagjpeg imagpcx imagpng imagpnm imagtga imagtiff imagxpm metafile overlay palette pen print printdlg quantize rawbmp region renderer''',
    6: '''glcanvas mediactrl sound webview webviewarchivehandler webviewfshandler htmllbox richtooltip''',
    7: '''nativewin dde dialup joystick power''',
}

def phase_for(path):
    rel = path.as_posix()
    if rel.startswith('wx/msw/'):
        return 7
    if any(rel.startswith('wx/' + d + '/') for d in ('aui', 'html', 'propgrid', 'ribbon', 'richtext', 'stc', 'xml', 'xrc')):
        return 6
    if rel.startswith('wx/persist/'):
        return 4
    for number, stems in GROUPS.items():
        if path.stem in stems.split():
            return number
    return 1

# Keep strings intact while removing comments. Preserve newlines for source links.
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)

def uncomment(text):
    return TOKENS.sub(lambda m: ('\n' * m[0].count('\n') or ' ') if m[0].startswith(('/', '/*')) else m[0], text)

def declarations(text):
    """Lossless non-comment source chunks, not a C++ semantic parser.

    Preserve all declarations, enum values, macros and conditional branches;
    opening/closing braces are boundaries, so inline definitions are retained too.
    Supporting tokens deliberately remain rather than risk silently dropping APIs.
    """
    clean = uncomment(text)
    chunks = []
    buffer = []
    start = None
    parens = 0
    quote = None
    escaped = False
    directive, directive_start = [], None
    for lineno, line in enumerate(clean.splitlines(), 1):
        if directive_start is not None or line.lstrip().startswith('#'):
            if directive_start is None:
                if buffer and ''.join(buffer).strip():
                    chunks.append((start, ' '.join(''.join(buffer).split())))
                buffer, start = [], None
                directive_start = lineno
            directive.append(line.strip())
            if not line.rstrip().endswith('\\'):
                chunks.append((directive_start, ' '.join(directive)))
                directive, directive_start = [], None
            # A continuation belongs to the directive, not C++ parenthesis depth.
            continue
        # Access labels qualify the following declarations but aren't APIs.
        line = re.sub(r'^\s*(public|protected|private)\s*:\s*', '', line)
        for ch in line + '\n':
            if start is None and not ch.isspace():
                start = lineno
            buffer.append(ch)
            if quote:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == quote:
                    quote = None
                continue
            if ch in ('"', "'"):
                quote = ch
            elif ch == '(':
                parens += 1
            elif ch == ')':
                parens -= 1
            elif ch in '{};' and parens == 0:
                value = ' '.join(''.join(buffer).split())
                if value not in ('', '}', ';', '};', '{'):
                    chunks.append((start, value))
                buffer, start = [], None
    if directive:
        chunks.append((directive_start, ' '.join(directive)))
    value = ' '.join(''.join(buffer).split())
    if value:
        chunks.append((start, value))
    return chunks


def is_context(decl, guards):
    """Retain compiler scaffolding without making it binding work."""
    if decl.startswith('#'):
        define = re.match(r'#\s*define\s+(\w+)', decl)
        return define is None or define[1] in guards
    return bool(re.match(r'(?:return\b|if\s*\(|else\b|for\s*\(|while\s*\(|namespace\b)', decl))


def annotation(status):
    names = ', '.join('`' + name + '`' for name in status['lisp'])
    native = ', '.join('`' + name + '`' for name in status['native'])
    label = 'Partial' if status['status'] == '~' else 'Implemented'
    return f" — **{label}**: {names}; ABI: {native}. {status['scope']}"


def load_status():
    status = json.loads((ROOT / 'docs/coverage/status.json').read_text())
    if status['version'] != VERSION:
        raise ValueError('Coverage status version differs from inventory version')
    entries = {}
    exports = set(re.findall(r'#:([\w-]+)', (ROOT / 'lisp/package.lisp').read_text().split('(:export', 1)[1]))
    ffi = (ROOT / 'lisp/ffi.lisp').read_text()
    bridge = (ROOT / 'native/wxlambda.cpp').read_text()
    for record in [*status['declarations'], *status['classes'].values()]:
        if record['status'] not in ('x', '~') or not record['scope']:
            raise ValueError('Status must be x/~ with a supported scope')
        names = {match[1] if (match := re.fullmatch(r'\(setf ([\w-]+)\)', name)) else name
                 for name in record['lisp']}
        if not record['lisp'] or not names <= exports:
            raise ValueError(f"Unknown Lisp exports: {record['lisp']}")
        if not record['native'] or any(f'"{name}"' not in ffi or not re.search(r'\b' + re.escape(name) + r'\s*\(', bridge) for name in record['native']):
            raise ValueError(f"Unknown native/FFI mappings: {record['native']}")
    for record in status['declarations']:
        key = (record['header'], record['line'], record['declaration'])
        if key in entries:
            raise ValueError(f'Duplicate status mapping: {key}')
        entries[key] = record
    return entries, status['classes']


def coverage_record(relative, line, declaration, statuses, class_statuses):
    """Use the same effective status for rendered checkboxes and gap counts."""
    match = re.match(r'(?:class|struct)\s+(?:WXDLL\w*\s+)?(wx\w+)\b', declaration)
    if match and '{' in declaration and match[1] in class_statuses:
        return class_statuses[match[1]]
    return statuses.get((relative.as_posix(), line, declaration))


def coverage_counts(relative, entries, guards, statuses, class_statuses):
    counts = Counter({'x': 0, '~': 0, ' ': 0, 'context': 0})
    for line, declaration in entries:
        record = coverage_record(relative, line, declaration, statuses, class_statuses)
        mark = 'context' if is_context(declaration, guards) else record['status'] if record else ' '
        counts[mark] += 1
    return counts


def count_description(counts):
    return (f"{counts['x']} implemented; {counts['~']} partial; {counts[' ']} pending; "
            f"{counts['context']} context entries. Native gaps stay visible; feature priority is separate.")


def build(source):
    headers = sorted(source.glob('wx/**/*.h'))
    if len(headers) != 398:
        raise SystemExit(f'Expected 398 v{VERSION} interface headers, got {len(headers)}')
    output = ROOT / 'docs/coverage'
    output.mkdir(parents=True, exist_ok=True)
    # Reject overlapping assignments instead of silently choosing the first.
    assigned = [stem for stems in GROUPS.values() for stem in stems.split()]
    if len(assigned) != len(set(assigned)):
        raise ValueError('A header stem belongs to multiple phases')
    statuses, class_statuses = load_status()
    matched_statuses = set()
    groups = {n: [] for n in PHASES}
    manifest = []
    classes = []
    for header in headers:
        relative = header.relative_to(source)
        data = header.read_bytes()
        text = data.decode('utf-8-sig')
        entries = declarations(text)
        # Some event/style identifiers exist only in Doxygen tables, not code.
        for match in re.finditer(r'@(event|style)\{([^}]+)\}', text):
            entries.append((text.count('\n', 0, match.start()) + 1,
                            'documented ' + match[1] + ': ' + ' '.join(match[2].split())))
        entries.sort(key=lambda entry: entry[0])
        number = phase_for(relative)
        guards = set(re.findall(r'^\s*#\s*ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b', uncomment(text), re.M))
        groups[number].append((relative, entries, guards))
        context_count = sum(is_context(decl, guards) for _, decl in entries)
        manifest.append({'header': relative.as_posix(), 'sha256': hashlib.sha256(data).hexdigest(), 'phase': number, 'entries': len(entries), 'tasks': len(entries) - context_count, 'context_entries': context_count})
        for line, decl in entries:
            key = (relative.as_posix(), line, decl)
            if key in statuses:
                if is_context(decl, guards):
                    raise ValueError(f'Status maps to context rather than API: {key}')
                matched_statuses.add(key)
        for line, decl in entries:
            match = re.match(r'(?:template\s*<.*?>\s*)?(?:class|struct)\s+(?:WXDLL\w*\s+)?(wx\w+)\b', decl)
            if match and '{' in decl:
                classes.append((number, match[1], relative, line))
    if matched_statuses != set(statuses):
        raise ValueError(f'Stale declaration status mappings: {set(statuses) - matched_statuses}')
    unknown_classes = set(class_statuses) - {name for _, name, _, _ in classes}
    if unknown_classes:
        raise ValueError(f'Stale class status mappings: {unknown_classes}')
    phase_counts = {}
    for number, title in PHASES.items():
        lines = [f'# Inventory category C{number}: {title}', '', '[Plan and status rules](../WXWIDGETS-COVERAGE.md). Generated by', '`tools/coverage_inventory.py`; source: wxWidgets v3.2.12 `interface/`.', 'Upstream declarations: [wxWindows Library Licence](UPSTREAM-LICENCE.txt).', '', 'Status is loaded from [status.json](status.json): `[x]` implemented,', '`[~]` partial with supported scope, `[ ]` pending. Compiler scaffolding and', 'inline implementation statements appear as **Context**, without task boxes.', 'Overloads, constants, macros, inline definitions and all conditional branches', 'are retained. This is a conservative source inventory, not a semantic C++ parser.', 'The goal is broad GUI coverage with selective depth. Categories are not a delivery sequence.', 'Native pending/partial entries are not automatic release blockers; feature priorities', 'are defined in the plan. Full-header coverage is claimed only for reviewed native scope.', 'Inherited declarations name the supported receiver types explicitly; a partial', 'class or constructor does not imply full upstream coverage.', '']
        counts = Counter()
        for relative, entries, guards in groups[number]:
            counts.update(coverage_counts(relative, entries, guards, statuses, class_statuses))
        phase_counts[number] = counts
        lines.extend(['## Current coverage and remaining work', '', count_description(counts), '',
                      'Counts describe inventory entries, not distinct APIs or percent completion.',
                      'See the [coverage summary](README.md) for phase/header comparisons.', ''])
        for relative, entries, guards in groups[number]:
            url = f'https://github.com/wxWidgets/wxWidgets/blob/v{VERSION}/interface/{relative}'
            counts = coverage_counts(relative, entries, guards, statuses, class_statuses)
            lines.extend([f'## `{relative}`', '', f'[Upstream source]({url})', '', count_description(counts), ''])
            for line, decl in entries:
                code = decl.replace('`', '&#96;')
                record = coverage_record(relative, line, decl, statuses, class_statuses)
                mark = record['status'] if record else ' '
                prefix = '**Context**:' if is_context(decl, guards) else f'[{mark}]'
                suffix = annotation(record) if record else ''
                lines.append(f'- {prefix} [L{line}]({url}#L{line}) `{code}`{suffix}')
            lines.append('')
        (output / f'phase-c{number}.md').write_text('\n'.join(lines))
    lines = ['# Class and widget index', '', '[Plan and status rules](../WXWIDGETS-COVERAGE.md). All concrete widgets, abstract', 'bases, helper classes and structs declared in the pinned interface headers are', 'listed together so less familiar widgets are not omitted. `[x]` means full class', 'coverage including inherited APIs; `[~]` means the annotated partial support.', 'Statuses and supported Lisp names are preserved in [status.json](status.json).', 'Forward declarations are represented in the per-header declaration inventories.', 'The goal is broad GUI coverage with selective depth; C1–C7 are inventory categories,', 'not a delivery sequence. This native-status index is not a feature-priority or release-blocker list.', '']
    for number, title in PHASES.items():
        lines.extend([f'## C{number}: {title}', ''])
        counts = Counter(class_statuses.get(name, {}).get('status', ' ')
                         for phase, name, _, _ in classes if phase == number)
        lines.extend([f"Class/struct definitions: {counts['x']} complete; {counts['~']} partial; "
                      f"{counts[' ']} pending. Partial and pending classes remain open.", '',
                      '[Declaration counts and remaining work](README.md).', ''])
        for phase, name, relative, line in classes:
            if phase == number:
                record = class_statuses.get(name)
                mark = record['status'] if record else ' '
                suffix = annotation(record) if record else ''
                lines.append(f'- [{mark}] `{name}` — [`{relative}`](phase-c{number}.md), upstream line {line}.{suffix}')
        lines.append('')
    (output / 'classes.md').write_text('\n'.join(lines))
    lines = ['# wxWidgets coverage summary', '',
             'Generated by `tools/coverage_inventory.py` from wxWidgets 3.2.12 interface headers',
             'and [status.json](status.json). [Coverage goal and status rules](../WXWIDGETS-COVERAGE.md).', '',
             'The goal is broad GUI coverage with selective depth. C1–C7 are inventory categories,',
             'not a delivery sequence. Native pending/partial status records factual gaps,',
             'not automatic release blockers. Core/planned/on-demand/Lisp-native/deferred',
             'feature priorities and representative-app acceptance live in the linked plan.', '',
             'Counts are source-inventory tasks, including declarations, overloads, classes,',
             'styles and event tables; they are not distinct API counts or percent completion.',
             'Context is excluded from tasks. Class-definition counts overlap declaration',
             'tasks and must not be added to them. Reconcile additional native public interfaces',
             'and verify platforms when selected features or completeness claims require it.', '',
             '## Inventory categories', '',
             '| Category | Headers | Tasks | Implemented | Partial | Pending | Context |',
             '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    total = Counter()
    for number, title in PHASES.items():
        c = phase_counts[number]
        total.update(c)
        tasks = c['x'] + c['~'] + c[' ']
        lines.append(f"| [C{number}: {title}](phase-c{number}.md) | {len(groups[number])} | "
                     f"{tasks} | {c['x']} | {c['~']} | {c[' ']} | {c['context']} |")
    class_counts = Counter(class_statuses.get(name, {}).get('status', ' ')
                           for _, name, _, _ in classes)
    lines.extend([f"| Total | {len(headers)} | {total['x'] + total['~'] + total[' ']} | "
                  f"{total['x']} | {total['~']} | {total[' ']} | {total['context']} |", '',
                  '## Classes and widgets', '',
                  f"{len(classes)} class/struct definitions: {class_counts['x']} complete; "
                  f"{class_counts['~']} partial; {class_counts[' ']} pending.", '',
                  'See the [complete class/widget index](classes.md) for partial scopes and missing',
                  'types. An implemented constructor does not complete a class or its inherited API.', '',
                  '## Header backlog', '',
                  'Use each linked header section to see the exact missing and partial declarations.',
                  'A zero pending count alone cannot close a header with partial entries or',
                  'unreviewed inherited/platform behavior.', ''])
    for number, title in PHASES.items():
        lines.extend([f'### C{number}: {title}', '',
                      '| Header | Implemented | Partial | Pending | Context |',
                      '| --- | ---: | ---: | ---: | ---: |'])
        for relative, entries, guards in groups[number]:
            c = coverage_counts(relative, entries, guards, statuses, class_statuses)
            anchor = re.sub(r'[^\w-]', '', relative.as_posix().lower())
            lines.append(f"| [{relative}](phase-c{number}.md#{anchor}) | "
                         f"{c['x']} | {c['~']} | {c[' ']} | {c['context']} |")
        lines.append('')
    lines.extend(['## Regenerate and verify', '',
                  'Use the pinned release interface directory; verify its hashes against',
                  '[manifest.json](manifest.json). Update status.json before regeneration,',
                  'rather than editing generated counts or checkboxes. Example commands:', '',
                  '```sh',
                  'python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface',
                  'python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface',
                  'python3 tools/docset.py build', 'python3 tools/docset.py check', '```', '',
                  'Use the project quiet-command wrapper in agent sessions. See the',
                  '[session handoff](../../IMPLEMENTATION.md) for the next selected GUI feature,',
                  'review/commit workflow and verification limitations.', ''])
    (output / 'README.md').write_text('\n'.join(lines))
    (output / 'manifest.json').write_text(json.dumps({'version': VERSION, 'source': f'https://github.com/wxWidgets/wxWidgets/tree/v{VERSION}/interface', 'headers': manifest, 'class_definitions': len(classes)}, indent=2) + '\n')
    print(f'{len(headers)} headers; {len(classes)} class/struct definitions; {sum(h["entries"] for h in manifest)} source entries')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('interface', type=Path)
    args = parser.parse_args()
    build(args.interface)
