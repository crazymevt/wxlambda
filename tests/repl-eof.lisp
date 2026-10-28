(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(wxlambda:with-repl-application
    (:input (make-string-input-stream "") :output (make-string-output-stream))
  (wxlambda:show-window (wxlambda:make-frame :title "EOF cleanup")))
(assert (not wxlambda::*active*))
(format t "PASS: REPL EOF cleanup~%")
