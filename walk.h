#ifndef _WALK_H
#define _WALK_H

#include "journal.h"
#include "balance.h"
#include "valexpr.h"
#include "datetime.h"

#include <iostream>
#include <fstream>
#include <deque>

namespace ledger {

template <typename T>
struct item_handler {
  item_handler * handler;

 public:
  item_handler() : handler(NULL) {
    DEBUG_PRINT("ledger.memory.ctors", "ctor item_handler<T>");
  }
  item_handler(item_handler * _handler) : handler(_handler) {
    DEBUG_PRINT("ledger.memory.ctors", "ctor item_handler<T>");
  }

  virtual ~item_handler() {
    DEBUG_PRINT("ledger.memory.dtors", "dtor item_handler<T>");
  }
  virtual void flush() {
    if (handler)
      handler->flush();
  }
  virtual void operator()(T& item) {
    if (handler)
      (*handler)(item);
  }
};

template <typename T>
class compare_items {
  const value_expr_t * sort_order;
 public:
  compare_items(const value_expr_t * _sort_order)
    : sort_order(_sort_order) {
    assert(sort_order);
  }
  bool operator()(const T * left, const T * right);
};

template <typename T>
bool compare_items<T>::operator()(const T * left, const T * right)
{
  assert(left);
  assert(right);

  value_t left_result;
  value_t right_result;
  sort_order->compute(left_result, details_t(*left));
  sort_order->compute(right_result, details_t(*right));

  return left_result < right_result;
}

template <>
bool compare_items<transaction_t>::operator()(const transaction_t * left,
					      const transaction_t * right);
template <>
bool compare_items<account_t>::operator()(const account_t * left,
					  const account_t * right);

//////////////////////////////////////////////////////////////////////
//
// Transaction handlers
//

#define TRANSACTION_RECEIVED   0x0001
#define TRANSACTION_HANDLED    0x0002
#define TRANSACTION_TO_DISPLAY 0x0004
#define TRANSACTION_DISPLAYED  0x0008
#define TRANSACTION_NO_TOTAL   0x0010
#define TRANSACTION_SORT_CALC  0x0020
#define TRANSACTION_COMPOSITE  0x0040
#define TRANSACTION_MATCHES    0x0080

struct transaction_xdata_t
{
  value_t	 total;
  value_t	 sort_value;
  value_t	 composite_amount;
  unsigned int   index;
  unsigned short dflags;
  std::time_t    date;

  transaction_xdata_t() : index(0), dflags(0), date(0) {}
};

inline bool transaction_has_xdata(const transaction_t& xact) {
  return xact.data != NULL;
}

extern std::list<transaction_xdata_t> transactions_xdata;
extern std::list<void **>	      transactions_xdata_ptrs;

inline transaction_xdata_t& transaction_xdata_(const transaction_t& xact) {
  return *((transaction_xdata_t *) xact.data);
}

transaction_xdata_t& transaction_xdata(const transaction_t& xact);
void add_transaction_to(const transaction_t& xact, value_t& value);

//////////////////////////////////////////////////////////////////////

inline void walk_transactions(transactions_list::iterator begin,
			      transactions_list::iterator end,
			      item_handler<transaction_t>& handler) {
  for (transactions_list::iterator i = begin; i != end; i++)
    handler(**i);
}

inline void walk_transactions(transactions_list& list,
			      item_handler<transaction_t>& handler) {
  walk_transactions(list.begin(), list.end(), handler);
}

inline void walk_entries(entries_list::iterator       begin,
			 entries_list::iterator       end,
			 item_handler<transaction_t>& handler) {
  for (entries_list::iterator i = begin; i != end; i++)
    walk_transactions((*i)->transactions, handler);
}

inline void walk_entries(entries_list& list,
			 item_handler<transaction_t>& handler) {
  walk_entries(list.begin(), list.end(), handler);
}

void clear_transactions_xdata();

//////////////////////////////////////////////////////////////////////

class ignore_transactions : public item_handler<transaction_t>
{
 public:
  virtual void operator()(transaction_t& xact) {}
};

class set_account_value : public item_handler<transaction_t>
{
 public:
  set_account_value(item_handler<transaction_t> * handler = NULL)
    : item_handler<transaction_t>(handler) {}

  virtual void operator()(transaction_t& xact);
};

class sort_transactions : public item_handler<transaction_t>
{
  typedef std::deque<transaction_t *> transactions_deque;

  transactions_deque   transactions;
  const value_expr_t * sort_order;
  bool                 allocated;

 public:
  sort_transactions(item_handler<transaction_t> * handler,
		    const value_expr_t * _sort_order)
    : item_handler<transaction_t>(handler),
      sort_order(_sort_order), allocated(false) {}

  sort_transactions(item_handler<transaction_t> * handler,
		    const std::string& _sort_order)
    : item_handler<transaction_t>(handler), allocated(false) {
    try {
      sort_order = parse_value_expr(_sort_order);
      allocated  = true;
    }
    catch (value_expr_error& err) {
      throw value_expr_error(std::string("In sort string '") + _sort_order +
			     "': " + err.what());
    }
  }

  virtual ~sort_transactions() {
    assert(sort_order);
    if (allocated)
      delete sort_order;
  }

  virtual void flush();
  virtual void operator()(transaction_t& xact) {
    transactions.push_back(&xact);
  }
};

class filter_transactions : public item_handler<transaction_t>
{
  item_predicate<transaction_t> pred;

 public:
  filter_transactions(item_handler<transaction_t> * handler,
		      const value_expr_t * predicate)
    : item_handler<transaction_t>(handler), pred(predicate) {}

  filter_transactions(item_handler<transaction_t> * handler,
		      const std::string& predicate)
    : item_handler<transaction_t>(handler), pred(predicate) {}

  virtual void operator()(transaction_t& xact) {
    if (pred(xact)) {
      transaction_xdata(xact).dflags |= TRANSACTION_MATCHES;
      (*handler)(xact);
    }
  }
};

class calc_transactions : public item_handler<transaction_t>
{
  transaction_t * last_xact;

 public:
  calc_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler), last_xact(NULL) {}

  virtual void operator()(transaction_t& xact);
};

class invert_transactions : public item_handler<transaction_t>
{
 public:
  invert_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler) {}

  virtual void operator()(transaction_t& xact);
};

inline void clear_entries_transactions(std::list<entry_t>& entries_list) {
  for (std::list<entry_t>::iterator i = entries_list.begin();
       i != entries_list.end();
       i++)
    (*i).transactions.clear();
}

class collapse_transactions : public item_handler<transaction_t>
{
  value_t	  subtotal;
  unsigned int    count;
  entry_t *       last_entry;
  transaction_t * last_xact;
  account_t       totals_account;

  std::list<entry_t>       entry_temps;
  std::list<transaction_t> xact_temps;

 public:
  collapse_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler), count(0),
      last_entry(NULL), last_xact(NULL),
      totals_account(NULL, "<Total>") {}

  ~collapse_transactions() {
    clear_entries_transactions(entry_temps);
  }

  virtual void flush() {
    if (subtotal)
      report_subtotal();
    item_handler<transaction_t>::flush();
  }

  void report_subtotal();

  virtual void operator()(transaction_t& xact);
};

class related_transactions : public item_handler<transaction_t>
{
  transactions_list transactions;
  bool		    also_matching;

 public:
  related_transactions(item_handler<transaction_t> * handler,
		       const bool _also_matching = false)
    : item_handler<transaction_t>(handler),
      also_matching(_also_matching) {}

  virtual void flush();
  virtual void operator()(transaction_t& xact) {
    transaction_xdata(xact).dflags |= TRANSACTION_RECEIVED;
    transactions.push_back(&xact);
  }
};

class changed_value_transactions : public item_handler<transaction_t>
{
  // This filter requires that calc_transactions be used at some point
  // later in the chain.

  bool		  changed_values_only;
  transaction_t * last_xact;
  value_t         last_balance;

  std::list<entry_t>       entry_temps;
  std::list<transaction_t> xact_temps;

 public:
  changed_value_transactions(item_handler<transaction_t> * handler,
			     bool _changed_values_only)
    : item_handler<transaction_t>(handler),
      changed_values_only(_changed_values_only), last_xact(NULL) {}

  ~changed_value_transactions() {
    clear_entries_transactions(entry_temps);
  }

  virtual void flush() {
    if (last_xact) {
      output_diff(now);
      last_xact = NULL;
    }
    item_handler<transaction_t>::flush();
  }

  void output_diff(const std::time_t current);

  virtual void operator()(transaction_t& xact);
};

class subtotal_transactions : public item_handler<transaction_t>
{
  typedef std::map<account_t *, value_t>  values_map;
  typedef std::pair<account_t *, value_t> values_pair;

 protected:
  values_map values;

  std::list<entry_t>       entry_temps;
  std::list<transaction_t> xact_temps;

 public:
  std::time_t start;
  std::time_t finish;

  subtotal_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler), start(0), finish(0) {}
#ifdef DEBUG_ENABLED
  subtotal_transactions(const subtotal_transactions&) {
    assert(0);
  }
#endif

  ~subtotal_transactions() {
    clear_entries_transactions(entry_temps);
  }

  void report_subtotal(const char * spec_fmt = NULL);

  virtual void flush() {
    if (values.size() > 0)
      report_subtotal();
    item_handler<transaction_t>::flush();
  }
  virtual void operator()(transaction_t& xact);
};

class interval_transactions : public subtotal_transactions
{
  interval_t      interval;
  transaction_t * last_xact;
  bool            started;

  item_handler<transaction_t> * sorter;

 public:
  interval_transactions(item_handler<transaction_t> * _handler,
			const interval_t&    _interval,
			const value_expr_t * sort_order = NULL)
    : subtotal_transactions(_handler), interval(_interval),
      last_xact(NULL), started(false), sorter(NULL) {
    if (sort_order) {
      sorter = new sort_transactions(handler, sort_order);
      handler = sorter;
    }
  }
  interval_transactions(item_handler<transaction_t> * _handler,
			const std::string& _interval,
			const std::string& sort_order = "")
    : subtotal_transactions(_handler), interval(_interval),
      last_xact(NULL), started(false), sorter(NULL) {
    if (! sort_order.empty()) {
      sorter = new sort_transactions(handler, sort_order);
      handler = sorter;
    }
  }

  virtual ~interval_transactions() {
    if (sorter)
      delete sorter;
  }

  void report_subtotal(const std::time_t moment = 0);

  virtual void flush() {
    if (last_xact)
      report_subtotal();
    subtotal_transactions::flush();
  }
  virtual void operator()(transaction_t& xact);
};

class by_payee_transactions : public item_handler<transaction_t>
{
  typedef std::map<std::string, subtotal_transactions *>  payee_subtotals_map;
  typedef std::pair<std::string, subtotal_transactions *> payee_subtotals_pair;

  payee_subtotals_map payee_subtotals;

 public:
  by_payee_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler) {}

  virtual ~by_payee_transactions();

  virtual void flush();
  virtual void operator()(transaction_t& xact);
};

class set_comm_as_payee : public item_handler<transaction_t>
{
  std::list<entry_t>       entry_temps;
  std::list<transaction_t> xact_temps;

 public:
  set_comm_as_payee(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler) {}

  ~set_comm_as_payee() {
    clear_entries_transactions(entry_temps);
  }

  virtual void operator()(transaction_t& xact);
};

class dow_transactions : public subtotal_transactions
{
  transactions_list days_of_the_week[7];

 public:
  dow_transactions(item_handler<transaction_t> * handler)
    : subtotal_transactions(handler) {}

  virtual void flush();
  virtual void operator()(transaction_t& xact) {
    struct std::tm * desc = std::localtime(&xact.entry->date);
    days_of_the_week[desc->tm_wday].push_back(&xact);
  }
};

class generate_transactions : public item_handler<transaction_t>
{
 protected:
  typedef std::pair<interval_t, transaction_t *> pending_xacts_pair;
  typedef std::list<pending_xacts_pair>          pending_xacts_list;

  pending_xacts_list	   pending_xacts;
  std::list<entry_t>	   entry_temps;
  std::list<transaction_t> xact_temps;

 public:
  generate_transactions(item_handler<transaction_t> * handler)
    : item_handler<transaction_t>(handler) {}

  ~generate_transactions() {
    clear_entries_transactions(entry_temps);
  }

  void add_period_entries(period_entries_list& period_entries);

  virtual void add_transaction(const interval_t& period, transaction_t& xact);
};

#define BUDGET_NO_BUDGET  0x00
#define BUDGET_BUDGETED   0x01
#define BUDGET_UNBUDGETED 0x02

class budget_transactions : public generate_transactions
{
  unsigned short flags;

 public:
  budget_transactions(item_handler<transaction_t> * handler,
		      unsigned long _flags = BUDGET_BUDGETED)
    : generate_transactions(handler), flags(_flags) {}

  void report_budget_items(const std::time_t moment);

  virtual void operator()(transaction_t& xact);
};

class forecast_transactions : public generate_transactions
{
  item_predicate<transaction_t> pred;

 public:
  forecast_transactions(item_handler<transaction_t> * handler,
			const value_expr_t * predicate)
    : generate_transactions(handler), pred(predicate) {}

  forecast_transactions(item_handler<transaction_t> * handler,
			const std::string& predicate)
    : generate_transactions(handler), pred(predicate) {}

  virtual void add_transaction(const interval_t& period,
			       transaction_t&	 xact);
  virtual void flush();
};


//////////////////////////////////////////////////////////////////////
//
// Account walking functions
//

#define ACCOUNT_TO_DISPLAY	 0x0001
#define ACCOUNT_DISPLAYED	 0x0002
#define ACCOUNT_SORT_CALC	 0x0004
#define ACCOUNT_HAS_NON_VIRTUALS 0x0008
#define ACCOUNT_HAS_UNB_VIRTUALS 0x0010

struct account_xdata_t
{
  value_t	 value;
  value_t	 total;
  value_t	 sort_value;
  unsigned int   count;		// transactions counted toward amount
  unsigned int   total_count;	// transactions counted toward total
  unsigned int   virtuals;
  unsigned short dflags;

  account_xdata_t() : count(0), total_count(0), virtuals(0), dflags(0) {}
};

inline bool account_has_xdata(const account_t& account) {
  return account.data != NULL;
}

extern std::list<account_xdata_t> accounts_xdata;
extern std::list<void **>	  accounts_xdata_ptrs;

inline account_xdata_t& account_xdata_(const account_t& account) {
  return *((account_xdata_t *) account.data);
}

account_xdata_t& account_xdata(const account_t& account);

//////////////////////////////////////////////////////////////////////

void sum_accounts(account_t& account);

typedef std::deque<account_t *> accounts_deque;

void sort_accounts(account_t&	        account,
		   const value_expr_t * sort_order,
		   accounts_deque&      accounts);
void walk_accounts(account_t&		    account,
		   item_handler<account_t>& handler,
		   const value_expr_t *     sort_order = NULL);
void walk_accounts(account_t&		    account,
		   item_handler<account_t>& handler,
		   const std::string&       sort_string);

void clear_accounts_xdata();

inline void clear_all_xdata() {
  clear_transactions_xdata();
  clear_accounts_xdata();
}

//////////////////////////////////////////////////////////////////////

void walk_commodities(commodities_map& commodities,
		      item_handler<transaction_t>& handler);

} // namespace ledger

#endif // _WALK_H