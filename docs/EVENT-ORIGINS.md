# Native event type and origin identity

ABI 30 extends EVENT-NATIVE-TYPE to a generic reader for immutable snapshots and
current innermost contexts. EVENT-ORIGIN-ID is a new generic reader with the same
saved/current lifetime distinction. Baseline: wxWidgets 3.2.12.

EVENT-NATIVE-TYPE returns the actual process-local native type integer from
GetEventType. Compare it with EVENT-TYPE during the active session; registry
names expire at cleanup, while saved integer metadata remains readable. Type
numbers are not stable serialized IDs or authority to cast native objects.
A bounded [registered-tag writer](EVENT-TAGS.md) now exists for native type;
origin now has bounded [clear-only detachment](COMMAND-EVENT-PAYLOADS.md). Changing a type tag does not
change the native C++ event class, so unvalidated type mutation could make native
handlers cast incompatible storage. Unrestricted mutation remains unbound; the registered-tag writer checks both
registry families and actual native class.

EVENT-ORIGIN-ID returns a positive opaque bridge identity if GetEventObject
matches a currently registered application, owned handler or tracked window.
It returns NIL for null or untracked origins; it does not distinguish those two
cases. The bridge compares pointer identities without dereferencing the origin.
This is not a general wxObject wrapper, native pointer, native window ID or
EVENT-ID value. It cannot be passed as a public receiver or used to reconstruct
a wrapper. It transfers no ownership and makes no liveness guarantee. A pending
destruction object can still have a captured origin identity. Lookup reflects
the registry at capture/read time, not a historical reconstruction of arbitrary
native pointer lifetimes.

EVENT-SOURCE remains the Lisp receiver on which the callback was registered.
A child command observed on its parent has the parent as EVENT-SOURCE, but its
native origin identity can still identify the child. The origin ID alone does
not tell you the originating object's class. Synthetic events use their initial
receiving handler as native origin; parent forwarding does not change it.
Each generic and control-specific callback copies origin metadata independently.
Already captured snapshots remain unchanged after later dispatch or source
cleanup. Saved integer type/origin metadata requires no GUI session.

Context reads require an active main-thread session and the innermost callback.
Expired or suspended outer contexts signal NATIVE-ERROR. No native event pointer
escapes. There is no new-origin setter or retention/pinning protocol yet;
using a borrowed object as native origin would require explicit lifetime rules.
Unsupported/untracked objects remain a coverage gap, rather than being silently
wrapped or adopted.

Run the example, which compares child and parent observations, checks current
native type and cleans up its bindings/windows:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-origins.lisp --eval '(wxlambda-event-origins:start)'
```

Regression checks cover all supported kinds, registered command/notify types,
standalone/application origin identity, child/parent receiver distinctions,
context expiration/suspension and saved reads after cleanup. Null/untracked
native-origin lookup is bounded to NIL by implementation; the public synthetic
API always supplies a known receiver and cannot manufacture arbitrary native
origin pointers. Real OS session events are not emitted.

GetEventObject coverage is partial: generic wxObject wrappers, origin retention,
full SetEventObject, arbitrary SetEventType, arbitrary native userData adoption and broader native event
ownership/cloning remain C1 work. Bounded owned-event/cloning, registered tags,
clear-only origins and category metadata are now bound.
Next design typed mutation/ownership contracts that preserve
native storage compatibility and session isolation. Existing read-only type
lookups must not become an unchecked route to native tag mutation.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre lookup and examples.
See [contexts](EVENT-CONTEXTS.md), [ID routing](EVENT-ROUTING.md) and
[docset installation](DOCSET.md).

[Event categories](EVENT-CATEGORIES.md) now provide saved/current native category flags and compiled keyword mask lookup, including the actual native ALL mask. Selective pumping remains unbound.

[Owned events](OWNED-EVENTS.md) now provide detached native construction/cloning,
explicit disposal, mutable value fields and copied dispatch. Earlier immutable
snapshots and scoped contexts keep their lifetime rules. Arbitrary native classes,
origin retention and client/userData ownership remain open.
