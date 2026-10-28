(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

;; A timed Gray stream models a terminal whose next input has not arrived yet.
;; No worker thread calls wxWidgets. Blocking READ-CHAR is allowed for forms,
;; but must never be entered while only whitespace has arrived.
(defpackage #:wxlambda-delayed-test
  (:use #:cl #+sbcl #:sb-gray #+ccl #:gray))
(in-package #:wxlambda-delayed-test)

(defclass delayed-input (fundamental-character-input-stream)
  ((chunks :initarg :chunks :accessor chunks)
   (start :initform (get-internal-real-time) :reader start)
   (current :initform nil :accessor current)
   (pushback :initform nil :accessor pushback)
   (waiting-hook :initarg :waiting-hook :accessor waiting-hook)
   (blocking-reads :initform 0 :accessor blocking-reads)))

(defmethod stream-read-char-no-hang ((stream delayed-input))
  (when (pushback stream)
    (return-from stream-read-char-no-hang (pop (pushback stream))))
  (loop
    (when (current stream)
      (let ((character (read-char (current stream) nil :eof)))
        (unless (eq character :eof)
          (return character))
        (setf (current stream) nil)))
    (unless (chunks stream) (return :eof))
    (let ((elapsed (/ (- (get-internal-real-time) (start stream))
                      internal-time-units-per-second)))
      (when (< elapsed (caar (chunks stream)))
        (when (waiting-hook stream)
          (funcall (waiting-hook stream))
          (setf (waiting-hook stream) nil))
        (return nil)))
    (setf (current stream) (make-string-input-stream (second (pop (chunks stream)))))))

(defmethod stream-read-char ((stream delayed-input))
  (or (stream-read-char-no-hang stream)
      (progn
        (incf (blocking-reads stream))
        (loop for character = (stream-read-char-no-hang stream)
              when character return character
              do (sleep 0.005)))))

(defmethod stream-unread-char ((stream delayed-input) character)
  (push character (pushback stream))
  nil)

(defvar *button*)
(defvar *calls* 0)
(defvar *responsive* nil)
(defvar *recovered* nil)
(let* ((output (make-string-output-stream))
       (input nil))
  (wxlambda:with-repl-application (:input input :output output :on-error :report)
    (let* ((frame (wxlambda:make-frame :title "Delayed REPL input"))
           (panel (wxlambda:make-panel frame)))
      (setf *button* (wxlambda:make-button panel))
      (wxlambda:on-click *button*
        (lambda ()
          (incf *calls*)
          (when (= *calls* 1) (error "Delayed callback recovery"))))
      (wxlambda:show-window frame)
      (setf input
            (make-instance 'delayed-input
              :waiting-hook (lambda () (wxlambda:queue-click *button*))
              :chunks
              (list (list 0 (format nil " ~C~C~C" #\Tab #\Return #\Newline))
                    ;; The event queued while input is unavailable must run
                    ;; before this first form, without waiting in READ.
                    (list 0.25 "(progn (assert (= wxlambda-delayed-test::*calls* 1)) (setf wxlambda-delayed-test::*responsive* t)) ")
                    (list 0.35 "(error \"Delayed evaluation recovery\") ")
                    (list 0.45 "(wxlambda:queue-click wxlambda-delayed-test::*button*) ")
                    (list 0.55 "(progn (assert (= wxlambda-delayed-test::*calls* 2)) (setf wxlambda-delayed-test::*recovered* t)) :recovered ")
                    ;; Delayed trailing whitespace and EOF must clean up too.
                    (list 0.65 (format nil " ~C~C " #\Return #\Newline)))))))
  (assert (= *calls* 2))
  ;; Assertions inside the REPL are recoverable; require their success outside
  ;; it so a regression cannot be printed and then mistaken for a passing test.
  (assert *responsive*)
  (assert *recovered*)
  (assert (zerop (blocking-reads input)))
  (assert (not wxlambda::*active*))
  (let ((printed (get-output-stream-string output)))
    (assert (search "Delayed callback recovery" printed))
    (assert (search "Delayed evaluation recovery" printed))
    (assert (search ":RECOVERED" printed))))
(format t "PASS: delayed whitespace/CRLF, GUI dispatch, separate error recovery, EOF cleanup~%")
