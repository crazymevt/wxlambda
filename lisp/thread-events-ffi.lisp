(in-package #:wxlambda)
(cffi:defcfun ("wxl_queue_thread_event" %queue-thread-event) :int
  (receiver :uint64) (text :pointer) (length :size))
