;;; Load this file, then call (wxlambda-live:start) from a terminal REPL.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defpackage #:wxlambda-live
  (:use #:cl)
  (:export #:start #:open-window #:close-window #:clicked #:*text* #:*button*))
(in-package #:wxlambda-live)

(defvar *frame* nil)
(defvar *text* nil)
(defvar *button* nil)
(defvar *clicks* 0)

(defun clicked ()
  (setf (wxlambda:text *text*) (format nil "Clicked ~D times" (incf *clicks*))))

(defun close-window ()
  "Release registered closures before destroying the controls they reference."
  (when *frame*
    ;; A user may already have closed the native window. Its stale handles
    ;; cannot be unbound; session cleanup releases those retained closures.
    (handler-case (wxlambda:off-click *button*)
      (wxlambda:native-error () nil))
    (handler-case (wxlambda:destroy-window *frame*)
      (wxlambda:native-error () nil))
    (setf *frame* nil *text* nil *button* nil)))

(defun open-window ()
  "Replace the current window within the active session."
  (close-window)
  (setf *frame* (wxlambda:make-frame :title "Live Lisp development")
        *text* (wxlambda:make-text *frame* :value "Redefine WXLAMBDA-LIVE:CLICKED")
        *button* (wxlambda:make-button *frame* :label "Run current CLICKED"))
  (wxlambda:make-column *frame*)
  (wxlambda:add-child *frame* *text*)
  (wxlambda:add-child *frame* *button*)
  ;; Resolve the function on each click so DEFUN changes take effect live.
  (wxlambda:on-click *button* (lambda () (clicked)))
  (wxlambda:show-window *frame*))

(defun start (&key (input *standard-input*) (output *standard-output*) (on-error :debug))
  "Enter one GUI session. Use WXLAMBDA:STOP or EOF; restart Lisp for another."
  (wxlambda:with-repl-application (:input input :output output :on-error on-error)
    (open-window)))
