#include <string>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <assert.h>
#include "common.h"

using namespace std;

void* prime_quote(env*, list*);
void* prime_lambda(env*, list*);
void* prime_label(env*, list*);
void* prime_cond(env*, list*);
void* prime_defun(env*, list*);

void* prime_head(env*, list*);
void* prime_tail(env*, list*);
void* prime_cons(env*, list*);
void* prime_atom(env*, list*);
void* prime_eq(env*, list*);
void* prime_list(env*, list*);

func_dict expr_func_dict =
{
    {prime_quote, "quote"},
    {prime_lambda, "lambda"},
    {prime_label, "label"},
    {prime_cond, "cond"},
    {prime_defun, "defun"}
};
func_dict val_func_dict =
{
    {prime_head, "car"},
    {prime_tail, "cdr"},
    {prime_cons, "cons"},
    {prime_atom, "atom"},
    {prime_eq, "eq"},
    {prime_list, "list"},
};

env* last_env;
env* cons(env* e)
{
    env* ret = new env();
    if (e != 0)
	ret->e = e->e;
    ret->prev = e;
    
    last_env = ret;
    return ret;
}

void register_func(env* e)
{
    for (auto p : expr_func_dict)
	(*e)[create_atom(p.second)] = (void*)p.first;
    for (auto p : val_func_dict)
	(*e)[create_atom(p.second)] = (void*)p.first;
}

bool is_expr_func(void* ptr)
{ return expr_func_dict.find((prime_func)ptr) != expr_func_dict.end(); }
bool is_val_func(void* ptr)
{ return val_func_dict.find((prime_func)ptr) != val_func_dict.end(); }

void* eval_val(env* e, atom a)
{
    if (e == 0)
	return 0;
    
    auto it = e->find(a);
    if (it != e->cend())
	return it->second;

    eval_val(e->prev, a);
}

list* eval_args(env* e, list* l)
{
    if (l == 0)
	return 0;
    return cons(eval(e, l->head), eval_args(e, l->tail));
}

void* eval_expr(env* e, list* l)
{
    if (l == 0)
	return 0;
    
    void* v = eval(e, l->head);
    if (v != 0)
    {
	if (is_expr_func(v))
	{
	    prime_func f = *(prime_func)v;
	    return f(e, l->tail);
	}
	else if (is_val_func(v))
	{
	    prime_func f = *(prime_func)v;
	    return f(e, eval_args(e, l->tail));
	}
	else if (is_list(v))
	{
	    list* t = (list*)v;
	    atom head = atom(t->head);
	    if (head == atom_lambda) // l : ((lambda (p1...pn) e) a1...an)
	    {
		env *env = cons(last_env);
		
		list* tail = t->tail; // ((p1...pn) e)
		list* params = (list*)(tail->head); // (p1...pn)
		void* body = tail->tail->head;
		list* args = eval_args(e, l->tail); // (a1...an)
		while (params) // bind p to a.
		{
		    assert(is_atom(params->head)); // param error.
		    assert(args);
		    
		    atom p = atom(params->head);
		    (*env)[p] = args->head;
		    params = params->tail;
		    args = args->tail;
		}
		return eval(env, body);
	    }
	}
    }
    
    return 0;
}

void* eval(env* e, void* v)
{
    if (is_atom(v))
	return eval_val(e, (atom)v);
    else
	return eval_expr(e, (list*)v);
    return 0;
}

void collect(list* l, unordered_set<list*>& set)
{
    set.insert(l);
    if (is_list(l->head))
	collect((list*)(l->head), set);
    if (l->tail)
	collect(l->tail, set);
}

void clean(env* e)
{
    unordered_set<list*> set;
    for (auto p : (*e))
	if (is_list(p.second))
	    collect((list*)(p.second), set);
    list_coll(set);

    while (last_env != e)
    {
	env* env = last_env;
	last_env = last_env->prev;
	delete env;
    }
}

void* prime_quote(env* e, list* l) // ('(...))
{ return l->head; } // don't eval the expr inside.
void* prime_lambda(env* e, list* l) // (lambda (p1...pn) e)
{ return cons(atom_lambda, l); }
void* prime_label(env* e, list* l) // (label f (lambda (p1...pn) e)
{
    if (is_atom(l->head))
    {
	atom label = atom(l->head);
	(*e)[label] = eval(e, l->tail->head); // ...
    }
    return 0;
}
void* prime_cond(env* e, list* l) // (cond ((...) (...)) ...)
{
    list* args = l;
    while (args != 0)
    {
	assert(!is_atom(args->head)); // syntax error.
	list* pair = (list*)(args->head);
	if (eval(e, pair->head) != 0)
	    return eval(e, pair->tail->head);
	args = args->tail;
    }
    return 0;
}
void* prime_defun(env* e, list* l) // (defun f (p1...pn) e)
{
    list* lambda = cons(atom_lambda, l->tail);
    void* label = l->head;
    return prime_label(e, cons(label, cons(lambda, 0)));
}

void* prime_head(env* e, list* l)
{
    if (l->head && !is_atom(l->head))
	return ((list*)(l->head))->head;
    return 0;
}
void* prime_tail(env* e, list* l)
{
    if (l->head && !is_atom(l->head))
	return ((list*)(l->head))->tail;
    return 0;
}
void* prime_cons(env* e, list* l)
{ return cons(l->head, (list*)(l->tail->head)); }
void* prime_atom(env* e, list* l)
{
    if (l->head == 0 ||    // nil list.
	is_atom(l->head)) // or an atom.
	return atom_true;
    return 0;
}
void* prime_eq(env* e, list* l)
{
    void* fst = l->head;
    void* snd = l->tail->head;
    if ((fst == 0 && snd == 0) ||
	(is_atom(fst) && is_atom(snd) && fst == snd))
	return atom_true;
    return 0;
}
void* prime_list(env* e, list* l)
{ return l; }
