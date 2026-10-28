;;; Load and call (wxlambda-standard-dialogs:start) for an interactive prompt.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-standard-dialogs (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-standard-dialogs)

(defun start ()
  (wxlambda:with-application ()
    (let ((dialog (wxlambda:make-text-entry-dialog :message "Document name:" :value "Untitled")))
      (unwind-protect
           (when (eq :ok (wxlambda:show-modal dialog))
             (format t "Document name: ~A~%" (wxlambda:dialog-value dialog)))
        (wxlambda:destroy-window dialog)
        (wxlambda:process-events)))))

(defun exercise ()
  ;; A deterministic cancellation example: real native modal loop, no user input.
  (let* ((dialog (wxlambda:make-text-entry-dialog :message "Bounded modal example" :value "Draft"))
         (binding nil) (closed nil))
    (unwind-protect
         (progn
           (setf binding
                 (wxlambda:bind-event (wxlambda:current-application-handler) :idle
                   (lambda (event)
                     (declare (ignore event))
                     (when (and (not closed) (wxlambda:dialog-modal-p dialog))
                       (setf closed t)
                       (wxlambda:end-modal dialog :cancel)))))
           (assert (eq :cancel (wxlambda:show-modal dialog)))
           (assert closed)
           (assert (string= "Draft" (wxlambda:dialog-value dialog)))
           t)
      (when binding (wxlambda:unbind-event binding))
      (wxlambda:destroy-window dialog)
      (wxlambda:process-events))))
