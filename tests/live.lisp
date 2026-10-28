(load (merge-pathnames "../examples/live.lisp" *load-truename*))

(defvar *live-original-button* nil)
(defvar *live-original-clicked* (symbol-function 'wxlambda-live:clicked))
(defvar *live-verified* nil)
(defvar *live-output* (make-string-output-stream))

(unwind-protect
     (wxlambda-live:start
      :input (make-string-input-stream
              "(setf *live-original-button* wxlambda-live:*button*)
(wxlambda:set-text wxlambda-live:*text* \"Changed without restarting\")
(assert (string= (wxlambda:get-text wxlambda-live:*text*) \"Changed without restarting\"))
(wxlambda:queue-click wxlambda-live:*button*)
(assert (string= (wxlambda:get-text wxlambda-live:*text*) \"Clicked 1 times\"))
(defun wxlambda-live:clicked () (wxlambda:set-text wxlambda-live:*text* \"Redefined live\"))
(wxlambda:queue-click wxlambda-live:*button*)
(assert (string= (wxlambda:get-text wxlambda-live:*text*) \"Redefined live\"))
(dotimes (i 50) (wxlambda:off-click wxlambda-live:*button*) (assert (zerop (hash-table-count wxlambda::*handlers*))) (wxlambda:on-click wxlambda-live:*button* (lambda () (wxlambda-live:clicked))) (assert (= 1 (hash-table-count wxlambda::*handlers*))))
(wxlambda-live:close-window)
(assert (zerop (hash-table-count wxlambda::*handlers*)))
(assert (handler-case (progn (wxlambda:queue-click *live-original-button*) nil) (wxlambda:native-error () t)))
(wxlambda-live:open-window)
(assert (= 1 (hash-table-count wxlambda::*handlers*)))
(wxlambda:queue-click wxlambda-live:*button*)
(assert (string= (wxlambda:get-text wxlambda-live:*text*) \"Redefined live\"))
(wxlambda-live:close-window)
(assert (zerop (hash-table-count wxlambda::*handlers*)))
(setf *live-verified* t)
(wxlambda:stop)")
      :output *live-output*
      :on-error :report)
  (setf (symbol-function 'wxlambda-live:clicked) *live-original-clicked*))
;; Every error is unexpected in this scenario; REPORT keeps the batch process
;; usable but its output must not hide a failed assertion.
(let ((printed (get-output-stream-string *live-output*)))
  (assert (not (search "Backtrace (up to" printed)) ()
          "Unexpected application REPL error:~%~A" printed))
(assert *live-verified*)
(assert (not wxlambda::*active*))
(format t "PASS: live text updates, callback redefinition, repeated unbinding, window recreation~%")
