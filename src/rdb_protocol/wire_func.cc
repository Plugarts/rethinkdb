// Copyright 2010-2013 RethinkDB, all rights reserved.
#include "rdb_protocol/wire_func.hpp"

#include "rdb_protocol/env.hpp"
#include "rdb_protocol/func.hpp"
#include "rdb_protocol/protocol.hpp"

namespace ql {

wire_func_t::wire_func_t() { }

class wire_func_construction_visitor_t : public func_visitor_t {
public:
    explicit wire_func_construction_visitor_t(wire_func_t *_that) : that(_that) { }

    void on_reql_func(const reql_func_t *reql_func) {
        that->func = wire_reql_func_t();
        wire_reql_func_t *p = boost::get<wire_reql_func_t>(&that->func);
        p->captured_scope = reql_func->captured_scope;
        p->arg_names = reql_func->arg_names;
        p->body = reql_func->body->get_src();
        p->backtrace = reql_func->backtrace();
    }
    void on_js_func(const js_func_t *js_func) {
        that->func = wire_js_func_t();
        wire_js_func_t *p = boost::get<wire_js_func_t>(&that->func);
        p->js_source = js_func->js_source;
        p->js_timeout_ms = js_func->js_timeout_ms;
        p->backtrace = js_func->backtrace();
    }

private:
    wire_func_t *that;
};

wire_func_t::wire_func_t(counted_t<func_t> f) {
    r_sanity_check(f.has());
    wire_func_construction_visitor_t v(this);
    f->visit(&v);
    reinitialize_cached_func();
}

wire_func_t::wire_func_t(protob_t<const Term> body, std::vector<sym_t> arg_names,
                         protob_t<const Backtrace> backtrace) {
    func = wire_reql_func_t();
    wire_reql_func_t *p = boost::get<wire_reql_func_t>(&func);
    p->arg_names = std::move(arg_names);
    p->body = std::move(body);
    p->backtrace = std::move(backtrace);
    reinitialize_cached_func();
}

wire_func_t::wire_func_t(const wire_func_t &copyee)
    : func(copyee.func), cached_func(copyee.cached_func) { }

wire_func_t &wire_func_t::operator=(const wire_func_t &assignee) {
    wire_func_t tmp(assignee);
    std::swap(func, tmp.func);
    std::swap(cached_func, tmp.cached_func);
    return *this;
}

wire_func_t::~wire_func_t() { }


struct wire_func_compile_visitor_t : public boost::static_visitor<counted_t<func_t> > {
    wire_func_compile_visitor_t() { }

    counted_t<func_t> operator()(const wire_reql_func_t &func) const {
        r_sanity_check(func.body.has() && func.backtrace.has());
        compile_env_t compile_env(func.captured_scope.compute_visibility().with_func_arg_name_list(func.arg_names));
        return make_counted<reql_func_t>(func.backtrace, func.captured_scope, func.arg_names,
                                         compile_term(&compile_env, func.body));
    }

    counted_t<func_t> operator()(const wire_js_func_t &func) const {
        r_sanity_check(func.backtrace.has());
        return make_counted<js_func_t>(func.js_source, func.js_timeout_ms, func.backtrace);
    }

    DISABLE_COPYING(wire_func_compile_visitor_t);
};

void wire_func_t::reinitialize_cached_func() {
    cached_func = boost::apply_visitor(wire_func_compile_visitor_t(), func);
}

counted_t<func_t> wire_func_t::compile_wire_func() const {
    return cached_func;
}

protob_t<const Backtrace> wire_func_t::get_bt() const {
    return cached_func->backtrace();
}

write_message_t &operator<<(write_message_t &msg, const wire_reql_func_t &func) {  // NOLINT(runtime/references)
    guarantee(func.body.has() && func.backtrace.has());
    msg << func.captured_scope;
    msg << func.arg_names;
    msg << *func.body;
    msg << *func.backtrace;
    return msg;
}

archive_result_t deserialize(read_stream_t *s, wire_reql_func_t *func) {
    var_scope_t captured_scope;
    archive_result_t res = deserialize(s, &captured_scope);
    if (res) { return res; }

    std::vector<sym_t> arg_names;
    res = deserialize(s, &arg_names);
    if (res) { return res; }

    protob_t<Term> body = make_counted_term();
    res = deserialize(s, &*body);
    if (res) { return res; }

    protob_t<Backtrace> backtrace = make_counted_backtrace();
    res = deserialize(s, &*backtrace);
    if (res) { return res; }

    func->captured_scope = std::move(captured_scope);
    func->arg_names = std::move(arg_names);
    func->body = std::move(body);
    func->backtrace = std::move(backtrace);
    return res;
}

write_message_t &operator<<(write_message_t &msg, const wire_js_func_t &func) {  // NOLINT(runtime/references)
    guarantee(func.backtrace.has());
    msg << func.js_source;
    msg << func.js_timeout_ms;
    msg << *func.backtrace;
    return msg;
}

archive_result_t deserialize(read_stream_t *s, wire_js_func_t *func) {
    wire_js_func_t tmp;

    archive_result_t res = deserialize(s, &tmp.js_source);
    if (res) { return res; }

    res = deserialize(s, &tmp.js_timeout_ms);
    if (res) { return res; }

    protob_t<Backtrace> backtrace = make_counted_backtrace();
    res = deserialize(s, &*backtrace);
    if (res) { return res; }
    tmp.backtrace = backtrace;

    *func = std::move(tmp);
    return res;
}

void wire_func_t::rdb_serialize(write_message_t &msg) const {
    msg << func;
}

archive_result_t wire_func_t::rdb_deserialize(read_stream_t *s) {
    archive_result_t res = deserialize(s, &func);
    if (res) { return res; }
    reinitialize_cached_func();
    return res;
}

gmr_wire_func_t::gmr_wire_func_t(counted_t<func_t> _group,
                                 counted_t<func_t> _map,
                                 counted_t<func_t> _reduce)
    : group(_group), map(_map), reduce(_reduce) { }

counted_t<func_t> gmr_wire_func_t::compile_group() const {
    return group.compile_wire_func();
}
counted_t<func_t> gmr_wire_func_t::compile_map() const {
    return map.compile_wire_func();
}
counted_t<func_t> gmr_wire_func_t::compile_reduce() const {
    return reduce.compile_wire_func();
}


}  // namespace ql
