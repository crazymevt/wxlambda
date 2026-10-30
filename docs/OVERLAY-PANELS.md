# In-window overlay panels

Create an ordinary PANEL parented to the main content panel, without adding it to a sizer. POSITION-PANEL sets parent-relative geometry, lays out children, and raises it. Reposition after parent resize using deferred UI actions. Bounds use wx coordinates; FROM-DIP converts design dimensions. Closing an overlay destroys its child panel, not the top-level frame.

WINDOW-DESCENDANTS returns borrowed generic WINDOW views of registered children. Observe mouse events on the owner and descendants to dismiss an overlay on outside clicks; exclude the overlay subtree. Unbind observers when closing. It does not include wx implementation-owned children and does not promise EQ identity or order. Native handle validation still rejects expired views.

Only ordinary panels without a containing sizer can be positioned. Both operations require a live GUI session. Geometry writes reject callback and virtual-provider contexts. Native container pane positioning remains the container's responsibility. macOS is locally verified; Windows/Linux execution remains pending.

LIST-ITEM-AT-POINT queries the native list-box row under client coordinates, returning NIL on blank space. Use it with mouse-up metadata to execute only a clicked row.

WINDOW-FOCUS-WITHIN-P tests native focus in a subtree, including native implementation children. Defer a focus-lost check until the new focus target is established to distinguish internal query/list navigation from outside focus.
