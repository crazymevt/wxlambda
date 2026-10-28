# Implementation plan

Target a usable first release with the existing controls, reliable REPL-driven
development, validated platforms, and standalone packaging. Full wxWidgets
coverage remains future work. This is a plan; unchecked steps are not claims of
implemented or verified behavior.

## Phase 1: Establish the release baseline

- [ ] Correct README coverage and CI counts to 19 scenarios.
- [ ] Record tested and intended OS/Lisp combinations separately.
- [ ] Run the pinned-dependency suite and cache-switch regression checks.

**Acceptance:** Documented commands reproduce the baseline without unexplained
failures.

## Phase 2: Finish REPL-driven development

- [ ] Add delayed-input tests for whitespace, CRLF, EOF, error recovery, and GUI
  responsiveness.
- [ ] Replace message-only error handling for interactive use with a debugger
  policy. Preserve the original condition and its available restarts; provide a
  restart that abandons the current form and returns to the application REPL.
  Investigate a backtrace fallback where debugger integration is unavailable.
- [ ] Define GUI processing as paused while the debugger is active, resuming
  after recovery. Never unwind through a foreign callback.
- [ ] Test recovery from evaluation errors and callback errors separately.
- [ ] Prototype SBCL Swank with `:communication-style nil` and `:fd-handler`,
  checking where forms actually execute. These are candidates requiring
  verification, not supported configurations yet.
- [ ] Prototype CCL integration using its initial-process adapter.
- [ ] Review the prototypes and select a demonstrated editor workflow or
  explicitly defer editor support. If none works reliably, use the documented
  terminal REPL workflow as the release criterion and keep editor REPLs
  unsupported.
- [ ] If scheduling is needed, define execution order, returned values, condition
  delivery, shutdown, and GUI-thread reentrancy before implementing it.
- [ ] Implement and verify the selected editor adapter if a prototype succeeds.
- [ ] Retain and prominently document one GUI session per process for the first
  release.
- [ ] Retain and prominently document the current rule that handlers for
  destroyed controls remain rooted until unbound or session cleanup.
- [ ] Demonstrate explicit unbinding before destruction and verify that repeated
  bind/unbind cycles release registered closures.
- [ ] Add a live-development example that updates controls and redefines callback
  functions within one session.

**Acceptance:** The terminal workflow supports live updates, useful error
diagnosis, and recovery without losing the session. Debugger behavior and
lifecycle limits are documented and tested. Editor support is either demonstrated
or explicitly deferred.

## Phase 3: Validate supported platforms

- [ ] Execute macOS and Linux hosted jobs and preserve results.
- [ ] Extend Windows checks to Lisp loading, callbacks, controls, and cleanup.
- [ ] Check Unicode and REPL behavior on advertised combinations.

**Acceptance:** Runtime evidence supports every advertised platform. Missing
repository access or runners remain explicit external dependencies.

## Phase 4: Complete a representative application

- [ ] Extend an example using the existing controls and layouts.
- [ ] Include handler replacement, explicit unbinding, and window recreation
  within the same session.
- [ ] Identify concrete gaps exposed by the example and add controls or
  operations only where needed.

**Acceptance:** Ordinary startup and live development work through public APIs,
with the accepted lifecycle limits visible.

## Phase 5: Implement standalone packaging

- [ ] Create a reproducible SBCL packaging example.
- [ ] Resolve native libraries relative to the packaged application.
- [ ] Stage required runtime libraries.
- [ ] Test outside the checkout, from another working directory and without a
  development Quicklisp installation.

**Acceptance:** The sample launches, processes events, and exits without relying
on development paths or Quicklisp.

## Phase 6: Prepare the release

- [ ] Review exports, documentation, examples, ABI compatibility, and limitations.
- [ ] Run supported-platform and packaged-application checks.
- [ ] Align ASDF and CMake release versions.
- [ ] Prepare release notes and installation, development, debugging, and
  packaging instructions.

**Acceptance:** A new user can follow the documented installation-to-packaging
workflow.

## Decisions

- D1 Target a bounded first release because full wxWidgets coverage is open-ended;
  rejected: exhaustive bindings as a release requirement.
- D2 Gate editor support on prototype evidence because main-thread integration
  remains unproven; rejected: making release completion depend indefinitely on
  SLIME/SLY.
- D3 Accept both existing lifecycle limits for the first release because restart
  support and destruction notifications require separate lifecycle designs;
  rejected: silently expanding their scope.
- D4 Pause GUI processing during debugging because nested event dispatch adds
  reentrancy risks; rejected: implicitly pumping events inside the debugger.

Implement these as separate reviewable changes, beginning with baseline
corrections and REPL regression tests. Platform work can proceed alongside local
development when runners become available.
