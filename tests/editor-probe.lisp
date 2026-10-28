;;; Optional Swank experiment, driven by editor-probe.py with Quicklisp loaded.
(require :asdf)
(ql:quickload :swank :silent t)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defvar *probe-launch-thread* #+sbcl sb-thread:*current-thread* #+ccl ccl:*current-process*)
(defvar *probe-binding* :unset)

(defun probe-main-p ()
  #+sbcl (eq sb-thread:*current-thread* *probe-launch-thread*)
  #+ccl (eq ccl:*current-process* ccl::*initial-process*))

(defun editor-gui-probe ()
  (let ((caller-main (probe-main-p))
        (body-main nil) (callback-main nil) (binding-seen nil)
        (*probe-binding* :caller))
    #+sbcl
    (assert (eq caller-main
                (not (string= (uiop:getenv "WXLAMBDA_SWANK_STYLE") ":spawn"))))
    #+sbcl
    (unless caller-main
      (assert (handler-case
                  (progn (wxlambda:with-application () nil) nil)
                (wxlambda:native-error (condition)
                  (search "main thread" (princ-to-string condition)))))
      (return-from editor-gui-probe "PROBE-PASS worker GUI startup rejected"))
    (wxlambda:with-application ()
      (setf body-main (probe-main-p)
            binding-seen *probe-binding*)
      (let* ((frame (wxlambda:make-frame :title "Swank thread probe"))
             (button (wxlambda:make-button frame)))
        (wxlambda:on-click button
          (lambda ()
            (setf callback-main (probe-main-p))
            (wxlambda:off-click button)
            (wxlambda:destroy-window frame)))
        (wxlambda:show-window frame)
        (wxlambda:queue-click button)
        (wxlambda:run)))
    (assert body-main)
    (assert callback-main)
    (assert (eq binding-seen (if caller-main :caller :unset)))
    (format nil "PROBE-PASS caller-main=~S body-main=~S callback-main=~S binding=~S"
            caller-main body-main callback-main binding-seen)))

(let* ((style (read-from-string (uiop:getenv "WXLAMBDA_SWANK_STYLE")))
       (swank:*communication-style* style))
  ;; Internal API used only by this experiment: announce before NIL style
  ;; blocks accepting requests. Bind solely to loopback and an ephemeral port.
  (let ((swank::*loopback-interface* "127.0.0.1"))
    (swank::setup-server
     0 (lambda (port)
         (with-open-file (stream (uiop:getenv "WXLAMBDA_SWANK_PORT_FILE")
                                 :direction :output :if-exists :supersede)
           (format stream "~D" port)))
     ;; SPAWN must keep its listener off the main thread so this experiment's
     ;; main-thread loop can observe the stop file. NIL/FD-HANDLER are one-shot.
     style (eq style :spawn) nil))
  (loop until (probe-file (uiop:getenv "WXLAMBDA_SWANK_STOP_FILE")) do
    #+sbcl (sb-sys:serve-all-events 0.01)
    #+ccl (sleep 0.01)))
