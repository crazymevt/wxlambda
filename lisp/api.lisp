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

(defstruct (event (:constructor %make-event (kind source text)))
  "A Lisp-owned snapshot. Slots are read-only; TEXT is an independent string."
  (kind nil :read-only t)
  (source nil :read-only t)
  (text nil :read-only t))

(defstruct handler source function)
(defvar *active* nil)
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

(cffi:defcallback dispatch-event :void ((token :uint64) (kind :int) (text :pointer))
  ;; Copy borrowed payloads before user code, including reentrant native calls.
  ;; Unwinding through C++ is unsafe. Re-signal after the native call returns.
  (handler-case
      (unless *callback-error*
        (let ((handler (gethash token *handlers*)))
          (when handler
            (let* ((event-kind (ecase kind (1 :click) (2 :text-change)))
                   (value (when (= kind 2)
                            (when (cffi:null-pointer-p text)
                              (error "Missing text-change payload"))
                            (cffi:foreign-string-to-lisp text :encoding :utf-8)))
                   (event (%make-event event-kind (handler-source handler) value)))
              (funcall (handler-function handler) event)))))
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
  (check-status (%run))
  (check-callback-error))
(defun stop () (check-status (%stop)))
(defun make-frame (&key (title "wxlambda") (width 480) (height 240))
  (wrap-window 'frame (%frame title width height)))
(defun make-panel (parent) (wrap-window 'panel (%panel (handle parent))))
(defun make-button (parent &key (label "Button"))
  (wrap-window 'button (%button (handle parent) label)))
(defun make-text (parent &key (value ""))
  (wrap-window 'text-control (%text (handle parent) value)))
(defun make-column (parent) (check-status (%column (handle parent))) parent)
(defun add-child (parent child &key (proportion 0))
  (check-status (%add (handle parent) (handle child) proportion)) child)
(defun show-window (window) (check-status (%show (handle window))) window)
(defun destroy-window (window) (check-status (%destroy (handle window))))
(defun set-text (control value &key notify)
  "Set CONTROL's text silently, or emit a text-change event when NOTIFY is true.
Handlers may run synchronously. Their errors are re-signaled after the native
setter returns, including when RUN has not yet been called."
  (let ((*in-notifying-setter* (or notify *in-notifying-setter*)))
    (check-status (%set-text (handle control) value (if notify 1 0))))
  ;; Restore the enclosing boundary before re-signaling. An unhandled error
  ;; will reach the outer callback, which then requests the loop stop.
  (check-callback-error)
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

(defun queue-click (button) (check-status (%click (handle button))))
