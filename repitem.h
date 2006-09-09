#ifndef _REPITEM_H
#define _REPITEM_H

#include "journal.h"
#include "datetime.h"
#include "session.h"

#include <iostream>
#include <memory>
#include <list>

namespace ledger {

class report_t;

class repitem_t
{
public:
  repitem_t * parent;
  repitem_t * next;
  repitem_t * prev;

  union {
    transaction_t * xact;
    entry_t *	    entry;
    account_t *	    account_ptr;
    report_t *	    report_ptr;
  };

  enum kind_t { XACT, ENTRY, ACCOUNT, REPORT };
  kind_t kind;

  bool istemp; // if item pointer is a temporary; assert that its
	       // journal pointer is NULL

  repitem_t * contents;
  repitem_t * last_content;
  repitem_t * children;
  repitem_t * last_child;

  mutable value_t _total;
  mutable value_t _value;
  mutable value_t _sort_value;
  unsigned short  flags;
  unsigned int    parents_elided;
  account_t *	  reported_account;
  datetime_t	  reported_date;

  repitem_t()
    : parent(NULL), next(NULL), prev(NULL), istemp(false),
      contents(NULL), last_content(NULL),
      children(NULL), last_child(NULL),
      flags(0), parents_elided(0), reported_account(NULL) {}

  virtual ~repitem_t();

  void add_value(value_t& val) const;
  void add_sort_value(value_t& val) const;
  void add_total(value_t& val) const;

  void set_value(value_t& result) {
    add_value(result);
  }

  datetime_t date() const;
  datetime_t effective_date() const;
  datetime_t actual_date() const;

  void set_date(value_t& result) {
    result = date();
  }

  account_t * account() const;
  report_t * report() const;

  bool valid() const;

  static repitem_t * wrap_item(transaction_t * txact);
  static repitem_t * wrap_item(entry_t * tentry);
  static repitem_t * wrap_item(account_t * taccount);

  repitem_t * add_content(repitem_t * item);
  repitem_t * add_child(repitem_t * item);

  static repitem_t * fake_transaction(account_t * taccount);
  static repitem_t * fake_entry(const datetime_t& date,
				const std::string& payee);

  void populate_entries(entries_list& entries);
  void populate_entries(entries_list& entries,
			const value_expr_t * filter);

  void populate_account(account_t& acct, repitem_t * item);
  void populate_accounts(entries_list& entries);
  void populate_accounts(entries_list& entries,
			 const value_expr_t * filter);

  void print_tree(std::ostream& out, int depth = 0);
};

class repitem_scope_t : public scope_t
{
 public:
  repitem_t * repitem;

  repitem_scope_t(scope_t * parent = NULL)
    : scope_t(parent), repitem(NULL)
  {
    define("value-0", value_expr_t::wrap_functor
	   (new repitem_ref_callback_t(this, &repitem_t::set_value)));
    define("date-0", value_expr_t::wrap_functor
	   (new repitem_ref_callback_t(this, &repitem_t::set_date)));
#if 0
    // Item details
    AMOUNT,
    COST,
    PRICE,
    DATE,
    ACT_DATE,
    EFF_DATE,
    CLEARED,
    PENDING,
    REAL,
    ACTUAL,
    INDEX,
    DEPTH,

    // Item totals
    COUNT,
    TOTAL,
    COST_TOTAL,
    PRICE_TOTAL,

    // Relating to format_t
    VALUE_EXPR,
    TOTAL_EXPR,

    // Functions
    F_NOW,
    F_ARITH_MEAN,
    F_QUANTITY,
    F_COMMODITY,
    F_SET_COMMODITY,
    F_VALUE,
    F_ABS,
    F_ROUND,
    F_PRICE,
    F_DATE,
    F_DATECMP,
    F_YEAR,
    F_MONTH,
    F_DAY,
    F_CODE_MASK,
    F_PAYEE_MASK,
    F_NOTE_MASK,
    F_ACCOUNT_MASK,
    F_SHORT_ACCOUNT_MASK,
    F_COMMODITY_MASK,

    F_PARENT,
#endif
  }

  void set_repitem(repitem_t * _repitem) {
    repitem = _repitem;
  }

  struct repitem_ref_callback_t : value_expr_t::functor_t
  {
    repitem_scope_t * scope;
    void (repitem_t::*mptr)(value_t& result);

    repitem_ref_callback_t(repitem_scope_t * _scope,
			   void (repitem_t::*_mptr)(value_t& result))
      : scope(_scope), mptr(_mptr) {}

    virtual void operator()(value_t& result, value_expr_t * context) {
      assert(scope->repitem);
      (scope->repitem->*mptr)(result);
    }
  };
};

class repitem_ref_scope_t : public scope_t
{
  repitem_scope_t * base_scope;

 public:
  repitem_ref_scope_t(repitem_t * repitem, repitem_scope_t * repitem_scope,
		      scope_t * parent = NULL)
    : scope_t(parent), base_scope(repitem_scope)
  {
    assert(base_scope->repitem == NULL);
    base_scope->set_repitem(repitem);
  }
  virtual ~repitem_ref_scope_t() {
    assert(base_scope->repitem != NULL);
    base_scope->set_repitem(NULL);
  }

  virtual void define(const std::string& name, value_expr_t * def) {
    assert(0);
  }
  virtual value_expr_t * lookup(const std::string& name) {
    return base_scope->lookup(name);
  }
};

//////////////////////////////////////////////////////////////////////

class repitem_predicate
{
 public:
  valexpr_t pred_expr;

  item_predicate(const std::string& predicate) {
    if (! predicate.empty())
      pred_expr.parse(predicate);
  }

  bool operator()(repitem_t * item) const {
    // jww (2006-09-08): Create a repitem scope here
    return (! pred_expr || pred_expr.calc(NULL).strip_annotations());
  }
};

} // namespace ledger

#endif // _REPITEM_H