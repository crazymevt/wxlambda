(load (merge-pathnames "../examples/pending-events.lisp" *load-truename*))
(defun expect-error (thunk &optional (type 'error))
  (assert (handler-case (progn (funcall thunk) nil)
            (error (condition) (typep condition type)))))

(defun check-post-copy-and-transfer ()
  (let* ((receiver (wxlambda:make-event-handler))
         (text (concatenate 'string "λ" (string (code-char 0)) "posted"))
         (source (wxlambda:make-event :kind :pending-test-command :id 42 :text text :integer 7))
         (snapshots nil)
         (binding (wxlambda:bind-event receiver :pending-test-command
                    (lambda (event) (push event snapshots)))))
    (unwind-protect
         (progn
           (setf (wxlambda:event-timestamp source) -1)
           (assert (eq receiver (wxlambda:post-event receiver source)))
           (assert (wxlambda:event-alive-p source))
           (setf (wxlambda:event-text source) "mutated after post"
                 (wxlambda:event-integer source) 99 (wxlambda:event-id source) 77)
           (wxlambda:destroy-event source) (setf source nil)
           (wxlambda:process-pending-events)
           (assert (= 1 (length snapshots)))
           (let ((snapshot (first snapshots)))
             (assert (string= text (wxlambda:event-text snapshot)))
             (assert (= 42 (wxlambda:event-id snapshot)))
             (assert (= 7 (wxlambda:event-integer snapshot)))
             (assert (= -1 (wxlambda:event-timestamp snapshot)))
             (assert (= (wxlambda::handle receiver) (wxlambda:event-origin-id snapshot)))
             (dolist (operation (list #'wxlambda:post-event #'wxlambda:transfer-event
                                      #'wxlambda:process-event-locally))
               (expect-error (lambda () (funcall operation receiver snapshot)))))
           (setf source (wxlambda:make-event :kind :pending-test-command :id 9 :text "transferred" :integer -3))
           (assert (eq receiver (wxlambda:transfer-event receiver source)))
           (assert (not (wxlambda:event-alive-p source)))
           (expect-error (lambda () (wxlambda:event-text source)) 'wxlambda:native-error)
           (expect-error (lambda () (wxlambda:clone-event source)) 'wxlambda:native-error)
           (expect-error (lambda () (wxlambda:transfer-event receiver source)) 'wxlambda:native-error)
           (expect-error (lambda () (wxlambda:destroy-event source)) 'wxlambda:native-error)
           (wxlambda:process-pending-events)
           (assert (= 2 (length snapshots)))
           (assert (string= "transferred" (wxlambda:event-text (first snapshots))))
           (assert (= -3 (wxlambda:event-integer (first snapshots))))
           (setf source nil))
      (when (and source (wxlambda:event-alive-p source)) (wxlambda:destroy-event source))
      (wxlambda:unbind-event binding) (wxlambda:destroy-event-handler receiver))))

(defun check-pending-deletion ()
  (let* ((receiver (wxlambda:make-event-handler))
         (source (wxlambda:make-event :kind :pending-test-command)) (calls 0)
         (binding (wxlambda:bind-event receiver :pending-test-command
                    (lambda (event) (declare (ignore event)) (incf calls)))))
    (unwind-protect
         (progn
           (wxlambda:post-event receiver source)
           (wxlambda:queue-event receiver source)
           (let ((moved (wxlambda:clone-event source)))
             (wxlambda:transfer-event receiver moved)
             (assert (not (wxlambda:event-alive-p moved))))
           (assert (wxlambda:delete-pending-events receiver))
           (assert (wxlambda:binding-active-p binding))
           ;; wxApp must no longer retain the now-empty pending receiver.
           (wxlambda:process-pending-events)
           (assert (zerop calls))
           (assert (wxlambda:delete-pending-events receiver))
           (wxlambda:post-event receiver source)
           (wxlambda:process-pending-events)
           (assert (= 1 calls))
           (expect-error (lambda () (wxlambda:delete-pending-events
                                     (wxlambda:current-application-handler))) 'wxlambda:native-error)
           (expect-error (lambda () (wxlambda:delete-pending-events source))))
      (wxlambda:unbind-event binding) (wxlambda:destroy-event source)
      (wxlambda:destroy-event-handler receiver)))
  ;; Ownership moved to a destination that dies before queue processing.
  (let* ((receiver (wxlambda:make-event-handler))
         (source (wxlambda:make-event :kind :pending-test-notify)))
    (wxlambda:transfer-event receiver source)
    (assert (not (wxlambda:event-alive-p source)))
    (wxlambda:destroy-event-handler receiver)
    (wxlambda:process-pending-events)))

(defun check-local-routing ()
  (let* ((frame (wxlambda:make-frame)) (panel (wxlambda:make-panel frame))
         (child (wxlambda:make-button panel))
         (source (wxlambda:make-event :kind :pending-test-command :text "original" :integer 5))
         (child-calls 0) (parent-calls 0) (app-calls 0)
         (child-binding (wxlambda:bind-event child :pending-test-command
                          (lambda (event) (declare (ignore event)) (incf child-calls))))
         (parent-binding (wxlambda:bind-event panel :pending-test-command
                           (lambda (event) (declare (ignore event)) (incf parent-calls))))
         (app-binding (wxlambda:bind-event (wxlambda:current-application-handler) :pending-test-command
                        (lambda (event) (declare (ignore event)) (incf app-calls)))))
    (unwind-protect
         (progn
           (assert (equal '(nil nil) (multiple-value-list
                      (wxlambda:process-event-locally child source))))
           (assert (equal '(1 0 0) (list child-calls parent-calls app-calls)))
           (wxlambda:process-event child source)
           (assert (equal '(2 1 1) (list child-calls parent-calls app-calls)))
           (wxlambda:unbind-event child-binding)
           (setf child-binding
                 (wxlambda:bind-event child :pending-test-command
                   (lambda (event context)
                     (declare (ignore event))
                     (incf child-calls)
                     (setf (wxlambda:event-text context) "local dispatch copy"
                           (wxlambda:event-integer context) 91)
                     (expect-error (lambda () (wxlambda:delete-pending-events child)) 'wxlambda:native-error)
                     (dolist (operation (list #'wxlambda:post-event #'wxlambda:transfer-event
                                              #'wxlambda:process-event-locally))
                       (expect-error (lambda () (funcall operation child context))))) :with-context t))
           (assert (equal '(t nil) (multiple-value-list
                      (wxlambda:process-event-locally child source))))
           (assert (equal '(3 1 1) (list child-calls parent-calls app-calls)))
           (assert (string= "original" (wxlambda:event-text source)))
           (assert (= 5 (wxlambda:event-integer source))))
      (wxlambda:unbind-event child-binding) (wxlambda:unbind-event parent-binding)
      (wxlambda:unbind-event app-binding) (wxlambda:destroy-event source)
      (wxlambda:destroy-window frame)))
  (let* ((receiver (wxlambda:make-event-handler))
         (source (wxlambda:make-event :kind :pending-test-notify))
         (binding (wxlambda:bind-event receiver :pending-test-notify
                    (lambda (event context)
                      (declare (ignore event))
                      (setf (wxlambda:event-vetoed-p context) t)) :with-context t)))
    (unwind-protect
         (progn
           (assert (equal '(t t) (multiple-value-list
                      (wxlambda:process-event-locally receiver source))))
           (assert (not (wxlambda:event-vetoed-p source)))
           (wxlambda:unbind-event binding)
           (setf binding (wxlambda:bind-event receiver :pending-test-notify
                           (lambda (event) (declare (ignore event)))))
           (setf (wxlambda:event-vetoed-p source) t)
           (assert (equal '(nil t) (multiple-value-list
                      (wxlambda:process-event-locally receiver source)))))
      (wxlambda:unbind-event binding) (wxlambda:destroy-event source)
      (wxlambda:destroy-event-handler receiver))))

(defun check-failed-transfer-preserves-source ()
  (let* ((dead (wxlambda:make-event-handler))
         (frame (wxlambda:make-frame)) (panel (wxlambda:make-panel frame))
         (text (wxlambda:make-text panel))
         (source (wxlambda:make-event :kind :pending-test-command :text "still owned")))
    (wxlambda:destroy-event-handler dead)
    (unwind-protect
         (progn
           (expect-error (lambda () (wxlambda:transfer-event dead source)) 'wxlambda:native-error)
           (assert (wxlambda:event-alive-p source))
           (assert (string= "still owned" (wxlambda:event-text source)))
           (dolist (kind '(:query-end-session :end-session))
             (let ((session (wxlambda:make-event :kind kind :can-veto-p nil :logging-off-p t)))
               (unwind-protect
                    (progn
                      (dolist (destination (list frame (wxlambda:current-application-handler)))
                        (dolist (operation (list #'wxlambda:post-event #'wxlambda:transfer-event
                                                 #'wxlambda:process-event-locally))
                          (expect-error (lambda () (funcall operation destination session)) 'wxlambda:native-error)
                          (assert (wxlambda:event-alive-p session))))
                      (let ((receiver (wxlambda:make-event-handler)))
                        (unwind-protect
                             (progn
                               (assert (equal '(nil nil) (multiple-value-list
                                          (wxlambda:process-event-locally receiver session))))
                               (wxlambda:post-event receiver session)
                               (assert (wxlambda:event-alive-p session))
                               (wxlambda:transfer-event receiver session)
                               (assert (not (wxlambda:event-alive-p session)))
                               (wxlambda:process-pending-events))
                          (wxlambda:destroy-event-handler receiver))))
                 (when (wxlambda:event-alive-p session) (wxlambda:destroy-event session)))))
           (let ((lazy (wxlambda:make-event :kind :text-change :text "not text-entry input")))
             (unwind-protect
                  (dolist (operation (list #'wxlambda:post-event #'wxlambda:transfer-event
                                           #'wxlambda:process-event-locally))
                    (expect-error (lambda () (funcall operation text lazy)) 'wxlambda:native-error)
                    (assert (wxlambda:event-alive-p lazy)))
               (wxlambda:destroy-event lazy))))
      (wxlambda:destroy-event source) (wxlambda:destroy-window frame))))

(defun check-all-pending-families ()
  (let ((receiver (wxlambda:make-event-handler)))
    (unwind-protect
         (dolist (kind '(:pending-test-command :pending-test-notify :close :query-end-session :end-session))
           (let* ((source (wxlambda:make-event :kind kind)) (calls 0)
                  (binding (wxlambda:bind-event receiver kind
                             (lambda (snapshot context)
                               (incf calls)
                               (assert (eq kind (wxlambda:event-kind snapshot)))
                               (setf (wxlambda:event-id context) 42)) :with-context t)))
             (unwind-protect
                  (progn
                    (assert (equal '(t nil) (multiple-value-list
                               (wxlambda:process-event-locally receiver source))))
                    (assert (zerop (wxlambda:event-id source)))
                    (wxlambda:post-event receiver source)
                    (assert (wxlambda:event-alive-p source))
                    (let ((moved (wxlambda:clone-event source)))
                      (wxlambda:transfer-event receiver moved)
                      (assert (not (wxlambda:event-alive-p moved))))
                    (wxlambda:process-pending-events)
                    (assert (= 3 calls)))
               (wxlambda:unbind-event binding) (wxlambda:destroy-event source))))
      (wxlambda:destroy-event-handler receiver))))

(defun check-local-errors-and-run-guard ()
  (let* ((frame (wxlambda:make-frame))
         (receiver (wxlambda:make-event-handler))
         (source (wxlambda:make-event :kind :pending-test-notify))
         (condition (make-condition 'simple-error :format-control "local copy callback failure"))
         (continued 0)
         (inner (wxlambda:bind-event receiver :pending-test-notify
                  (lambda (snapshot context)
                    (declare (ignore snapshot context)) (error condition)) :with-context t))
         (binding (wxlambda:bind-event receiver :pending-test-command
                    (lambda (snapshot context)
                      (declare (ignore snapshot context))
                      (expect-error (lambda () (wxlambda:delete-pending-events receiver)) 'wxlambda:native-error)
                      (assert (eq condition (handler-case
                                             (wxlambda:process-event-locally receiver source)
                                             (error (caught) caught))))
                      (incf continued)
                      (if (= continued 2) (wxlambda:stop)
                          (wxlambda:queue-event receiver :pending-test-command))) :with-context t)))
    (unwind-protect
         (progn
           (assert (eq condition (handler-case
                                  (wxlambda:process-event-locally receiver source)
                                  (error (caught) caught))))
           (assert (wxlambda:event-alive-p source))
           (wxlambda:show-window frame)
           (wxlambda:queue-event receiver :pending-test-command)
           (wxlambda:run)
           (assert (= 2 continued)))
      (wxlambda:unbind-event binding) (wxlambda:unbind-event inner)
      (wxlambda:destroy-event source) (wxlambda:destroy-event-handler receiver)
      (wxlambda:destroy-window frame))))

(let (escaped escaped-receiver)
  (wxlambda:with-application ()
    (wxlambda-pending-events:exercise)
    (wxlambda:register-event-type :pending-test-command :family :command)
    (wxlambda:register-event-type :pending-test-notify)
    (check-post-copy-and-transfer)
    (check-pending-deletion)
    (check-local-routing)
    (check-failed-transfer-preserves-source)
    (check-local-errors-and-run-guard)
    (check-all-pending-families)
    (setf escaped (wxlambda:make-event :kind :pending-test-command)
          escaped-receiver (wxlambda:make-event-handler)))
  (expect-error (lambda () (wxlambda:transfer-event escaped-receiver escaped))
                'wxlambda:native-error)
  (expect-error (lambda () (wxlambda:clone-event escaped)) 'wxlambda:native-error))
(format t "PASS: posting copies, consumed queue transfer, pending deletion, local dispatch and error/lifetime guards~%")
