Require Export ZArith.
Require Export List.
Require Export Arith.

Inductive bin : Set := node : bin->bin->bin | leaf : nat->bin.
Section assoc_eq.

Variables (A : Set)(f : A->A->A)
  (assoc : forall x y z:A, f x (f y z) = f (f x y) z).

Fixpoint bin_A (l:list A)(def:A)(t:bin){struct t} : A :=
  match t with
  | node t1 t2 => f (bin_A l def t1)(bin_A l def t2)
  | leaf n => nth n l def
  end.
Fixpoint flatten_aux (t fin:bin){struct t} : bin :=
  match t with
  | node t1 t2 => flatten_aux t1 (flatten_aux t2 fin)
  | x => node x fin
  end.
Fixpoint flatten (t:bin) : bin :=
  match t with
  | node t1 t2 => flatten_aux t1 (flatten t2)
  | x => x
  end.
Theorem flatten_aux_valid_A :
 forall (l:list A)(def:A)(t t':bin),
 f (bin_A l def t)(bin_A l def t') = bin_A l def (flatten_aux t t').
Proof.
 intros l def t; elim t; simpl; auto.
 intros t1 IHt1 t2 IHt2 t';  rewrite <- IHt1; rewrite <- IHt2.
 symmetry; apply assoc.
Qed.

Theorem flatten_valid_A :
 forall (l:list A)(def:A)(t:bin),
   bin_A l def t = bin_A l def (flatten t).
Proof.
 intros l def t; elim t; simpl; trivial.
 intros t1 IHt1 t2 IHt2; rewrite <- flatten_aux_valid_A; rewrite <- IHt2.
 trivial.
Qed.

Theorem flatten_valid_A_2 :
 forall (t t':bin)(l:list A)(def:A),
   bin_A l def (flatten t) = bin_A l def (flatten t')->
   bin_A l def t = bin_A l def t'. 
Proof.
 intros t t' l def Heq.
 rewrite (flatten_valid_A l def t); rewrite (flatten_valid_A l def t').
 trivial.
Qed.

End assoc_eq.

Check flatten_valid_A_2.

Ltac term_list f l v :=
  match v with
  | (f ?X1 ?X2) =>
    let l1 := term_list f l X2 in term_list f l1 X1
  | ?X1 => constr:(cons X1 l)
  end.

Ltac compute_rank l n v :=
  match l with
  | (cons ?X1 ?X2) =>
    let tl := constr:X2 in
    match constr:(X1 = v) with
    | (?X1 = ?X1) => n
    | _ => compute_rank tl (S n) v
    end
  end.

Ltac model_aux l f v :=
  match v with
  | (f ?X1 ?X2) =>
    let r1 := model_aux l f X1 with r2 := model_aux l f X2 in
      constr:(node r1 r2)
  | ?X1 => let n := compute_rank l 0 X1 in constr:(leaf n)
  | _ => constr:(leaf 0)
  end.

Ltac model_A A f def v :=
  let l := term_list f (nil (A:=A)) v in
  let t := model_aux l f v in
  constr:(bin_A A f l def t).

Ltac assoc_eq A f assoc_thm :=
  match goal with
  | [ |- (@eq A ?X1 ?X2) ] =>
  let term1 := model_A A f X1 X1 
  with term2 := model_A A f X1 X2 in
  (change (term1 = term2);
   apply flatten_valid_A_2 with (1 := assoc_thm); auto)
  end.

Theorem reflection_test3 :
 forall x y z t u:Z, (x*(y*z*(t*u)) = x*y*(z*(t*u)))%Z.
Proof.
 intros; assoc_eq Z Zmult Zmult_assoc.
Qed.