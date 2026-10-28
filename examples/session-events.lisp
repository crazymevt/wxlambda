;;; Synthetic policy exercise only: never emits OS session events on wxApp.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-session-events (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-session-events)
(defun exercise ()
  (let* ((receiver (wxlambda:make-event-handler))
         (binding (wxlambda:bind-event receiver :query-end-session
                    (lambda (event context)
                      (assert (wxlambda:event-logging-off-p event))
                      (when (wxlambda:event-can-veto-p context)
                        (setf (wxlambda:event-vetoed-p context) t))) :with-context t)))
    (unwind-protect
         (assert (equal '(t t) (multiple-value-list
                   (wxlambda:process-event receiver :query-end-session :logging-off-p t))))
      (wxlambda:unbind-event binding)
      (wxlambda:destroy-event-handler receiver))))
(defun start () (wxlambda:with-application () (exercise)))
