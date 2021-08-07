#ifndef _DATABASE_HPP
#define _DATABASE_HPP

#include <boost/json.hpp>

#include <cstddef>
#include <string>
#include <vector>
#include <queue>
#include <optional>
#include <mutex>
#include <memory>
#include <initializer_list>

#include <pqxx/pqxx>

namespace bserv {

class db_connection_manager;

class db_connection {
private:
    db_connection_manager& mgr_;
    std::shared_ptr<pqxx::connection> conn_;
public:
    db_connection(
        db_connection_manager& mgr,
        std::shared_ptr<pqxx::connection> conn)
    : mgr_{mgr}, conn_{conn} {}
    // non-copiable, non-assignable
    db_connection(const db_connection&) = delete;
    db_connection& operator=(const db_connection&) = delete;
    // during the destruction, it should put itself back to the 
    // manager's queue
    ~db_connection();
    pqxx::connection& get() { return *conn_; }
};

// provides the database connection pool functionality
class db_connection_manager {
private:
    std::queue<std::shared_ptr<pqxx::connection>> queue_;
    // this lock is for manipulating the `queue_`
    mutable std::mutex queue_lock_;
    // since C++ 17 doesn't provide the semaphore functionality,
    // mutex is used to mimic it. (boost provides it)
    // if there are no available connections, trying to lock on
    // it will cause blocking.
    mutable std::mutex counter_lock_;
    friend db_connection;
public:
    db_connection_manager(const std::string& conn_str, int n) {
        for (int i = 0; i < n; ++i)
            queue_.emplace(
                std::make_shared<pqxx::connection>(conn_str));
    }
    // if there are no available database connections, this function
    // blocks until there is any;
    // otherwise, this function returns a pointer to `db_connection`.
    std::shared_ptr<db_connection> get_or_block() {
        // `counter_lock_` must be acquired first.
        // exchanging this statement with the next will cause dead-lock,
        // because if the request is blocked by `counter_lock_`,
        // the destructor of `db_connection` will not be able to put
        // itself back due to the `queue_lock_` has already been acquired
        // by this request!
        counter_lock_.lock();
        // `queue_lock_` is acquired so that only one thread will
        // modify the `queue_`
        std::lock_guard<std::mutex> lg{queue_lock_};
        std::shared_ptr<pqxx::connection> conn = queue_.front();
        queue_.pop();
        // if there are no connections in the `queue_`,
        // `counter_lock_` remains to be locked
        // so that the following requests will be blocked
        if (queue_.size() != 0) counter_lock_.unlock();
        return std::make_shared<db_connection>(*this, conn);
    }
};

inline db_connection::~db_connection() {
    std::lock_guard<std::mutex> lg{mgr_.queue_lock_};
    mgr_.queue_.emplace(conn_);
    // if this is the first available connection back to the queue,
    // `counter_lock_` is unlocked so that the blocked requests will
    // be notified
    if (mgr_.queue_.size() == 1)
        mgr_.counter_lock_.unlock();
}

// **************************************************************************

class db_parameter {
public:
    virtual ~db_parameter() = default;
    virtual std::string get_value(pqxx::work&) = 0;
};

class db_name : public db_parameter {
private:
    std::string value_;
public:
    db_name(const std::string& value)
    : value_{value} {}
    std::string get_value(pqxx::work& w) {
        return w.quote_name(value_);
    }
};

template <typename Type>
class db_value : public db_parameter {
private:
    Type value_;
public:
    db_value(const Type& value)
    : value_{value} {}
    std::string get_value(pqxx::work&) {
        return std::to_string(value_);
    }
};

template <>
class db_value<std::string> : public db_parameter {
private:
    std::string value_;
public:
    db_value(const std::string& value)
    : value_{value} {}
    std::string get_value(pqxx::work& w) {
        return w.quote(value_);
    }
};

template <>
class db_value<bool> : public db_parameter {
private:
    bool value_;
public:
    db_value(const bool& value)
    : value_{value} {}
    std::string get_value(pqxx::work&) {
        return value_ ? "true" : "false";
    }
};

namespace db_internal {

template <typename Param>
std::shared_ptr<db_parameter> convert_parameter(
    const Param& param) {
    return std::make_shared<db_value<Param>>(param);
}

template <typename Param>
std::shared_ptr<db_parameter> convert_parameter(
    const db_value<Param>& param) {
    return std::make_shared<db_value<Param>>(param);
}

inline std::shared_ptr<db_parameter> convert_parameter(
    const char* param) {
    return std::make_shared<db_value<std::string>>(param);
}

inline std::shared_ptr<db_parameter> convert_parameter(
    const db_name& param) {
    return std::make_shared<db_name>(param);
}

template <typename ...Params>
std::vector<std::string> convert_parameters(
    pqxx::work& w, std::shared_ptr<Params>... params) {
    return {params->get_value(w)...};
}

// *************************************

class db_field_holder {
protected:
    std::string name_;
public:
    db_field_holder(const std::string& name)
    : name_{name} {}
    virtual ~db_field_holder() = default;
    virtual void add(
        const pqxx::row& row, size_t field_idx,
        boost::json::object& obj) = 0;
};

template <typename Type>
class db_field : public db_field_holder {
public:
    using db_field_holder::db_field_holder;
    void add(
        const pqxx::row& row, size_t field_idx,
        boost::json::object& obj) {
        obj[name_] = row[field_idx].as<Type>();
    }
};

template <>
class db_field<std::string> : public db_field_holder {
public:
    using db_field_holder::db_field_holder;
    void add(
        const pqxx::row& row, size_t field_idx,
        boost::json::object& obj) {
        obj[name_] = row[field_idx].c_str();
    }
};

}  // db_internal

template <typename Type>
std::shared_ptr<db_internal::db_field_holder> make_db_field(
    const std::string& name) {
    return std::make_shared<db_internal::db_field<Type>>(name);
}

class invalid_operation_exception : public std::exception {
private:
    std::string msg_;
public:
    invalid_operation_exception(const std::string& msg)
    : msg_{msg} {}
    const char* what() const noexcept { return msg_.c_str(); }
};

class db_relation_to_object {
private:
    std::vector<std::shared_ptr<db_internal::db_field_holder>> fields_;
public:
    db_relation_to_object(
        const std::initializer_list<
            std::shared_ptr<db_internal::db_field_holder>>& fields)
    : fields_{fields} {}
    boost::json::object convert_row(const pqxx::row& row) {
        boost::json::object obj;
        for (size_t i = 0; i < fields_.size(); ++i)
            fields_[i]->add(row, i, obj);
        return obj;
    }
    std::vector<boost::json::object> convert_to_vector(
        const pqxx::result& result) {
        std::vector<boost::json::object> results;
        for (const auto& row : result)
            results.emplace_back(convert_row(row));
        return results;
    }
    std::optional<boost::json::object> convert_to_optional(
        const pqxx::result& result) {
        if (result.size() == 0) return std::nullopt;
        if (result.size() == 1) return convert_row(result[0]);
        // result.size() > 1
        throw invalid_operation_exception{
            "too many objects to convert"};
    }
};

// Usage:
// db_exec(tx, "select * from ? where ? = ? and first_name = 'Name??'",
//         db_name("auth_user"), db_name("is_active"), db_value<bool>(true));
// -> SQL: select * from "auth_user" where "is_active" = true and first_name = 'Name?'
// ======================================================================================
// db_exec(tx, "select * from ? where ? = ? and first_name = ?",
//         db_name("auth_user"), db_name("is_active"), false, "Name??");
// -> SQL: select * from "auth_user" where "is_active" = false and first_name = 'Name??'
// ======================================================================================
// Note: "?" is the placeholder for parameters, and "??" will be converted to "?" in SQL.
//       But, "??" in the parameters remains.
template <typename ...Params>
pqxx::result db_exec(pqxx::work& w,
    const std::string& s, const Params&... params) {
    std::vector<std::string> param_vec =
        db_internal::convert_parameters(
            w, db_internal::convert_parameter(params)...);
    size_t idx = 0;
    std::string query;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '?') {
            if (i + 1 < s.length() && s[i + 1] == '?') {
                query += s[++i];
            } else {
                if (idx < param_vec.size()) {
                    query += param_vec[idx++];
                } else throw std::out_of_range{"too few parameters"};
            }
        } else query += s[i];
    }
    if (idx != param_vec.size())
        throw invalid_operation_exception{"too many parameters"};
    return w.exec(query);
}


// TODO: add support for time conversions between postgresql and c++, use timestamp?
//       what about time zone?

}  // bserv

#endif  // _DATABASE_HPP