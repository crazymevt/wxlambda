(in-package #:wxlambda)
(cffi:defcfun ("wxl_activity_indicator" %activity-indicator) :uint64 (parent :uint64) (running :int))
(cffi:defcfun ("wxl_activity_running" %activity-running) :int (control :uint64))
(cffi:defcfun ("wxl_set_activity_running" %set-activity-running) :int (control :uint64) (running :int))
