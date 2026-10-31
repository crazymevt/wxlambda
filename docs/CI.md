# Continuous integration and public repository setup

The GitHub Actions workflow is `.github/workflows/ci.yml`. It runs on pushes,
pull requests and manual dispatch, with read-only repository permissions and
superseded runs cancelled per branch. Action revisions and source dependencies
are pinned. A workflow configuration is not evidence that a remote run passed.

Configure probes explicitly inherit wxWidgets compile definitions, including
the selected port, Unicode and DLL configuration. The Edge probe includes the
public WebView header before its platform header; the GTK probe still executes
the loaded-library ABI check and rejects mixed WebKitGTK generations. Windows
CI selects Release for probes to match its release-only wx build. These fixes
do not enable the unavailable Windows browser contract or bypass either gate.

Experimental GTK configuration exports the same wx library search path as its
runtime tests: the ABI probe must load the just-built wx libraries, not only
link them. Native/Lisp PNG fixtures use platform temporary directories, including
the drawing monitor's default export.

Windows GUI CI sets `WXLAMBDA_TEST_ABORT_ON_ASSERT`. A wx assertion prints its
file/line/function/condition/message and aborts rather than opening an unattended
native assertion dialog. Outside this opt-in test mode, normal wx assertion
handling is unchanged. This is diagnostic, not assertion suppression or a claim
that the Windows event-payload timeout is fixed.

The private native test binary has an isolated assertion fixture, checked with
`python3 tests/assert-diagnostic.py build/wxlambda-client-data-native`. The child
must fail promptly and emit file/line metadata; no assertion is reported as a
successful GUI operation. Experimental Linux CI runs this diagnostic separately
before its ordinary native invariants.

## Jobs and their actual scope

| Job | What it checks |
| --- | --- |
| Documentation/reporting/portability | Python unit tests, committed docset consistency/public exports and GUI-independent ASan/UBSan cancellation tests. Shared docset library is pinned. |
| Experimental Linux WebView | Builds pinned wx3.2.12 against GTK3/WebKitGTK4.1; runs real offline WebView JavaScript, the example exercise, native invariants and portability CTest under Xvfb. |
| General Linux GUI | Builds against distro wx plus its WebView development package; runs the GUI suite and the explicit WebView-unavailable contract, not browser execution. |
| macOS GUI | Builds pinned wx3.2.12 from source, avoiding a moving Homebrew version conflicting with the WKWebView gate; runs the GUI suite including actual WebView execution. |
| Windows x64/MSVC | Stages checksum-pinned SBCL/WebView2 SDK, applies the pinned Edge cancellation prerequisite, builds wx/bridge and verifies the header/binary link marker; runs portability CTest and the GUI suite with WebView explicitly unavailable. |

The Windows SDK is not the WebView2 Runtime, and the prerequisite patch is not
the Windows bridge. The Linux experimental job is the Linux browser execution
gate; a passing distro-Linux unavailable test must not be cited as that evidence.
Neither new platform backend is marked accepted merely because this workflow
exists. See [WebView portability](WEB-VIEW-PORTABILITY.md).

## Evidence and failures

GUI jobs retain test logs/JSON summaries and available CMake/configure/CTest
diagnostics as artifacts for30 days, including on failure. The main runner records
the commit, dirty state, runtime preflight, expected/completed scenario counts,
ordered results, elapsed time and exit status. It stops on failure and refuses
to reuse an existing report directory. Build logs are also available in Actions.

`WXLAMBDA_TEST_WEBVIEW=supported` is the runner default. Setting it explicitly to
`unavailable` selects the named `webview-unavailable` scenario and records that
mode in the summary. That test requires the known construction-gate error; an
unrelated native failure or unexpectedly available backend fails. It never
claims JavaScript execution. Unknown policy values reject rather than skip.

Two Python integration tests optionally consume real pinned source/package
inputs; without those inputs they report explicit skips. The Windows patch check
uses its real pinned source tree. SDK staging itself always verifies the package
checksum. These unit/integration checks do not replace native COM execution.

Additional optional integration inputs are `WXLAMBDA_SBCL_MSI` (the pinned
installer, checked but never executed by Python tests) and
`WXLAMBDA_GENERIC_DIALOG_SOURCE` (pinned wx source for private adapter regeneration).
These report explicit skips when not supplied.

### First-run failures and repair scope

The first completed public run exposed four distinct setup failures, not four
failed GUI assertions:

- Linux distro headers omitted the generic file dialog. The bridge now compiles
  private, symbol-isolated copies of the pinned generic file/directory dialogs;
  it does not fall back to platform-native panels. See
  [the modal contract](MESSAGE-FILE-DIALOGS.md).
- The experimental GTK job passed an invalid configure flag. The pinned script
  accepts `--enable-webviewwebkit`; GTK3 selects WebKit2 internally.
- macOS selected a system TIFF library without usable include flags. Its pinned
  source build now requests bundled TIFF/JPEG/PNG/zlib instead of that detection.
- The Windows SBCL response failed checksum validation. An independent official
  download still matched the original pin. The bounded downloader now tries two
  official hosts, validates both the original SHA256 and compound-file header,
  logs rejected response sizes/hashes, and never publishes rejected payloads as
  an installer. PowerShell independently rechecks the same pin before extraction.
  No checksum was changed or validation disabled. Download diagnostics are
  retained even when extraction never starts.

The repair run passed the distro-Linux bridge build and Windows SBCL/SDK staging.
Windows then exposed a stale-docset test mutation that translated line endings
and invalidated byte offsets. That mutation now uses a same-length binary
replacement, preserving length and CRLF counts, rather than text-mode rewriting.
The actual initial Windows docset build/check stages had already passed.
Remaining remote build/runtime results still require validation; local macOS
results and unit tests do not establish full GTK/MSVC acceptance.

## Local checks and publishing

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
actionlint .github/workflows/ci.yml
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project's quiet-command wrapper for agent-run commands. GUI checks need
a real desktop/Xvfb and the documented dependencies. No automatic clipboard
transfer tests should read or overwrite the user's system clipboard.

Before public publication, inspect the staged tree and history for credentials,
private keys and unintended files. Build products, vendor checkouts, dependency
caches and local evidence directories are ignored, not source deliverables.
This project currently has no selected project-wide licence file; do not infer
an open-source licence from public GitHub visibility or invent one on release.
Bundled upstream inventory licence notices remain included.

Create a new repository only under the agreed owner/name, without overwriting an
existing remote or force-pushing history. After pushing, inspect the actual run:

```sh
gh run list --workflow ci.yml
gh run view RUN_ID
gh run view RUN_ID --log-failed
gh run download RUN_ID
```

Initial remote jobs may reveal platform-specific failures. Keep those failures
visible and fix them; do not silently disable tests or relabel unavailable
behavior as supported browser functionality. Add required status checks only
with an accurate understanding of these distinct scopes.
