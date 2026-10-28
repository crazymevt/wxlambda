#!/usr/bin/env python3
"""Build, check, or install the Cadre-format wxlambda docset.

Uses the shared docset Lisp library. Set WXLAMBDA_DOCSET_SOURCE to its checkout;
the default is ../docset. No GUI or native bridge is loaded by the build.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FILES = ('manifest.sexp', 'index.sexp', 'entries.sexp')


class Keyword(str):
    pass


def key(name):
    return Keyword(':' + name)


def sexp(value):
    if isinstance(value, Keyword):
        return value
    if isinstance(value, str):
        return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
    if value is None:
        return 'nil'
    if isinstance(value, int):
        return str(value)
    return '(' + ' '.join(sexp(item) for item in value) + ')'


def plist(**values):
    return [part for name, value in values.items() for part in (key(name.replace('_', '-')), value)]


def balanced(text, start):
    """Extract an actual Lisp lambda list, including nested defaults and strings."""
    depth, quoted, escaped = 0, False, False
    for pos in range(start, len(text)):
        char = text[pos]
        if quoted:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise ValueError('Unbalanced API lambda list')


def exports():
    return re.findall(r'#:([\w-]+)', (ROOT / 'lisp/package.lisp').read_text(encoding='utf-8').split('(:export', 1)[1])


def inlines(text, source=None, links=None):
    result, start = [], 0
    for match in re.finditer(r'`([^`]+)`|\[([^\]]+)\]\(([^)]+)\)|\*\*([^*]+)\*\*', text):
        if match.start() > start:
            result.append(text[start:match.start()])
        if match[1]:
            result.append([key('code'), match[1]])
        elif match[2]:
            target = match[3]
            if '://' in target or target.startswith('mailto:'):
                result.append([key('link'), target, match[2]])
            elif source is not None and links is not None:
                resolved = (source.parent / target.split('#', 1)[0]).resolve() if not target.startswith('#') else source.resolve()
                if resolved in links:
                    result.append([key('ref'), links[resolved], match[2]])
                else:
                    # Repository source links are retained as paths, not broken
                    # relative URLs in the installed docset.
                    result.append([key('code'), match[2] + ' (' + target + ')'])
            else:
                result.append([key('code'), match[2] + ' (' + target + ')'])
        else:
            result.append([key('b'), match[4]])
        start = match.end()
    if start < len(text):
        result.append(text[start:])
    return result


def markdown(text, source=None, links=None):
    blocks, paragraph, code = [], [], None
    language = None

    def flush():
        if paragraph:
            blocks.append([key('p'), *inlines(' '.join(paragraph), source, links)])
            paragraph.clear()

    for line in text.splitlines():
        if line.startswith('```'):
            flush()
            if code is None:
                code, language = [], line[3:].strip() or None
            else:
                blocks.append([key('code-block'), language, '\n'.join(code)])
                code = None
        elif code is not None:
            code.append(line)
        elif match := re.match(r'^(#{1,6})\s+(.*)', line):
            flush()
            blocks.append([key('heading'), min(len(match[1]), 4), *inlines(match[2], source, links)])
        elif not line.strip():
            flush()
        elif match := re.match(r'^\s*(?:[-*]|\d+\.)\s+(.*)', line):
            flush()
            blocks.append([key('list'), [key('item'), [key('p'), *inlines(match[1], source, links)]]])
        else:
            paragraph.append(line.strip())
    flush()
    if code is not None:
        raise ValueError('Unclosed Markdown code fence')
    return blocks


def sources():
    # Agent instructions stay in the repository, outside the reader-facing guides.
    guides = sorted(p for p in (ROOT / 'docs').glob('*.md')
                    if p.name not in {'AGENTS.md', 'CLAUDE.md'})
    return [ROOT / 'README.md', ROOT / 'IMPLEMENTATION.md', *guides]


def content():
    records = json.loads((ROOT / 'docs/api-reference.json').read_text(encoding='utf-8'))
    names = exports()
    if len(records) != len(names) or {r['name'] for r in records} != set(names):
        raise ValueError('API reference must document every public export exactly once')
    for record in records:
        for field in ('name', 'kind', 'arguments', 'returns', 'description', 'scope', 'wxwidgets'):
            if not isinstance(record.get(field), str) or not record[field]:
                raise ValueError(f'Missing API reference field {field}: {record.get("name")}')
    guides = sources()
    examples = sorted((ROOT / 'examples').glob('*.lisp'))
    notes = json.loads((ROOT / 'docs/example-reference.json').read_text(encoding='utf-8'))
    if set(notes) != {p.name for p in examples}:
        raise ValueError('Example reference must cover every example source exactly once')
    links = {p.resolve(): prefix + '/' + p.relative_to(ROOT).as_posix()
             for prefix, paths in [('guide', guides), ('example', examples)] for p in paths}
    api = (ROOT / 'lisp/api.lisp').read_text(encoding='utf-8') + '\n' + (ROOT / 'lisp/ffi.lisp').read_text(encoding='utf-8')
    signatures = {}
    for match in re.finditer(r'^\(def(?:un|macro)\s+([\w-]+)\s+(?=\()', api, re.M):
        signatures[match[1]] = balanced(api, match.end())
    entries, toc = [], []
    for title, kind in [('API reference', 'api'), ('Guides', 'guide'), ('Examples', 'example')]:
        children = []
        if kind == 'api':
            for record in sorted(records, key=lambda r: r['name']):
                name = record['name']
                signature = signatures.get(name, record.get('syntax'))
                if signature is None:
                    raise ValueError(f'Missing signature for {name}')
                syntax = f'(wxlambda:{name} {signature[1:-1]})' if name in signatures else signature
                sections = [
                    ['Syntax', [key('code-block'), 'lisp', syntax]],
                    ['Arguments', [key('p'), record['arguments']]],
                    ['Returns', [key('p'), record['returns']]],
                    ['Description', [key('p'), record['description']]],
                    ['Errors and scope', [key('p'), record['scope']]],
                    ['wxWidgets', [key('p'), record['wxwidgets']]],
                ]
                entry_id = 'api/' + name
                entries.append(plist(entry=entry_id, names=[name], kind=key(record['kind']),
                                     kind_name=record['kind'].title(), package='WXLAMBDA',
                                     summary=record['description'], sections=sections))
                children.append(plist(title=name, entry=entry_id))
        else:
            paths = guides if kind == 'guide' else examples
            for path in paths:
                relative = path.relative_to(ROOT).as_posix()
                entry_id = kind + '/' + relative
                if kind == 'guide':
                    sections = [['Guide', *markdown(path.read_text(encoding='utf-8'), path, links)]]
                    summary = relative
                else:
                    example_notes = notes[path.name]
                    sections = [['Usage', [key('p'), example_notes['description']],
                                 [key('code-block'), 'sh', example_notes['command']]],
                                ['Source', [key('code-block'), 'lisp', path.read_text(encoding='utf-8')]]]
                    summary = example_notes['description']
                entries.append(plist(entry=entry_id, names=[relative, path.stem], kind=key(kind),
                                     kind_name=kind.title(), summary=summary, sections=sections))
                children.append(plist(title=relative, entry=entry_id))
        toc.append(plist(title=title, children=children))
    manifest = plist(id='wxlambda', title='wxlambda', subtitle='Common Lisp bindings to wxWidgets',
                     version=re.search(r':version\s+"([^"]+)"', (ROOT / 'wxlambda.asd').read_text(encoding='utf-8'))[1], system='wxlambda', packages=['WXLAMBDA'],
                     source='wxlambda source tree', toc=toc)
    return manifest, entries


def run(mode, output, lisp, library):
    if not (library / 'docset.asd').is_file():
        raise ValueError(f'Docset library missing at {library}; set WXLAMBDA_DOCSET_SOURCE')
    manifest, entries = content()
    with tempfile.TemporaryDirectory(prefix='wxlambda-docset-') as temp:
        temp = Path(temp)
        (temp / 'manifest.sexp').write_text(sexp(manifest), encoding='utf-8')
        (temp / 'entries.sexp').write_text(sexp(entries), encoding='utf-8')
        env = {**os.environ, 'WXLAMBDA_DOCSET_SOURCE': str(library),
               'WXLAMBDA_DOCSET_INPUT': str(temp), 'WXLAMBDA_DOCSET_OUTPUT': str(output),
               'WXLAMBDA_DOCSET_MODE': mode}
        subprocess.run([lisp, '--noinform', '--no-sysinit', '--no-userinit', '--non-interactive',
                        '--load', str(ROOT / 'tools/write-docset.lisp')], env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('build', 'check', 'install'))
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/docset')
    parser.add_argument('--library', type=Path, default=Path(os.environ.get('WXLAMBDA_DOCSET_SOURCE', ROOT.parent / 'docset')))
    parser.add_argument('--lisp', default='sbcl', help='SBCL executable (documentation tool only)')
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    run('build' if args.command == 'build' else 'check', output, args.lisp, args.library.resolve())
    if args.command == 'install':
        destination = args.destination or Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'docsets/wxlambda'
        destination = destination.resolve()
        if destination == output:
            raise ValueError('Installation directory must differ from build output')
        destination.mkdir(parents=True, exist_ok=True)
        for name in FILES:
            shutil.copyfile(output / name, destination / name)
        run('check', destination, args.lisp, args.library.resolve())
        print('Installed:', destination)


if __name__ == '__main__':
    main()
