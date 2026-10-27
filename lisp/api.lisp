(in-package #:wxlambda)

(define-condition native-error (error)
  ((message :initarg :message :reader native-error-message))
  (:report (lambda (condition stream) (write-string (native-error-message condition) stream))))

(defclass window () ((handle :initarg :handle :reader handle)))
(defclass frame (window) ())
(defclass panel (window) ())
(defclass button (window) ())
(defclass text-control (window) ())
(defvar *active* nil)
(defvar *handlers* nil)
(defvar *callback-error* nil)
(defvar *next-token* 0)

(defun check-status (status)
  (unless (zerop status) (error 'native-error :message (%last-error)))
  t)
(defun wrap-window (class id)
  (when (zerop id) (error 'native-error :message (%last-error)))
  (make-instance class :handle id))

(cffi:defcallback dispatch-event :void ((token :uint64))
  ;; Unwinding through C++ is unsafe. Re-signal after the native loop returns.
  (handler-case
      (let ((handler (gethash token *handlers*)))
        (when handler (funcall handler)))
    (error (condition)
      (unless *callback-error* (setf *callback-error* condition))
      (%stop))))

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
  (when *callback-error* (error *callback-error*)))
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
(defun set-text (control value) (check-status (%set-text (handle control) value)) value)
(defun on-click (button function)
  "Register a no-argument FUNCTION. Replaces this button's previous handler.
Closures remain rooted until the application session ends."
  (unless *active* (error "No active application session"))
  (check-type function function)
  (let ((token (incf *next-token*)))
    (check-status (%on-click (handle button) token))
    (setf (gethash token *handlers*) function))
  button)
(defun queue-click (button) (check-status (%click (handle button))))
