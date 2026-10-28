(in-package #:wxlambda)

(define-condition native-error (error)
  ((message :initarg :message :reader native-error-message))
  (:report (lambda (condition stream) (write-string (native-error-message condition) stream))))

(defclass window () ((handle :initarg :handle :reader handle)))
(defclass frame (window) ())
(defclass panel (window) ())
(defclass button (window)
  ((handler-token :initform nil :accessor handler-token)))
(defclass text-control (window)
  ((handler-token :initform nil :accessor handler-token)))

(defclass choice (window)
  ((handler-token :initform nil :accessor handler-token)))

(defclass label (window) ())
(defclass checkbox (window)
  ((handler-token :initform nil :accessor handler-token)))

(defstruct (event (:constructor %make-event (kind source text checked-p selection)))
  "A Lisp-owned snapshot. Slots are read-only; TEXT is an independent string."
  (kind nil :read-only t)
  (source nil :read-only t)
  (text nil :read-only t)
  (checked-p nil :read-only t)
  (selection nil :read-only t))

(defstruct handler source function)
(defvar *active* nil)
(defvar *repl-running* nil)
(defvar *repl-debugging* nil)
(defvar *handlers* nil)
(defvar *callback-error* nil)
(defvar *in-notifying-setter* nil)
(defvar *next-token* 0)

(defun check-status (status)
  (unless (zerop status) (error 'native-error :message (%last-error)))
  t)
(defun wrap-window (class id)
  (when (zerop id) (error 'native-error :message (%last-error)))
  (make-instance class :handle id))

(cffi:defcallback dispatch-event :void ((token :uint64) (payload :pointer))
  ;; Copy borrowed payloads before user code, including reentrant native calls.
  ;; Unwinding through C++ is unsafe. Re-signal after the native call returns.
  (handler-case
      (unless *callback-error*
        (let ((handler (gethash token *handlers*)))
          (when handler
            (when (cffi:null-pointer-p payload) (error "Missing event payload"))
            (cffi:with-foreign-slots ((kind text checked selection) payload (:struct native-event))
              (let* ((event-kind (ecase kind (1 :click) (2 :text-change) (3 :toggle) (4 :selection)))
                     (value (when (= kind 2)
                              (when (cffi:null-pointer-p text)
                                (error "Missing text-change payload"))
                              (cffi:foreign-string-to-lisp text :encoding :utf-8)))
                     (checked-p (and (= kind 3) (not (zerop checked))))
                     (event (%make-event event-kind (handler-source handler) value checked-p
                                         (when (= kind 4) (unless (= selection -1) selection)))))
                (funcall (handler-function handler) event))))))
    (error (condition)
      (unless *callback-error* (setf *callback-error* condition))
      ;; Notifying setters have a nearer Lisp boundary where their caller can
      ;; handle this error. Only errors escaping that boundary stop the loop.
      (unless *in-notifying-setter* (%stop)))))

(defun check-callback-error ()
  (when *callback-error*
    (let ((condition *callback-error*))
      (setf *callback-error* nil)
      (error condition))))

(defun %call-with-application (function)
  "Call FUNCTION in one GUI session on the process main thread.
Run explicitly with RUN. Only one session per process is currently supported."
  (when *active* (error "An application session is already active"))
  (load-native-library)
  (let ((*active* t) (*handlers* (make-hash-table)) (*callback-error* nil))
    (check-status (%start (cffi:callback dispatch-event)))
    (unwind-protect (funcall function)
      (check-status (%finish)))))

(defun call-with-application (function)
  "Run FUNCTION in a main-thread GUI session, masking SBCL floating-point traps
while native GUI code runs. The REPL blocks until FUNCTION returns."
  #+sbcl (sb-int:with-float-traps-masked (:invalid :divide-by-zero :overflow
                                        :underflow :inexact)
           (%call-with-application function))
  #+(and ccl darwin)
  (if (eq ccl:*current-process* ccl::*initial-process*)
      (%call-with-application function)
      (let ((failure nil))
        (multiple-value-prog1
            (ccl::call-in-initial-process
             (lambda ()
               (handler-case (%call-with-application function)
                 (error (condition) (setf failure condition) nil))))
          (when failure (error failure)))))
  #-(or sbcl (and ccl darwin)) (%call-with-application function))

(defmacro with-application (() &body body)
  `(call-with-application (lambda () ,@body)))

(defun run ()
  "Block in the GUI event loop until the last frame closes or STOP is called."
  (when *repl-running*
    (error "RUN is unavailable inside the application REPL; events are already being processed"))
  (check-status (%run))
  (check-callback-error))
(defun stop ()
  "Leave RUN or the application REPL."
  (if *repl-running* (setf *repl-running* nil) (check-status (%stop))))

(defun process-events ()
  "Process pending GUI events on the application thread without blocking."
  (when *repl-debugging*
    (error "GUI event processing is paused in the application REPL debugger"))
  (check-status (%poll))
  (check-callback-error))

(defun repl-input-ready-p (input)
  "Skip available standard whitespace without waiting. Return READY-P, EOF-P."
  (let ((eof (gensym "EOF")))
    (loop for character = (read-char-no-hang input nil eof) do
      (cond ((eq character eof) (return (values nil t)))
            ((null character) (return (values nil nil)))
            ((find character '(#\Space #\Tab #\Newline #\Return #\Page)))
            (t (unread-char character input)
               (return (values t nil)))))))

(defun report-repl-error (condition output)
  "Report before unwinding so the fallback backtrace retains evaluation frames."
  (format output "~&~A: ~A~%" (type-of condition) condition)
  (format output "Backtrace (up to 20 frames):~%")
  (let ((*print-level* 4) (*print-length* 8))
    (unless (ignore-errors
              #+sbcl (sb-debug:print-backtrace :stream output :count 20)
              #+ccl (ccl:print-call-history :stream output :count 20)
              #+(or sbcl ccl) t
              #-(or sbcl ccl) nil)
      (format output "Backtrace unavailable on this Lisp.~%")))
  (finish-output output))

(defun call-with-repl-recovery (function on-error output)
  (restart-case
      (handler-bind
          ((error (lambda (condition)
                    ;; This handler is reached only at a Lisp boundary, after
                    ;; foreign callbacks have returned. Do not pump GUI events
                    ;; during diagnosis, including explicit PROCESS-EVENTS calls.
                    (let ((*repl-debugging* t))
                      (ecase on-error
                        (:debug (invoke-debugger condition))
                        (:report
                         (report-repl-error condition output)
                         (invoke-restart 'return-to-application-repl)))))))
        (funcall function))
    (return-to-application-repl ()
      :report "Abandon the current operation and return to the application REPL."
      nil)))

(defun application-repl (input output on-error)
  (check-type on-error (member :debug :report))
  (let ((*repl-running* t)
        (eof (gensym "EOF")))
    (loop while *repl-running* do
      (call-with-repl-recovery
       (lambda ()
         (process-events)
         (unless *repl-running* (return))
         (multiple-value-bind (ready-p eof-p) (repl-input-ready-p input)
           (when eof-p (return))
           (when ready-p
             (let ((form (read input nil eof)))
               (when (eq form eof) (return))
               (dolist (value (multiple-value-list (eval form)))
                 (prin1 value output)
                 (terpri output))
               (finish-output output)))))
       on-error output)
      (unless *repl-running* (return))
      (sleep 0.01))))

(defmacro with-repl-application ((&key (input '*standard-input*)
                                     (output '*standard-output*)
                                     (on-error :debug)) &body body)
  "Run BODY, then a cooperative terminal REPL in the same GUI session.
Use STOP or EOF to leave. Live objects can be kept in special variables.
Incomplete input, evaluation, and the debugger suspend event processing.
ON-ERROR is :DEBUG (the default), preserving evaluation restarts, or :REPORT,
printing a backtrace and abandoning the operation. The debugger offers the
RETURN-TO-APPLICATION-REPL restart. Setup errors propagate to the caller."
  `(call-with-application
    (lambda ()
      ,@body
      (application-repl ,input ,output ,on-error))))
(defun make-frame (&key (title "wxlambda") (width 480) (height 240))
  (wrap-window 'frame (%frame title width height)))
(defun make-panel (parent) (wrap-window 'panel (%panel (handle parent))))
(defun make-button (parent &key (label "Button"))
  (wrap-window 'button (%button (handle parent) label)))
(defun make-text (parent &key (value ""))
  (wrap-window 'text-control (%text (handle parent) value)))
(defun make-label (parent &key (label ""))
  "Create non-editable static text using wxWidgets label/mnemonic syntax."
  (wrap-window 'label (%label (handle parent) label)))
(defun set-label (label value)
  "Set static LABEL's text. Call LAYOUT-WINDOW on its parent to reflow if needed."
  (check-status (%set-label (handle label) value)) value)
(defun get-label (label)
  "Return an independent string containing static LABEL's raw text."
  (let ((pointer (%get-label (handle label))))
    (when (cffi:null-pointer-p pointer)
      (error 'native-error :message (%last-error)))
    (cffi:foreign-string-to-lisp pointer :encoding :utf-8)))
(defun make-checkbox (parent &key (label "") checked)
  "Create a two-state checkbox. CHECKED is a generalized boolean."
  (wrap-window 'checkbox (%checkbox (handle parent) label (if checked 1 0))))
(defun checked-p (checkbox)
  "Return T or NIL for CHECKBOX's current state."
  (let ((result (%checked (handle checkbox))))
    (when (minusp result) (error 'native-error :message (%last-error)))
    (not (zerop result))))
(defun make-column (parent) (check-status (%column (handle parent))) parent)
(defun make-row (parent) (check-status (%row (handle parent))) parent)
(defun add-child (parent child &key (proportion 0) (border 8) (expand t) (align :start))
  "Add CHILD to PARENT's row/column. PROPORTION allocates the major axis;
EXPAND fills the minor axis. With EXPAND NIL, ALIGN is :START, :CENTER, or :END.
BORDER is nonnegative spacing on all four sides, in wxWidgets coordinates."
  (check-status (%add (handle parent) (handle child) proportion border (if expand 1 0)
                      (ecase align (:start 0) (:center 1) (:end 2))))
  child)
(defun add-spacer (parent size)
  "Add fixed nonnegative SIZE along PARENT's layout axis."
  (check-status (%spacer (handle parent) size)) parent)
(defun add-stretch (parent &key (proportion 1))
  "Add flexible space with positive PROPORTION along PARENT's layout axis."
  (check-status (%stretch (handle parent) proportion)) parent)
(defun layout-window (window)
  "Recompute WINDOW's sizer layout after batching changes."
  (check-status (%layout (handle window))) window)
(defun window-geometry (window)
  (cffi:with-foreign-objects ((x :int) (y :int) (width :int) (height :int))
    (check-status (%geometry (handle window) x y width height))
    (values (cffi:mem-ref x :int) (cffi:mem-ref y :int)
            (cffi:mem-ref width :int) (cffi:mem-ref height :int))))
(defun window-position (window)
  "Return X and Y as two values, relative to the parent (screen for frames)."
  (multiple-value-bind (x y width height) (window-geometry window)
    (declare (ignore width height))
    (values x y)))
(defun window-size (window)
  "Return outer WIDTH and HEIGHT as two values in wxWidgets coordinates."
  (multiple-value-bind (x y width height) (window-geometry window)
    (declare (ignore x y))
    (values width height)))
(defun boolean-result (result)
  (when (minusp result) (error 'native-error :message (%last-error)))
  (not (zerop result)))
(defun show-window (window &optional (show t))
  "Show WINDOW when SHOW is true, hide it when NIL. Return WINDOW and a second
value indicating whether its native shown state changed. SHOWN-P queries its own
shown flag, independently of whether an ancestor is hidden."
  (values window (boolean-result (%show-state (handle window) (if show 1 0)))))
(defun shown-p (window)
  "Return WINDOW's own shown flag, not its effective visibility on screen."
  (boolean-result (%shown (handle window))))
(defun enable-window (window &optional (enable t))
  "Enable WINDOW for input when ENABLE is true, disable it when NIL.
Return WINDOW and whether the native enabled state changed."
  (values window (boolean-result (%enable (handle window) (if enable 1 0)))))
(defun enabled-p (window)
  "Return whether WINDOW accepts input, including its ancestors' enabled state."
  (boolean-result (%enabled (handle window))))
(defun destroy-window (window) (check-status (%destroy (handle window))))
(defun call-setter (function notify)
  (let ((*in-notifying-setter* (or notify *in-notifying-setter*)))
    (check-status (funcall function)))
  ;; Restore the enclosing boundary before re-signaling. An unhandled error
  ;; will reach the outer callback, which then requests the loop stop.
  (check-callback-error))
(defun set-checked (checkbox checked &key notify)
  "Set a generalized boolean state silently, or emit a synchronous :TOGGLE event
with :NOTIFY T. Return the requested state as T or NIL, even if a handler changes
it again. Handler errors can be caught by this caller."
  (call-setter (lambda () (%set-checked (handle checkbox) (if checked 1 0) (if notify 1 0)))
               notify)
  (not (null checked)))
(defun set-text (control value &key notify)
  "Set CONTROL's text silently, or emit a text-change event when NOTIFY is true.
Handlers may run synchronously. Their errors are re-signaled after the native
setter returns, including when RUN has not yet been called."
  (call-setter (lambda () (%set-text (handle control) value (if notify 1 0))) notify)
  value)
(defun get-text (control)
  "Return an independent Lisp string containing CONTROL's current text."
  (let ((pointer (%get-text (handle control))))
    (when (cffi:null-pointer-p pointer)
      (error 'native-error :message (%last-error)))
    (cffi:foreign-string-to-lisp pointer :encoding :utf-8)))

(defun register-handler (source function native-register)
  (unless *active* (error "No active application session"))
  (check-type function function)
  (let ((token (incf *next-token*))
        (old-token (handler-token source))
        (registered nil))
    ;; Root the closure and source before publishing the token to the bridge.
    (setf (gethash token *handlers*) (make-handler :source source :function function))
    (unwind-protect
         (progn
           (check-status (funcall native-register (handle source) token))
           (setf (handler-token source) token registered t)
           (when old-token (remhash old-token *handlers*)))
      (unless registered (remhash token *handlers*))))
  source)

(defun unregister-handler (source native-unregister)
  (unless *active* (error "No active application session"))
  (check-status (funcall native-unregister (handle source)))
  (let ((token (handler-token source)))
    (when token (remhash token *handlers*)))
  (setf (handler-token source) nil)
  source)

(defun on-click (button function &key with-event)
  "Register FUNCTION, releasing any previous handler's closure.
By default FUNCTION takes no arguments; WITH-EVENT passes one EVENT instead."
  (check-type function function)
  (register-handler button
                    (if with-event function
                        (lambda (event) (declare (ignore event)) (funcall function)))
                    #'%on-click))

(defun off-click (button)
  "Remove BUTTON's handler and release its closure. Repeated removal is harmless.
Queued clicks use the handler registered at dispatch time."
  (unregister-handler button #'%off-click))

(defun on-text-change (control function)
  "Register FUNCTION taking one EVENT with kind :TEXT-CHANGE and copied text.
Replaces and releases the previous text handler. User edits and SET-TEXT with
:NOTIFY T generate events; ordinary SET-TEXT calls remain silent."
  (register-handler control function #'%on-text-change))

(defun off-text-change (control)
  "Remove CONTROL's text-change handler and release its closure. Idempotent
for a live text control; safe to call from inside the handler."
  (unregister-handler control #'%off-text-change))

(defun on-toggle (checkbox function)
  "Register FUNCTION taking one :TOGGLE event with an EVENT-CHECKED-P snapshot."
  (register-handler checkbox function #'%on-toggle))
(defun off-toggle (checkbox)
  "Remove CHECKBOX's toggle handler and release its closure. Idempotent for a
live checkbox, including when called from inside its handler."
  (unregister-handler checkbox #'%off-toggle))

(defun queue-click (button) (check-status (%click (handle button))))

(defun make-choice (parent &key (items nil) selection)
  "Create a dropdown from a list of strings. SELECTION is a zero-based index or NIL."
  (check-type items list)
  (check-type selection (or null (integer 0 2147483647)))
  (dolist (item items) (check-type item string))
  (let ((strings nil))
    (unwind-protect
         (progn
           (dolist (item items)
             (push (cffi:foreign-string-alloc item :encoding :utf-8) strings))
           (setf strings (nreverse strings))
           (cffi:with-foreign-object (array :pointer (max 1 (length strings)))
             (loop for pointer in strings for i from 0 do
               (setf (cffi:mem-aref array :pointer i) pointer))
             (wrap-window 'choice (%choice (handle parent) array (length strings) (or selection -1)))))
      (dolist (pointer strings) (cffi:foreign-string-free pointer)))))

(defun selection (choice)
  "Return the zero-based selected index, or NIL when nothing is selected."
  (let ((index (%selection (handle choice))))
    (when (= index -2) (error 'native-error :message (%last-error)))
    (unless (= index -1) index)))

(defun set-selection (choice index &key notify)
  "Select INDEX (zero-based) or NIL. NOTIFY emits synchronously, even if unchanged.
Return the requested index. Handler errors are re-signaled to the caller."
  (check-type index (or null (integer 0 2147483647)))
  (call-setter (lambda () (%set-selection (handle choice) (or index -1) (if notify 1 0))) notify)
  index)

(defun on-selection (choice function)
  "Register a handler taking one :SELECTION event with an EVENT-SELECTION snapshot."
  (register-handler choice function #'%on-selection))
(defun off-selection (choice)
  "Remove the selection handler and release its closure. Idempotent for a live choice."
  (unregister-handler choice #'%off-selection))
