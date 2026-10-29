(in-package #:wxlambda)
(pushnew '(120 . :thread) *extension-event-kinds* :test #'equal)
(defmethod event-text :around ((context event-context))
  ;; Native wxThreadEvent has text but IsCommandEvent is false. Preserve the
  ;; generic reader's old command-only behavior for every other event kind.
  (if (eq (event-kind context) :thread)
      (cffi:with-foreign-object (length :size)
        (let ((pointer (%event-text (event-context-token context) 1 length)))
          (when (cffi:null-pointer-p pointer) (error 'native-error :message (%last-error)))
          (cffi:foreign-string-to-lisp pointer :count (cffi:mem-ref length :size) :encoding :utf-8)))
      (call-next-method)))
(defgeneric queue-thread-event (receiver payload)
  (:documentation "Copy a UTF-8 payload into a native wxThreadEvent for a GUI-registered receiver; callable from worker threads."))
(defmethod queue-thread-event ((receiver event-handler) payload)
  (check-type payload string)
  ;; No require-active-application or callback-error inspection: workers must not
  ;; read ordinary GUI/session registries or invoke a Lisp GUI callback.
  (cffi:with-foreign-string ((pointer length) payload :encoding :utf-8)
    (check-status (%queue-thread-event (handle receiver) pointer (1- length))))
  receiver)
