;;; Load and call (wxlambda-event-metadata:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-event-metadata (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-event-metadata)
(defun exercise ()
  "Inspect and alter a synthetic close event; snapshots remain unchanged."
  (let* ((receiver (wxlambda:make-event-handler))
         (captured nil)
         (binding (wxlambda:bind-event receiver :close
                    (lambda (event context)
                      (setf captured event)
                      (assert (not (wxlambda:event-command-p context)))
                      (assert (not (wxlambda:event-can-veto-p context)))
                      ;; This explicitly changes native metadata; it is not an OS guarantee.
                      (setf (wxlambda:event-can-veto-p context) t
                            (wxlambda:event-timestamp context) 123
                            (wxlambda:event-vetoed-p context) t)
                      (assert (= 123 (wxlambda:event-timestamp context)))) :with-context t)))
    (unwind-protect
         (progn
           (assert (equal '(t t) (multiple-value-list
                     (wxlambda:process-event receiver :close :can-veto-p nil))))
           (assert (not (wxlambda:event-can-veto-p captured)))
           (assert (zerop (wxlambda:event-timestamp captured))))
      (wxlambda:unbind-event binding)
      (wxlambda:destroy-event-handler receiver))))
(defun start () (wxlambda:with-application () (exercise)))
