(defpackage #:wxlambda
  (:use #:cl)
  (:export #:load-native-library #:call-with-application #:with-application
           #:run #:stop #:make-frame #:make-panel #:make-button #:make-text
           #:make-column #:make-row #:add-child #:add-spacer #:add-stretch
           #:layout-window #:window-position #:window-size #:show-window #:destroy-window
           #:set-text #:get-text #:on-click #:off-click #:queue-click #:native-error
           #:on-text-change #:off-text-change
           #:make-label #:get-label #:set-label
           #:make-checkbox #:checked-p #:set-checked #:on-toggle #:off-toggle
           #:event #:event-p #:event-kind #:event-source #:event-text #:event-checked-p))
