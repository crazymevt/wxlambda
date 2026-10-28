(in-package #:wxlambda)

(define-condition native-error (error)
  ((message :initarg :message :reader native-error-message))
  (:report (lambda (condition stream) (write-string (native-error-message condition) stream))))

(defclass event-handler () ((handle :initform nil :reader handle)))
(defclass application-handler (event-handler) ()
  (:documentation "Borrowed bridge application identity; obtain with CURRENT-APPLICATION-HANDLER."))
(defvar *application-handler* nil)
(defun current-application-handler ()
  "Return the unique borrowed application handler for the active session."
  (require-active-application)
  (let ((id (%application-handler)))
    (when (zerop id) (error 'native-error :message (%last-error)))
    (or *application-handler*
        (let ((view (allocate-instance (find-class 'application-handler))))
          (setf (slot-value view 'handle) id
                *application-handler* view)))))
(defclass window (event-handler) ())
(defmethod handle :around ((object event-handler))
  (let ((id (and (slot-boundp object 'handle) (call-next-method))))
    (or id (error 'native-error :message "Object has no native handle; construction did not complete"))))
(defclass frame (window) ())
(defclass panel (window) ())
(defclass button (window)
  ((handler-token :initform nil :accessor handler-token)))
(defclass text-control (window)
  ((handler-token :initform nil :accessor handler-token)))

(defclass choice (window)
  ((handler-token :initform nil :accessor handler-token)))

(defclass label (window) ())
(defclass gauge (window) ())
(defclass checkbox (window)
  ((handler-token :initform nil :accessor handler-token)))

(defstruct (event (:constructor %make-event (kind source text checked-p selection identifier native-type-code can-veto veto family-code integer logging-off time-stamp command origin-code)))
  "A Lisp-owned snapshot. Slots are read-only; TEXT is an independent string."
  (kind nil :read-only t)
  (source nil :read-only t)
  (text nil :read-only t)
  (checked-p nil :read-only t)
  (selection nil :read-only t)
  (identifier 0 :read-only t)
  (native-type-code 0 :read-only t)
  (can-veto nil :read-only t)
  (veto nil :read-only t)
  (family-code 0 :read-only t)
  (integer 0 :read-only t)
  (logging-off nil :read-only t)
  (time-stamp 0 :read-only t)
  (command nil :read-only t)
  (origin-code 0 :read-only t))

(defstruct (event-binding (:constructor %make-event-binding (source kind token)))
  (source nil :read-only t) (kind nil :read-only t) (token 0 :read-only t))
(defvar *registered-event-types* nil)
(defvar *registered-event-kinds* nil)
(defvar *registered-event-families* nil)
(defvar *event-bindings* nil)
(defstruct (event-context (:constructor %make-event-context (token)))
  (token 0 :read-only t))
(defstruct handler source function with-context)
(defvar *active* nil)
(defvar *repl-running* nil)
(defvar *repl-debugging* nil)
(defvar *handlers* nil)
(defvar *callback-error* nil)
(defvar *in-notifying-setter* nil)
(defvar *next-token* 0)
(defvar *construction-context* nil)

(defun check-status (status)
  (unless (zerop status) (error 'native-error :message (%last-error)))
  t)

(cffi:defcallback dispatch-event :void ((token :uint64) (payload :pointer))
  ;; Copy borrowed payloads before user code, including reentrant native calls.
  ;; Unwinding through C++ is unsafe. Re-signal after the native call returns.
  (handler-case
      (unless *callback-error*
        (let ((handler (gethash token *handlers*)))
          (when handler
            (when (cffi:null-pointer-p payload) (error "Missing event payload"))
            (cffi:with-foreign-slots ((kind text checked selection id text-length context native-type can-veto vetoed family integer logging-off timestamp command origin-id) payload (:struct native-event))
              (let* ((event-kind (or (case kind (1 :click) (2 :text-change) (3 :toggle) (4 :selection) (5 :close) (6 :query-end-session) (7 :end-session))
                                     (gethash kind *registered-event-kinds*)
                                     (error "Unregistered native event kind ~S" kind)))
                     (value (progn
                              (when (and (= kind 2) (cffi:null-pointer-p text))
                                (error "Missing text-change payload"))
                              (unless (cffi:null-pointer-p text)
                                (cffi:foreign-string-to-lisp text :count text-length :encoding :utf-8))))
                     (checked-p (and (= kind 3) (not (zerop checked))))
                     (event (%make-event event-kind (handler-source handler) value checked-p
                                         (when (= kind 4) (unless (= selection -1) selection)) id native-type
                                         (not (zerop can-veto)) (not (zerop vetoed)) family integer (not (zerop logging-off)) timestamp (not (zerop command)) origin-id)))
                (if (handler-with-context handler)
                    (funcall (handler-function handler) event (%make-event-context context))
                    (funcall (handler-function handler) event)))))))
    (error (condition)
      (unless *callback-error* (setf *callback-error* condition))
      ;; Notifying setters have a nearer Lisp boundary where their caller can
      ;; handle this error. Only errors escaping that boundary stop the loop.
      (unless *in-notifying-setter* (%stop)))))

(defun reap-event-bindings ()
  (when (and *active* *event-bindings*)
    (let (expired)
      (maphash (lambda (token binding)
                 (declare (ignore binding))
                 (let ((active (%binding-active token)))
                   (when (minusp active) (check-status active))
                   (when (zerop active) (push token expired))))
               *event-bindings*)
      (dolist (token expired)
        (remhash token *event-bindings*)
        (remhash token *handlers*)))))

(defun check-callback-error ()
  ;; Preserve an escaping user condition if cleanup itself encounters an error.
  (if *callback-error* (ignore-errors (reap-event-bindings)) (reap-event-bindings))
  (when *callback-error*
    (let ((condition *callback-error*))
      (setf *callback-error* nil)
      (error condition))))

(defun %call-with-application (function)
  "Call FUNCTION in one GUI session on the process main thread.
Run explicitly with RUN. Only one session per process is currently supported."
  (when *active* (error "An application session is already active"))
  (load-native-library)
  (let ((*active* t) (*application-handler* nil) (*handlers* (make-hash-table)) (*event-bindings* (make-hash-table)) (*callback-error* nil)
        (*registered-event-types* (make-hash-table)) (*registered-event-kinds* (make-hash-table))
        (*registered-event-families* (make-hash-table)))
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
  "Process GUI events and idle work without blocking; forbidden inside pumps or callbacks."
  (when *repl-debugging*
    (error "GUI event processing is paused in the application REPL debugger"))
  (check-status (%poll))
  (check-callback-error))

(defun require-active-application ()
  (unless *active*
    (error 'native-error :message "An active application session is required")))

(defun main-loop-running-p ()
  "Return true while the active application's native blocking main loop runs."
  (require-active-application)
  (boolean-result (%main-loop-running)))
(defun pending-events-p ()
  "Query queued wx events, excluding OS input, idle work and future timers."
  (require-active-application)
  (boolean-result (%pending-events)))
(defun process-pending-events ()
  "Dispatch queued wx events only, outside RUN, other pumps and callbacks."
  (require-active-application)
  (when *repl-debugging*
    (error "GUI event processing is paused in the application REPL debugger"))
  (check-status (%process-pending-events))
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

;;; Native-backed CLOS initialization. A concrete class owns exactly one native kind.
(defun adopt-native (object id)
  (when (zerop id) (error 'native-error :message (%last-error)))
  (setf (slot-value object 'handle) id)
  object)

(defun rollback-construction (objects)
  (when *event-bindings*
    (let (tokens)
      (maphash (lambda (token binding)
                 (when (member (event-binding-source binding) objects :test #'eq)
                   (push token tokens))) *event-bindings*)
      (dolist (token tokens)
        (ignore-errors (%unbind token))
        (remhash token *event-bindings*))))
  ;; Remove Lisp callback roots before requesting native destruction. Children
  ;; created by nested initializers appear before their parents in this list.
  (when *handlers*
    (let ((tokens nil))
      (maphash (lambda (token handler)
                 (when (member (handler-source handler) objects :test #'eq)
                   (push token tokens)))
               *handlers*)
      (dolist (token tokens) (remhash token *handlers*))))
  (dolist (object objects)
    (when (and (slot-exists-p object 'handler-token)
               (slot-boundp object 'handler-token))
      (setf (handler-token object) nil))
    (when (and (slot-boundp object 'handle) (slot-value object 'handle))
      ;; Keep the original initialization condition/nonlocal exit intact.
      (ignore-errors (funcall (if (typep object 'window) #'%destroy #'%handler-destroy)
                             (slot-value object 'handle)))
      (setf (slot-value object 'handle) nil))))

(defmethod initialize-instance :around ((object event-handler) &rest initargs)
  (declare (ignore initargs))
  (when (typep object 'application-handler)
    (error "APPLICATION-HANDLER is borrowed; use CURRENT-APPLICATION-HANDLER"))
  (when (and (typep object 'window)
             (/= 1 (count-if (lambda (kind) (typep object kind))
                         '(frame panel button text-control label checkbox choice gauge))))
    (error "~A must inherit exactly one concrete wxlambda widget class; WINDOW is abstract"
           (class-name (class-of object))))
  (unless *active*
    (error 'native-error :message "Native construction requires an active application session"))
  (when (and (slot-boundp object 'handle) (slot-value object 'handle))
    (error "This object already has a native handle"))
  (let* ((parent-context *construction-context*)
         (*construction-context* (list (list object)))
         (complete nil))
    (unwind-protect
         (multiple-value-prog1 (call-next-method)
           (unless (and (slot-boundp object 'handle) (slot-value object 'handle))
             (error "No native object was created; INITIALIZE-INSTANCE must call its inherited method"))
           (when parent-context
             (setf (car parent-context) (nconc (car *construction-context*) (car parent-context))))
           (setf complete t))
      (unless complete (rollback-construction (car *construction-context*))))))

(defmethod initialize-instance :after ((object event-handler) &rest initargs)
  (declare (ignore initargs))
  ;; This least-specific AFTER method runs before any native allocation.
  (unless (slot-boundp object 'handle)
    (error "INITIALIZE-INSTANCE must call inherited initialization before native construction"))
  (unless (typep object 'window) (adopt-native object (%event-handler))))

(defmethod initialize-instance :after ((object frame) &key (title "wxlambda") (width 480) (height 240))
  (adopt-native object (%frame title width height)))
(defmethod initialize-instance :after ((object panel) &key parent)
  (check-type parent window)
  (adopt-native object (%panel (handle parent))))
(defmethod initialize-instance :after ((object button) &key parent (label "Button"))
  (check-type parent window)
  (adopt-native object (%button (handle parent) label)))
(defmethod initialize-instance :after ((object text-control) &key parent (value ""))
  (check-type parent window)
  (adopt-native object (%text (handle parent) value)))
(defmethod initialize-instance :after ((object label) &key parent (label ""))
  (check-type parent window)
  (adopt-native object (%label (handle parent) label)))
(defmethod initialize-instance :after ((object checkbox) &key parent (label "") checked)
  (check-type parent window)
  (adopt-native object (%checkbox (handle parent) label (if checked 1 0))))
(defmethod initialize-instance :after ((object gauge) &key parent (range 100) (value 0) (orientation :horizontal))
  (check-type parent window)
  (check-type range (integer 1 2147483647))
  (check-type value (integer 0 2147483647))
  (check-type orientation (member :horizontal :vertical))
  (adopt-native object (%gauge (handle parent) range value (if (eq orientation :vertical) 1 0))))
(defmethod initialize-instance :after ((object choice) &key parent (items nil) selection)
  (check-type parent window)
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
             (adopt-native object (%choice (handle parent) array (length strings) (or selection -1)))))
      (dolist (pointer strings) (cffi:foreign-string-free pointer)))))

(defun make-widget (class expected &rest initargs)
  (let ((resolved (if (typep class 'class) class (find-class class))))
    (unless (subtypep resolved (find-class expected))
      (error "~A is not a subclass of ~A" class expected))
    (apply #'make-instance resolved
           (loop for (key value) on initargs by #'cddr
                 unless (eq key :class) append (list key value)))))

(defun make-frame (&rest initargs &key (class 'wxlambda:frame) &allow-other-keys)
  (apply #'make-widget class 'frame initargs))
(defgeneric make-panel (parent &rest initargs &key class  &allow-other-keys)
  (:documentation "Construct a panel or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-panel ((parent window) &rest initargs &key (class 'wxlambda:panel) &allow-other-keys)
  (apply #'make-widget class 'panel :parent parent initargs))
(defgeneric make-button (parent &rest initargs &key class label &allow-other-keys)
  (:documentation "Construct a button or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-button ((parent window) &rest initargs &key (class 'wxlambda:button) &allow-other-keys)
  (apply #'make-widget class 'button :parent parent initargs))
(defgeneric make-text (parent &rest initargs &key class value &allow-other-keys)
  (:documentation "Construct a text-control or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-text ((parent window) &rest initargs &key (class 'wxlambda:text-control) &allow-other-keys)
  (apply #'make-widget class 'text-control :parent parent initargs))
(defgeneric make-label (parent &rest initargs &key class label &allow-other-keys)
  (:documentation "Construct a label or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-label ((parent window) &rest initargs &key (class 'wxlambda:label) &allow-other-keys)
  (apply #'make-widget class 'label :parent parent initargs))
(defgeneric make-checkbox (parent &rest initargs &key class label checked &allow-other-keys)
  (:documentation "Construct a checkbox or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-checkbox ((parent window) &rest initargs &key (class 'wxlambda:checkbox) &allow-other-keys)
  (apply #'make-widget class 'checkbox :parent parent initargs))
(defgeneric make-gauge (parent &rest initargs &key class range value orientation &allow-other-keys)
  (:documentation "Construct a gauge or subclass with forwarded native and subclass initargs."))
(defmethod make-gauge ((parent window) &rest initargs &key (class 'gauge) &allow-other-keys)
  (apply #'make-widget class 'gauge :parent parent initargs))


(defgeneric make-choice (parent &rest initargs &key class items selection &allow-other-keys)
  (:documentation "Construct a choice or subclass using MAKE-INSTANCE and forwarded initargs."))
(defmethod make-choice ((parent window) &rest initargs &key (class 'wxlambda:choice) &allow-other-keys)
  (apply #'make-widget class 'choice :parent parent initargs))

(defgeneric label-text (label)
  (:documentation "Read raw label text including mnemonic syntax. Other control types are not accepted."))

(defmethod label-text ((label label))
  "Return an independent string containing static LABEL's raw text."
  (let ((pointer (%get-label (handle label))))
    (when (cffi:null-pointer-p pointer)
      (error 'native-error :message (%last-error)))
    (cffi:foreign-string-to-lisp pointer :encoding :utf-8)))

(defgeneric checked-p (checkbox &key notify)
  (:documentation "Read the boolean checkbox state."))

(defmethod checked-p ((checkbox checkbox) &key notify)
  "Return T or NIL for CHECKBOX's current state."
  (declare (ignore notify))
  (let ((result (%checked (handle checkbox))))
    (when (minusp result) (error 'native-error :message (%last-error)))
    (not (zerop result))))
(defgeneric make-column (parent)
  (:documentation "Attach a parent-owned vertical box sizer. Standalone nested sizer objects are not exposed."))
(defmethod make-column ((parent window)) (check-status (%column (handle parent))) parent)
(defgeneric make-row (parent)
  (:documentation "Attach a parent-owned horizontal box sizer. Standalone nested sizer objects are not exposed."))
(defmethod make-row ((parent window)) (check-status (%row (handle parent))) parent)
(defgeneric add-child (parent child &key proportion border expand align)
  (:documentation "Add a child window to the layout. Non-start minor-axis alignment requires EXPAND NIL. Borders apply uniformly on four sides."))
(defmethod add-child ((parent window) (child window) &key (proportion 0) (border 8) (expand t) (align :start))
  "Add CHILD to PARENT's row/column. PROPORTION allocates the major axis;
EXPAND fills the minor axis. With EXPAND NIL, ALIGN is :START, :CENTER, or :END.
BORDER is nonnegative spacing on all four sides, in wxWidgets coordinates."
  (check-status (%add (handle parent) (handle child) proportion border (if expand 1 0)
                      (ecase align (:start 0) (:center 1) (:end 2))))
  child)
(defgeneric add-spacer (parent size)
  (:documentation "Add fixed layout-axis spacing and relayout the parent."))
(defmethod add-spacer ((parent window) size)
  "Add fixed nonnegative SIZE along PARENT's layout axis."
  (check-status (%spacer (handle parent) size)) parent)
(defgeneric add-stretch (parent &key proportion)
  (:documentation "Add flexible layout-axis spacing and relayout the parent."))
(defmethod add-stretch ((parent window) &key (proportion 1))
  "Add flexible space with positive PROPORTION along PARENT's layout axis."
  (check-status (%stretch (handle parent) proportion)) parent)
(defgeneric layout-window (window)
  (:documentation "Recompute the window sizer layout, including after batching label changes. Native success boolean is not exposed."))
(defmethod layout-window ((window window))
  "Recompute WINDOW's sizer layout after batching changes."
  (check-status (%layout (handle window))) window)
(defun window-geometry (window)
  (cffi:with-foreign-objects ((x :int) (y :int) (width :int) (height :int))
    (check-status (%geometry (handle window) x y width height))
    (values (cffi:mem-ref x :int) (cffi:mem-ref y :int)
            (cffi:mem-ref width :int) (cffi:mem-ref height :int))))
(defgeneric window-position (window)
  (:documentation "Query position relative to parent; frame coordinates are screen-relative."))
(defmethod window-position ((window window))
  "Return X and Y as two values, relative to the parent (screen for frames)."
  (multiple-value-bind (x y width height) (window-geometry window)
    (declare (ignore width height))
    (values x y)))
(defgeneric window-size (window)
  (:documentation "Query outer window dimensions in wxWidgets coordinates."))
(defmethod window-size ((window window))
  "Return outer WIDTH and HEIGHT as two values in wxWidgets coordinates."
  (multiple-value-bind (x y width height) (window-geometry window)
    (declare (ignore x y))
    (values width height)))
(defun get-window-string (window native-getter)
  (cffi:with-foreign-object (length :size)
    (let ((pointer (funcall native-getter (handle window) length)))
      (when (cffi:null-pointer-p pointer)
        (error 'native-error :message (%last-error)))
      (values (cffi:foreign-string-to-lisp pointer :encoding :utf-8
                                        :count (cffi:mem-ref length :size))))))
(defun set-window-string (window value native-setter)
  (cffi:with-foreign-string ((pointer byte-size) value :encoding :utf-8)
    ;; CFFI's UTF-8 allocation includes one terminating byte.
    (check-status (funcall native-setter (handle window) pointer (1- byte-size))))
  value)
(defgeneric window-name (window)
  (:documentation "Read the logical window name, independent of displayed labels, text, title and native ID. Names need not be unique; Unicode, empty strings and embedded NULs are preserved."))
(defmethod window-name ((window window))
  "Return an independent string containing WINDOW's logical name, not its label.
Names need not be unique. Embedded NULs are preserved."
  (get-window-string window #'%name))

(defgeneric frame-title (frame)
  (:documentation "Read the frame title, including the title initially supplied to MAKE-FRAME. Returned strings survive later metadata queries and destruction."))

(defmethod frame-title ((frame frame))
  "Return an independent string containing FRAME's title. Accepts frames only."
  (get-window-string frame #'%title))

(defun boolean-result (result)
  (when (minusp result) (error 'native-error :message (%last-error)))
  (not (zerop result)))
(defgeneric show-window (window &optional show)
  (:documentation "Show with true SHOW, hide with NIL. Existing one-argument calls still show the window and return it as their primary value."))
(defmethod show-window ((window window) &optional (show t))
  "Apply the state and return WINDOW and the native changed-state flag."
  (multiple-value-bind (value changed) (funcall #'(setf shown-p) show window)
    (declare (ignore value))
    (values window changed)))
(defgeneric shown-p (window)
  (:documentation "Read the window's own shown flag. A hidden ancestor does not change this flag; it does not report effective screen visibility."))
(defmethod shown-p ((window window))
  "Return WINDOW's own shown flag, not its effective visibility on screen."
  (boolean-result (%shown (handle window))))
(defgeneric enable-window (window &optional enable)
  (:documentation "Enable or disable user input. Repeated requests are harmless; a disabled parent also disables its children effectively."))
(defmethod enable-window ((window window) &optional (enable t))
  "Apply the state and return WINDOW and the native changed-state flag."
  (multiple-value-bind (value changed) (funcall #'(setf enabled-p) enable window)
    (declare (ignore value))
    (values window changed)))
(defgeneric enabled-p (window)
  (:documentation "Read effective enabled state, including disabled ancestors. A child's own disabled state survives parent disable/re-enable."))
(defmethod enabled-p ((window window))
  "Return whether WINDOW accepts input, including its ancestors' enabled state."
  (boolean-result (%enabled (handle window))))
(defgeneric destroy-window (window)
  (:documentation "Request destruction; first explicitly unbind handlers to release Lisp closures. Child handles become invalid when native destruction occurs."))
(defmethod destroy-window ((window window))
  (check-status (%destroy (handle window)))
  (reap-event-bindings)
  t)
(defun call-setter (function notify)
  (let ((*in-notifying-setter* (or notify *in-notifying-setter*)))
    (check-status (funcall function)))
  ;; Restore the enclosing boundary before re-signaling. An unhandled error
  ;; will reach the outer callback, which then requests the loop stop.
  (check-callback-error))

(defgeneric text (control &key notify)
  (:documentation "Read the current UTF-8 text; returned storage survives later native calls."))

(defmethod text ((control text-control) &key notify)
  "Return an independent Lisp string containing CONTROL's current text."
  (declare (ignore notify))
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

(defgeneric on-click (button function &key with-event)
  (:documentation "Replace and release any prior handler. Receive :CLICK snapshots with no payload. Callback conditions never unwind through C++; they are re-signaled at a Lisp boundary."))

(defmethod on-click ((button button) function &key with-event)
  "Register FUNCTION, releasing any previous handler's closure.
By default FUNCTION takes no arguments; WITH-EVENT passes one EVENT instead."
  (check-type function function)
  (register-handler button
                    (if with-event function
                        (lambda (event) (declare (ignore event)) (funcall function)))
                    #'%on-click))

(defgeneric off-click (button)
  (:documentation "Remove the handler and release its closure. Repeated removal is harmless for a live control, including inside its callback."))

(defmethod off-click ((button button))
  "Remove BUTTON's handler and release its closure. Repeated removal is harmless.
Queued clicks use the handler registered at dispatch time."
  (unregister-handler button #'%off-click))

(defgeneric on-text-change (control function)
  (:documentation "Replace and release any prior handler. Receive :TEXT-CHANGE snapshots with copied text. Callback conditions never unwind through C++; they are re-signaled at a Lisp boundary."))

(defmethod on-text-change ((control text-control) function)
  "Register FUNCTION taking one EVENT with kind :TEXT-CHANGE and copied text.
Replaces and releases the previous text handler. User edits and SETF on TEXT with
:NOTIFY T generate events; ordinary SETF on TEXT calls remain silent."
  (register-handler control function #'%on-text-change))

(defgeneric off-text-change (control)
  (:documentation "Remove the handler and release its closure. Repeated removal is harmless for a live control, including inside its callback."))

(defmethod off-text-change ((control text-control))
  "Remove CONTROL's text-change handler and release its closure. Idempotent
for a live text control; safe to call from inside the handler."
  (unregister-handler control #'%off-text-change))

(defgeneric on-toggle (checkbox function)
  (:documentation "Replace and release any prior handler. Receive :TOGGLE snapshots with boolean state. Callback conditions never unwind through C++; they are re-signaled at a Lisp boundary."))

(defmethod on-toggle ((checkbox checkbox) function)
  "Register FUNCTION taking one :TOGGLE event with an EVENT-CHECKED-P snapshot."
  (register-handler checkbox function #'%on-toggle))
(defgeneric off-toggle (checkbox)
  (:documentation "Remove the handler and release its closure. Repeated removal is harmless for a live control, including inside its callback."))

(defmethod off-toggle ((checkbox checkbox))
  "Remove CHECKBOX's toggle handler and release its closure. Idempotent for a
live checkbox, including when called from inside its handler."
  (unregister-handler checkbox #'%off-toggle))

(defgeneric queue-click (button)
  (:documentation "Post a button click for later dispatch. It uses the handler registered at dispatch time, including any replacement."))

(defmethod queue-click ((button button)) (check-status (%click (handle button))))

(defgeneric selection (choice &key notify)
  (:documentation "Read selected index; NIL represents native no selection."))

(defmethod selection ((choice choice) &key notify)
  "Return the zero-based selected index, or NIL when nothing is selected."
  (declare (ignore notify))
  (let ((index (%selection (handle choice))))
    (when (= index -2) (error 'native-error :message (%last-error)))
    (unless (= index -1) index)))

(defgeneric on-selection (choice function)
  (:documentation "Replace and release any prior handler. Receive :SELECTION snapshots with selected index or NIL. Callback conditions never unwind through C++; they are re-signaled at a Lisp boundary."))

(defmethod on-selection ((choice choice) function)
  "Register a handler taking one :SELECTION event with an EVENT-SELECTION snapshot."
  (register-handler choice function #'%on-selection))
(defgeneric off-selection (choice)
  (:documentation "Remove the handler and release its closure. Repeated removal is harmless for a live control, including inside its callback."))

(defmethod off-selection ((choice choice))
  "Remove the selection handler and release its closure. Idempotent for a live choice."
  (unregister-handler choice #'%off-selection))

;;; SETF is the canonical property-writing protocol.
(defgeneric (setf text) (value control &key notify)
  (:documentation "Assign text and return the supplied value."))
(defmethod (setf text) (value (control text-control) &key notify)
  (call-setter (lambda () (%set-text (handle control) value (if notify 1 0))) notify)
  value)
(defgeneric (setf label-text) (value label)
  (:documentation "Assign label-text and return the supplied value."))
(defmethod (setf label-text) (value (label label))
  (check-status (%set-label (handle label) value)) value)
(defgeneric (setf checked-p) (value checkbox &key notify)
  (:documentation "Assign checked-p and return the supplied value."))
(defmethod (setf checked-p) (checked (checkbox checkbox) &key notify)
  (call-setter (lambda () (%set-checked (handle checkbox) (if checked 1 0) (if notify 1 0)))
               notify)
  checked)
(defgeneric (setf selection) (value choice &key notify)
  (:documentation "Assign selection and return the supplied value."))
(defmethod (setf selection) (index (choice choice) &key notify)
  (check-type index (or null (integer 0 2147483647)))
  (call-setter (lambda () (%set-selection (handle choice) (or index -1) (if notify 1 0))) notify)
  index)
(defgeneric (setf window-name) (value window)
  (:documentation "Assign window-name and return the supplied value."))
(defmethod (setf window-name) (value (window window))
  (set-window-string window value #'%set-name))
(defgeneric (setf frame-title) (value frame)
  (:documentation "Assign frame-title and return the supplied value."))
(defmethod (setf frame-title) (value (frame frame))
  (set-window-string frame value #'%set-title))
(defgeneric (setf shown-p) (value window)
  (:documentation "Assign shown-p; return the supplied value and the native changed-state flag."))
(defmethod (setf shown-p) (value (window window))
  (values value (boolean-result (%show-state (handle window) (if value 1 0)))))
(defgeneric (setf enabled-p) (value window)
  (:documentation "Assign enabled-p; return the supplied value and the native changed-state flag."))
(defmethod (setf enabled-p) (value (window window))
  (values value (boolean-result (%enable (handle window) (if value 1 0)))))


(defun gauge-integer-result (value)
  (when (minusp value) (error 'native-error :message (%last-error)))
  value)
(defgeneric gauge-value (gauge)
  (:documentation "Read determinate progress. After pulsing, the value is platform-dependent."))
(defmethod gauge-value ((gauge gauge))
  (gauge-integer-result (%gauge-value (handle gauge))))
(defgeneric gauge-range (gauge)
  (:documentation "Read the positive maximum progress value."))
(defmethod gauge-range ((gauge gauge))
  (gauge-integer-result (%gauge-range (handle gauge))))
(defgeneric (setf gauge-value) (value gauge)
  (:documentation "Set progress from zero through the range and return VALUE; leave pulse mode."))
(defmethod (setf gauge-value) (value (gauge gauge))
  (check-type value (integer 0 2147483647))
  (check-status (%set-gauge-value (handle gauge) value))
  value)
(defgeneric (setf gauge-range) (value gauge)
  (:documentation "Set a positive range, clamp progress, leave pulse mode and return VALUE."))
(defmethod (setf gauge-range) (value (gauge gauge))
  (check-type value (integer 1 2147483647))
  (check-status (%set-gauge-range (handle gauge) value))
  value)
(defgeneric gauge-vertical-p (gauge)
  (:documentation "Return true for a vertically constructed gauge."))
(defmethod gauge-vertical-p ((gauge gauge))
  (boolean-result (%gauge-vertical (handle gauge))))
(defgeneric pulse-gauge (gauge)
  (:documentation "Advance platform-native indeterminate progress and return GAUGE."))
(defmethod pulse-gauge ((gauge gauge))
  (check-status (%gauge-pulse (handle gauge)))
  gauge)

(defun make-event-handler (&rest initargs &key (class 'event-handler) &allow-other-keys)
  "Construct an owned standalone event handler or subclass with forwarded initargs."
  (let ((resolved (if (typep class 'class) class (find-class class))))
    (when (subtypep resolved (find-class 'window))
      (error "MAKE-EVENT-HANDLER requires a standalone class, not a window class"))
    (apply #'make-widget resolved 'event-handler initargs)))
(defgeneric handler-alive-p (handler)
  (:documentation "Return whether the native handler is live in the active session."))
(defmethod handler-alive-p ((handler event-handler))
  (require-active-application)
  (boolean-result (%handler-alive (handle handler))))
(defgeneric handler-enabled-p (handler)
  (:documentation "Read wxEvtHandler's own event-processing flag, not window input state."))
(defmethod handler-enabled-p ((handler event-handler))
  (require-active-application)
  (boolean-result (%handler-enabled (handle handler))))
(defgeneric (setf handler-enabled-p) (value handler)
  (:documentation "Assign the own event-processing flag and return VALUE."))
(defmethod (setf handler-enabled-p) (value (handler event-handler))
  (require-active-application)
  (check-status (%set-handler-enabled (handle handler) (if value 1 0)))
  value)
(defgeneric destroy-event-handler (handler)
  (:documentation "Destroy an owned standalone handler; reject borrowed window handlers."))
(defmethod destroy-event-handler ((handler event-handler))
  (require-active-application)
  (check-status (%handler-destroy (handle handler)))
  (reap-event-bindings)
  t)

(defun builtin-event-kind (kind)
  (case kind (:click 1) (:text-change 2) (:toggle 3) (:selection 4) (:close 5) (:query-end-session 6) (:end-session 7)))
(defun native-event-kind (kind)
  (or (builtin-event-kind kind)
      (and *registered-event-types* (gethash kind *registered-event-types*))
      (error "Unknown or unregistered event kind ~S" kind)))
(defun check-event-id (id)
  (check-type id (integer -2147483648 2147483647))
  (when (= id -1) (error "Event ID -1 is reserved; use NIL for an unfiltered binding"))
  id)
(defgeneric bind-event (handler kind function &key id last-id with-context)
  (:documentation "Bind an observer, or a two-argument contextual callback with explicit Skip control."))
(defmethod bind-event ((source event-handler) kind function &key id last-id with-context)
  (require-active-application)
  (check-type function function)
  (let ((native-kind (native-event-kind kind)))
    (when id (check-event-id id))
    (when last-id
      (check-event-id last-id)
      (unless (and id (<= id last-id)) (error "LAST-ID requires an ordered ID range")))
    (let* ((token (incf *next-token*))
           (binding (%make-event-binding source kind token))
           (complete nil))
      (setf (gethash token *event-bindings*) binding
            (gethash token *handlers*) (make-handler :source source :function function :with-context with-context))
      (unwind-protect
           (progn
             (check-status (%bind (handle source) native-kind token (or id -1) (or last-id -1) (if with-context 1 0)))
             (setf complete t)
             binding)
        (unless complete
          (remhash token *event-bindings*)
          (remhash token *handlers*))))))
(defgeneric binding-active-p (binding)
  (:documentation "Query a binding in the active session, releasing stale generic roots."))
(defmethod binding-active-p ((binding event-binding))
  (require-active-application)
  (reap-event-bindings)
  (eq binding (gethash (event-binding-token binding) *event-bindings*)))
(defgeneric unbind-event (binding)
  (:documentation "Remove this independent binding; return whether it was active."))
(defmethod unbind-event ((binding event-binding))
  (require-active-application)
  (let ((token (event-binding-token binding)))
    (unless (eq binding (gethash token *event-bindings*)) (return-from unbind-event nil))
    (let ((removed (boolean-result (%unbind token))))
      (remhash token *event-bindings*)
      (remhash token *handlers*)
      removed)))
(defun call-synthetic-event (function handler kind id text checked-p selection can-veto-p can-supplied
                            integer integer-supplied vetoed-p veto-supplied logging-off-p logging-supplied)
  (require-active-application)
  (let* ((native-kind (native-event-kind kind))
         (custom (and *registered-event-families* (gethash kind *registered-event-families*))))
    (when (and logging-supplied (not (member kind '(:query-end-session :end-session))))
      (error "LOGGING-OFF-P applies only to session events"))
    (check-event-id id)
    (check-type text string)
    (check-type selection (or null (integer 0 2147483647)))
    (check-type integer (integer -2147483648 2147483647))
    (unless (or (eq kind :text-change) custom (zerop (length text)))
      (error "TEXT applies only to text-change or registered event types"))
    (when (and checked-p (not (eq kind :toggle))) (error "CHECKED-P applies only to :TOGGLE"))
    (when (and selection (not (eq kind :selection))) (error "SELECTION applies only to :SELECTION"))
    (when (and can-supplied (not (member kind '(:close :query-end-session :end-session)))) (error "CAN-VETO-P applies only to close/session events"))
    (when (and integer-supplied (not custom)) (error "INTEGER applies only to registered event types"))
    (when (and veto-supplied (not (eq custom :notify))) (error "VETOED-P applies only to registered notify types"))
    (cffi:with-foreign-string ((pointer bytes) text :encoding :utf-8)
      (funcall function (handle handler) native-kind id pointer (1- bytes)
               (cond (custom integer) ((eq kind :toggle) (if checked-p 1 0))
                     ((eq kind :selection) (or selection -1)) (t 0))
               (if can-veto-p 1 0) (if vetoed-p 1 0) (if logging-off-p 1 0)))))
(defgeneric queue-event (handler kind &key id text checked-p selection can-veto-p integer vetoed-p logging-off-p)
  (:documentation "Queue a copied supported event; queued frame-close events may invoke native closing."))
(defmethod queue-event ((handler event-handler) kind &key (id 0) (text "") checked-p selection
                         (can-veto-p t can-supplied) (integer 0 integer-supplied) (vetoed-p nil veto-supplied) (logging-off-p nil logging-supplied))
  (call-synthetic-event (lambda (&rest arguments) (check-status (apply #'%queue-event arguments)))
                        handler kind id text checked-p selection can-veto-p can-supplied
                        integer integer-supplied vetoed-p veto-supplied logging-off-p logging-supplied)
  handler)
(defgeneric process-event (handler kind &key id text checked-p selection can-veto-p integer vetoed-p logging-off-p)
  (:documentation "Synchronously dispatch a fresh supported event; return native handled and final veto values."))
(defmethod process-event ((handler event-handler) kind &key (id 0) (text "") checked-p selection
                           (can-veto-p t can-supplied) (integer 0 integer-supplied) (vetoed-p nil veto-supplied) (logging-off-p nil logging-supplied))
  (require-active-application)
  (cffi:with-foreign-object (final-veto :int)
    (let ((handled
            (let ((*in-notifying-setter* t))
              (call-synthetic-event
               (lambda (&rest arguments)
                 (boolean-result (apply #'%process-event (append arguments (list final-veto)))))
               handler kind id text checked-p selection can-veto-p can-supplied
               integer integer-supplied vetoed-p veto-supplied logging-off-p logging-supplied))))
      (check-callback-error)
      (values handled (not (zerop (cffi:mem-ref final-veto :int)))))))
(defun register-event-type (name &key (family :notify))
  "Register a unique session-local keyword for a custom COMMAND or NOTIFY type."
  (require-active-application)
  (check-type name (satisfies keywordp))
  (unless (member family '(:command :notify)) (error "Unsupported custom event family ~S" family))
  (when (or (builtin-event-kind name) (gethash name *registered-event-types*))
    (error "Event kind is already registered: ~S" name))
  (let ((code (gauge-integer-result (%register-event-type (ecase family (:command 0) (:notify 2))))))
    (setf (gethash name *registered-event-types*) code
          (gethash code *registered-event-kinds*) name
          (gethash name *registered-event-families*) family))
  name)

(defun event-type (kind)
  "Return the process-local native type integer for one supported event KIND."
  (require-active-application)
  (gauge-integer-result (%event-type (native-event-kind kind))))
(defgeneric event-context-active-p (context)
  (:documentation "Return whether CONTEXT belongs to the currently executing innermost native callback."))
(defmethod event-context-active-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-active (event-context-token context))))
(defgeneric event-skipped-p (context)
  (:documentation "Read the current borrowed event's Skip flag."))
(defmethod event-skipped-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-skipped (event-context-token context))))
(defgeneric (setf event-skipped-p) (value context)
  (:documentation "Assign native Skip and return the supplied generalized boolean."))
(defmethod (setf event-skipped-p) (value (context event-context))
  (require-active-application)
  (check-status (%context-set-skipped (event-context-token context) (if value 1 0)))
  value)
(defgeneric event-propagates-p (context)
  (:documentation "Query whether native propagation level is positive."))
(defmethod event-propagates-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-propagates (event-context-token context))))
(defgeneric stop-event-propagation (context)
  (:documentation "Set propagation level to zero and return its previous value."))
(defmethod stop-event-propagation ((context event-context))
  (require-active-application)
  (gauge-integer-result (%context-stop-propagation (event-context-token context))))
(defgeneric resume-event-propagation (context level)
  (:documentation "Restore a nonnegative propagation LEVEL and return CONTEXT."))
(defmethod resume-event-propagation ((context event-context) level)
  (require-active-application)
  (check-type level (integer 0 2147483647))
  (check-status (%context-resume-propagation (event-context-token context) level))
  context)

(defun event-family (event)
  "Read the immutable native family classification, including saved custom events."
  (ecase (event-family-code event) (0 :command) (1 :close) (2 :notify)))
(defgeneric event-can-veto-p (event)
  (:documentation "Read captured or current close/notify veto capability."))
(defmethod event-can-veto-p ((event event)) (event-can-veto event))
(defmethod event-can-veto-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-can-veto (event-context-token context))))
(defgeneric event-vetoed-p (event)
  (:documentation "Read captured veto state or current close/notify veto state."))
(defmethod event-vetoed-p ((event event)) (event-veto event))
(defmethod event-vetoed-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-vetoed (event-context-token context))))
(defgeneric (setf event-vetoed-p) (value context)
  (:documentation "Set checked close veto or notify Veto/Allow, returning the supplied boolean."))
(defmethod (setf event-vetoed-p) (value (context event-context))
  (require-active-application)
  (check-status (%context-set-vetoed (event-context-token context) (if value 1 0)))
  value)
(defgeneric close-window (frame &key force)
  (:documentation "Synchronously request frame closing and return native acceptance, not destruction."))
(defmethod close-window ((frame frame) &key force)
  (require-active-application)
  (let ((result (let ((*in-notifying-setter* t))
                  (boolean-result (%close (handle frame) (if force 1 0))))))
    (check-callback-error)
    result))

(defgeneric event-logging-off-p (event)
  (:documentation "Read session logging-off metadata; SETF requires the current session-event context."))
(defmethod event-logging-off-p ((event event))
  (unless (member (event-kind event) '(:query-end-session :end-session))
    (error "Logging-off requires a session-event snapshot"))
  (event-logging-off event))
(defmethod event-logging-off-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-logging-off (event-context-token context))))
(defgeneric (setf event-logging-off-p) (value event))
(defmethod (setf event-logging-off-p) (value (context event-context))
  (require-active-application)
  (check-status (%context-set-logging-off (event-context-token context) (if value 1 0)))
  value)

(defgeneric (setf event-can-veto-p) (value context)
  (:documentation "Assign current wxCloseEvent CanVeto metadata, returning VALUE; snapshots stay immutable."))
(defmethod (setf event-can-veto-p) (value (context event-context))
  (require-active-application)
  (check-status (%context-set-can-veto (event-context-token context) (if value 1 0)))
  value)
(defgeneric event-timestamp (event)
  (:documentation "Read captured/current native millisecond timestamp; SETF requires a current context."))
(defmethod event-timestamp ((event event)) (event-time-stamp event))
(defmethod event-timestamp ((context event-context))
  (require-active-application)
  (cffi:with-foreign-object (value :int64)
    (check-status (%context-timestamp (event-context-token context) value))
    (cffi:mem-ref value :int64)))
(defgeneric (setf event-timestamp) (value context))
(defmethod (setf event-timestamp) (value (context event-context))
  (require-active-application)
  (unless (typep value `(signed-byte ,(* 8 (cffi:foreign-type-size :long))))
    (error 'type-error :datum value :expected-type `(signed-byte ,(* 8 (cffi:foreign-type-size :long)))))
  (check-status (%context-set-timestamp (event-context-token context) value))
  value)
(defgeneric event-command-p (event)
  (:documentation "Read captured/current native IsCommandEvent, including notify-derived events."))
(defmethod event-command-p ((event event)) (event-command event))
(defmethod event-command-p ((context event-context))
  (require-active-application)
  (boolean-result (%context-command (event-context-token context))))

(defgeneric event-id (event)
  (:documentation "Read captured/current native ID; SETF changes only the current event's routing metadata."))
(defmethod event-id ((event event)) (event-identifier event))
(defmethod event-id ((context event-context))
  (require-active-application)
  (cffi:with-foreign-object (value :int)
    (check-status (%context-id (event-context-token context) value))
    (cffi:mem-ref value :int)))
(defgeneric (setf event-id) (value context))
(defmethod (setf event-id) (value (context event-context))
  (require-active-application)
  ;; Unlike binding filters/synthetic constructors, native SetId accepts -1.
  (check-type value (integer -2147483648 2147483647))
  (check-status (%context-set-id (event-context-token context) value))
  value)

(defgeneric event-native-type (event)
  (:documentation "Read captured/current process-local native event type without granting cast authority."))
(defmethod event-native-type ((event event)) (event-native-type-code event))
(defmethod event-native-type ((context event-context))
  (require-active-application)
  (cffi:with-foreign-object (value :int)
    (check-status (%context-native-type (event-context-token context) value))
    (cffi:mem-ref value :int)))
(defgeneric event-origin-id (event)
  (:documentation "Read the known native origin's opaque bridge ID, or NIL for null/untracked origins."))
(defmethod event-origin-id ((event event))
  (let ((id (event-origin-code event))) (unless (zerop id) id)))
(defmethod event-origin-id ((context event-context))
  (require-active-application)
  (cffi:with-foreign-object (value :uint64)
    (check-status (%context-origin-id (event-context-token context) value))
    (let ((id (cffi:mem-ref value :uint64))) (unless (zerop id) id))))
