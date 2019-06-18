;; Model M1 from PNAS March 11, 2014 111 (10) 3883-3888; https://doi.org/10.1073/pnas.1317072111
(define-model maclean2014-m1
  (variable t :independent)
  (variable x_0 :default 0.5)
  (variable x_1)
  (variable x_2)
  (variable y_0 :default 0.5)
  (variable y_1)
  (variable z)
  (parameter a_x :default 0.1)
  (parameter a_y :default 0.9)
  (parameter b_x :default 0.8)
  (parameter b_y :default 0.4)
  (parameter c_x :default 0.1)
  (parameter d_x :default 0.9)
  (parameter e_x :default 0.25)
  (parameter e_y :default 0.8)
  (parameter k :default 1)
  (eq (diff (bvar t) x_0)
      (minus (times a_x x_0 (minus k z))
             (times b_x x_0)))
  (eq (diff (bvar t) x_1)
      (minus (plus (times b_x x_0)
                   (times c_x x_1 (minus k z)))
             (times d_x x_1)))
  (eq (diff (bvar t) x_2)
      (minus (times d_x x_1)
             (times e_x x_2)))
  (eq (diff (bvar t) y_0)
      (minus (times a_y y_0 (minus k z))
             (times b_y y_0)))
  (eq (diff (bvar t) y_1)
      (minus (times b_y y_0)
             (times e_y y_1)))
  (eq z
      (plus x_0 x_1 x_2 y_0 y_1)))