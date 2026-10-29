(in-package #:wxlambda)
(cffi:defcfun ("wxl_input_detail" %input-detail) :int (token :uint64) (index :int) (value :pointer))
(cffi:defcfun ("wxl_input_allow_next" %input-allow-next) :int (token :uint64))
(cffi:defcfun ("wxl_input_next_allowed" %input-next-allowed) :int (token :uint64))
