(load (merge-pathnames "../examples/application-events.lisp" *load-truename*))

(defun expect-native-error (thunk)
  (assert (handler-case (progn (funcall thunk) nil)
            (wxlambda:native-error () t))))

(assert (not wxlambda::*native-loaded*))
(dolist (operation '(wxlambda:main-loop-running-p wxlambda:pending-events-p
                     wxlambda:process-pending-events))
  ;; Normal fresh-ASDF-load calls must not require resolved foreign symbols.
  (expect-native-error operation))
(assert (not wxlambda::*native-loaded*))
(wxlambda:load-native-library)
(dolist (operation (list #'wxlambda::%main-loop-running #'wxlambda::%pending-events
                         #'wxlambda::%process-pending-events))
  (assert (= -1 (funcall operation))))

(wxlambda:with-application ()
  (wxlambda-application-events:exercise)
  (let* ((frame (wxlambda:make-frame))
         (button (wxlambda:make-button frame))
         (text (wxlambda:make-text frame))
         (calls 0)
         (expected-loop nil))
    (flet ((check-boundaries ()
             (incf calls)
             (assert (eql expected-loop (wxlambda:main-loop-running-p)))
             (assert (typep (wxlambda:pending-events-p) 'boolean))
             (dolist (operation '(wxlambda:run wxlambda:process-events
                                  wxlambda:process-pending-events))
               (expect-native-error operation))
             ;; Raw bridge entry points must enforce the same boundary.
             (dolist (operation (list #'wxlambda::%run #'wxlambda::%poll
                                      #'wxlambda::%process-pending-events #'wxlambda::%finish))
               (assert (= -1 (funcall operation))))))
      ;; Synchronous command callbacks before any pump may not enter dispatch.
      (wxlambda:on-text-change text
        (lambda (event) (declare (ignore event)) (check-boundaries)))
      (setf (wxlambda:text text :notify t) "before pumping")
      (assert (= 1 calls))
      (wxlambda:on-click button #'check-boundaries)
      (wxlambda:queue-click button)
      (assert (wxlambda:pending-events-p))
      ;; Debugger pauses must leave queued callbacks untouched.
      (let ((wxlambda::*repl-debugging* t))
        (assert (handler-case (progn (wxlambda:process-pending-events) nil)
                  (error () t))))
      (assert (= 1 calls))
      (wxlambda:process-pending-events)
      (assert (= 2 calls))
      (wxlambda:queue-click button)
      (wxlambda:process-events)
      (assert (= 3 calls))
      (setf expected-loop t)
      (wxlambda:on-click button (lambda () (check-boundaries) (wxlambda:stop)))
      (wxlambda:queue-click button)
      (wxlambda:run)
      (assert (= 4 calls)))
    ;; Escaping callback errors are re-signaled after returning from native code.
    (let ((failure (make-condition 'simple-error :format-control "pending callback failure")))
      (wxlambda:on-click button (lambda () (error failure)))
      (wxlambda:queue-click button)
      (assert (handler-case (progn (wxlambda:process-pending-events) nil)
                (error (condition) (eq failure condition)))))
    ;; The guard unwinds on callback failure; subsequent queued dispatch recovers.
    (wxlambda:on-click button (lambda () (incf calls)))
    (wxlambda:queue-click button)
    (wxlambda:process-pending-events)
    (assert (= 5 calls))
    (assert (not (wxlambda:main-loop-running-p)))
    (wxlambda:off-text-change text)
    (wxlambda:off-click button)
    (wxlambda:destroy-window frame)))

(dolist (operation '(wxlambda:main-loop-running-p wxlambda:pending-events-p
                     wxlambda:process-pending-events))
  (expect-native-error operation))
(format t "PASS: application loop/queue status, pending dispatch, callback/pump guards, error recovery and example~%")
