(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defvar *evaluation-condition* (make-condition 'simple-error :format-control "Evaluation diagnosis"))
(defvar *abandon-condition* (make-condition 'simple-error :format-control "Abandon diagnosis"))
(defvar *callback-condition* (make-condition 'simple-error :format-control "Callback diagnosis"))
(defvar *inside-evaluation* nil)
(defvar *inside-callback* nil)
(defvar *resumed* nil)
(defvar *after-abandon* nil)
(defvar *debugger-visits* 0)
(defvar *clicks* 0)
(defvar *button*)

(defun recoverable-evaluation ()
  (unwind-protect
       (progn
         (setf *inside-evaluation* t)
         (setf *resumed*
               (restart-case (error *evaluation-condition*)
                 (supply-test-value (value) value))))
    (setf *inside-evaluation* nil)))

(defun abandoned-evaluation ()
  (unwind-protect
       (progn
         (setf *inside-evaluation* t)
         (error *abandon-condition*)
         (error "Must not resume an abandoned form"))
    (setf *inside-evaluation* nil)))

(defun test-repl-debugger ()
 (let (#+sbcl (sb-ext:*invoke-debugger-hook* nil)
      (*debugger-hook*
        (lambda (condition previous-hook)
          (declare (ignore previous-hook))
          (incf *debugger-visits*)
          (assert (find-restart 'wxlambda:return-to-application-repl))
          (assert wxlambda::*active*)
          (assert wxlambda::*repl-debugging*)
          (cond
            ((eq condition *callback-condition*)
             ;; The callback has unwound and returned through the foreign
             ;; boundary; only the REPL's recovery restart remains.
             (assert (not *inside-callback*))
             (assert (not (find-restart 'callback-local-restart)))
             (assert (= *clicks* 1))
             (wxlambda:queue-click *button*)
             (sleep 0.03)
             (assert (= *clicks* 1))
             (assert
              (handler-case (progn (wxlambda:process-events) nil)
                (error (failure)
                  (search "paused" (princ-to-string failure)))))
             (assert (= *clicks* 1))
             (invoke-restart 'wxlambda:return-to-application-repl))
            ((eq condition *evaluation-condition*)
             (assert *inside-evaluation*)
             (assert (find-restart 'supply-test-value))
             (invoke-restart 'supply-test-value :resumed))
            ((eq condition *abandon-condition*)
             (assert *inside-evaluation*)
             (invoke-restart 'wxlambda:return-to-application-repl))
            ;; Any failed assertion above or in a form is a test failure,
            ;; rather than an automatically recovered application error.
            (t (error "Unexpected debugger condition: ~A" condition)))))
      (output (make-string-output-stream)))
  ;; Omit ON-ERROR to verify that interactive debugging is the default.
  (wxlambda:with-repl-application
      (:input (make-string-input-stream
               "(assert (= *clicks* 2))
(recoverable-evaluation)
(assert (eq *resumed* :resumed))
(abandoned-evaluation)
(setf *after-abandon* t)
(assert (not *inside-evaluation*))
(wxlambda:stop)")
       :output output)
    (let* ((frame (wxlambda:make-frame :title "REPL debugger recovery"))
           (panel (wxlambda:make-panel frame)))
      (setf *button* (wxlambda:make-button panel))
      (wxlambda:on-click *button*
        (lambda ()
          (incf *clicks*)
          (when (= *clicks* 1)
            (unwind-protect
                 (progn
                   (setf *inside-callback* t)
                   (restart-case (error *callback-condition*)
                     (callback-local-restart () nil)))
              (setf *inside-callback* nil)))))
      (wxlambda:show-window frame)
      (wxlambda:queue-click *button*)))
  (assert (= *debugger-visits* 3))
  (assert (= *clicks* 2))
  (assert (eq *resumed* :resumed))
  (assert *after-abandon*)
  (assert (not *inside-evaluation*))
  (assert (not wxlambda::*active*))
  (assert (not wxlambda::*repl-debugging*))))

;; CCL's application adapter does not transfer dynamic bindings. Establish the
;; test hook on the GUI thread, just as a terminal debugger would be configured.
#+(and ccl darwin)
(let ((failure nil))
  (ccl::call-in-initial-process
   (lambda ()
     (handler-case (test-repl-debugger)
       (error (condition) (setf failure condition)))))
  (when failure (error failure)))
#-(and ccl darwin) (test-repl-debugger)
(format t "PASS: original conditions, evaluation restarts, abandon recovery, safe callback boundary, paused GUI~%")
