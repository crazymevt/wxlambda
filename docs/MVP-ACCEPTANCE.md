# Finite primary-goal acceptance

The primary goal is a useful Common Lisp GUI foundation with broad everyday
coverage and selective depth. It is not exhaustive wxWidgets parity. This report
sets a finite completion checkpoint so repeated requests for another batch do not
silently become a requirement to bind the entire native inventory.

The declared target is the automated local Apple Silicon macOS MVP using
wxWidgets3.2.12, SBCL and CCL. Completion requires all rows below and the shared
reliability/documentation/package gates. **Primary automated local macOS MVP goal reached on 2026-10-29.** All declared
workflow, reliability, documentation, inventory and package gates passed.

| Workflow | Bounded acceptance |
| --- | --- |
| Form/editor | Persistent document form with cancellation/failed-save close protection, dirty baseline, UTF-8 IO, menus/toolbars/status/shortcuts, deferred dialogs and clipboard adapters; native multiline editing and everyday controls have independent coverage. |
| Tree/list manager | Cohesive record manager: stable IDs, tree category selection, search filtering, list selection, notebook details, shared menu/tool/keyboard CRUD and copy actions, injected deterministic prompt/confirmation/clipboard paths, pending-action disposal and stale-target tests. |
| Drawing/monitoring | Retained geometry/text/font/colour/RGB bitmap rendering, PNG pixel verification, native resize/DIP conversion, activity feedback, real owner-bound timer updates, deferred Pause/Resume and teardown. |
| Reliability | Main-thread/native-entry rules, CLOS construction rollback, checked lifetimes, immutable snapshots/context isolation, error/commit semantics, native zero-assertion harness and both-Lisp full GUI suites. |
| Deliverables | Standalone docs/examples, factual pinned native coverage, all public exports indexed in the Cadre-format docset, zero docset checker/link/discovery/lookup problems and relocated package smoke tests. |

## Boundaries

A completed checkpoint does not claim full native class parity, every desktop
platform, or production validation of every physical interaction. Hosted
Windows/Linux/Qt, native input/IME/accessibility, exhaustive locale/font/DPI
combinations, external clipboard interoperability and graphical Cadre remain
unverified. The existing wxQt tree gate remains in force. Native OS message/file
dialogs, raw ownership adoption and broad virtual overrides remain unbound.

Specialized grid/data-view, rich text, AUI/ribbon, web/media/OpenGL, printing,
additional drawing backends and unneeded overloads are on-demand/deferred under
the [feature policy](WXWIDGETS-COVERAGE.md). They are not automatic blockers for
this checkpoint. The native inventory keeps their true pending/partial states.

The primary automated local macOS MVP goal has been reached. There is no next
mandatory widget batch. Further work should be driven
by a concrete application, a requested platform/manual validation target or an
explicitly chosen extension.

## Evidence

Local final evidence, 2026-10-29:

- ABI55 main/native builds passed; native combined regression reported zero wx
  assertions and checked exact PNG geometry/bitmap pixels (quiet-run.51d4fRNq,
  quiet-run.hDxZV5Dn). Final monitor PNG was visually inspected.
- Full82 GUI scenarios plus preflight passed on SBCL2.6.9 and CCL1.13:
  `evidence/mvp/{sbcl,ccl}-20261029/` (quiet-run.ZeZLFmJK/89VRCUnf).
- All five relocated package isolation checks passed, including new canvas PNG
  and activity smoke checks: `evidence/packaging/mvp-20261029/`
  (quiet-run.3sDyUEjf/tYx5iZ0g).
- The docset contains392 entries:278 public exports,56 examples and58 project
  guides. Build/check reports zero problems; both-Lisp actual class/SETF/export
  coverage and isolated install/discovery/symbol/guide/example reading passed
  (quiet-run.w2lndltK/6KS7vQ0h). Final regenerated inventory is included.
- All16 Python runner/documentation checks passed. Pinned native inventory
  regeneration and all14 consistency checks passed
  (quiet-run.BY4cUhyv/yMEP7cuM).
- Two independent subagents reviewed implementation, workflow, documentation,
  metadata and package integration; actionable findings were resolved.

Normal Cadre installation uses `python3 tools/docset.py install` under
`$XDG_DATA_HOME/docsets/wxlambda/` (default `~/.local/share/docsets/wxlambda/`).
The verified isolated install is under `/private/tmp/wxlambda-mvp-docsets/wxlambda/`.
Graphical Cadre itself was not exercised. See [docset installation](DOCSET.md),
[record manager](RECORD-MANAGER.md), [drawing canvas](DRAWING-CANVAS.md),
[activity indicators](ACTIVITY-INDICATORS.md) and [persistent form](DOCUMENT-FORM.md).
