(defpackage #:wxlambda
  (:use #:cl)
  (:export #:load-native-library #:call-with-application #:with-application
           #:run #:stop #:make-frame #:make-panel #:make-button #:make-text
           #:make-column #:add-child #:show-window #:destroy-window
           #:set-text #:on-click #:queue-click #:native-error))
