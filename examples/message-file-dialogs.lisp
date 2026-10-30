;;; START is interactive; EXERCISE runs bounded automatic cancellation in a session.
;;; File/directory dialogs use the bundled private wx3.2.12 generic adapters on
;;; every port; see docs/MESSAGE-FILE-DIALOGS.md (no platform-native panel fallback).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-message-file-dialogs (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-message-file-dialogs)

(defun start ()
  (wxlambda:with-application ()
    (let ((question (wxlambda:make-message-dialog :title "Open a document"
                      :message "Choose a file to open?" :buttons :yes-no-cancel :icon :question)))
      (unwind-protect
           (when (eq :yes (wxlambda:show-modal question))
             (let ((picker (wxlambda:make-file-dialog :mode :open :title "Open a document")))
               (unwind-protect
                    (when (eq :ok (wxlambda:show-modal picker))
                      (format t "Selected: ~A~%" (wxlambda:file-dialog-path picker)))
                 (wxlambda:destroy-window picker))))
        (wxlambda:destroy-window question)
        (wxlambda:process-events)))))

(defun cancel-in-idle (dialog)
  (let ((binding nil) (cancelled nil))
    (unwind-protect
         (progn
           (setf binding
                 (wxlambda:bind-event (wxlambda:current-application-handler) :idle
                   (lambda (event)
                     (declare (ignore event))
                     (when (and (not cancelled) (wxlambda:dialog-modal-p dialog))
                       (setf cancelled t)
                       (wxlambda:end-modal dialog :cancel)))))
           (assert (eq :cancel (wxlambda:show-modal dialog)))
           (assert cancelled)
           t)
      (when binding (wxlambda:unbind-event binding)))))

(defun exercise ()
  (let ((message (wxlambda:make-message-dialog :message "Cancellation is available even without a Cancel button."
                                             :buttons :yes-no))
        (file (wxlambda:make-file-dialog :mode :open :directory (uiop:temporary-directory))))
    (unwind-protect
         (progn
           (assert (eq :yes-no (wxlambda:message-dialog-buttons message)))
           (assert (eq :open (wxlambda:file-dialog-mode file)))
           (cancel-in-idle message)
           (cancel-in-idle file)
           t)
      (wxlambda:destroy-window message)
      (wxlambda:destroy-window file)
      (wxlambda:process-events))))
