;;; Ordinary startup: load this file to run the form until its window closes.
(load (merge-pathnames "form-app.lisp" *load-truename*))
(wxlambda-form:start)
