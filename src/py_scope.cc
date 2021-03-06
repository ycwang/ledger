/*
 * Copyright (c) 2003-2009, John Wiegley.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of New Artisans LLC nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <system.hh>

#include "pyinterp.h"

namespace ledger {

using namespace boost::python;

namespace {
  void py_scope_define(scope_t& scope, const string& name, expr_t& def)
  {
    return scope.define(name, def.get_op());
  }

  expr_t py_scope_lookup(scope_t& scope, const string& name)
  {
    return scope.lookup(name);
  }

  value_t py_scope_getattr(scope_t& scope, const string& name)
  {
    return expr_t(scope.lookup(name)).calc(scope);
  }

  struct scope_wrapper : public scope_t
  {
    PyObject * self;

    scope_wrapper(PyObject * self_) : self(self_) {}

    virtual expr_t::ptr_op_t lookup(const string&) {
      return NULL;
    }    
  };
}

void export_scope()
{
  class_< scope_t, scope_wrapper, boost::noncopyable > ("Scope", no_init)
    .def("define", py_scope_define)
    .def("lookup", py_scope_lookup)
    .def("__getattr__", py_scope_getattr)
    ;

  class_< child_scope_t, bases<scope_t>,
          boost::noncopyable > ("ChildScope")
    .def(init<>())
    .def(init<scope_t&>())
    ;

  class_< symbol_scope_t, bases<child_scope_t>,
          boost::noncopyable > ("SymbolScope")
    .def(init<>())
    .def(init<scope_t&>())
    ;

  class_< call_scope_t, bases<child_scope_t>,
          boost::noncopyable > ("CallScope", init<scope_t&>())
    ;

  class_< bind_scope_t, bases<child_scope_t>,
          boost::noncopyable > ("BindScope", init<scope_t&, scope_t&>())
    ;
}

} // namespace ledger
