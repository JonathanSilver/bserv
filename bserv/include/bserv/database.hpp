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

// [2022.3.23] fix issue:
// because the project structure of pqxx has been updated,
// including only pqxx is not enough
#include <pqxx/result>

namespace bserv {

	using raw_db_connection_type = pqxx::connection;
	using raw_db_transaction_type = pqxx::work;

	class db_field {
	private:
		pqxx::field field_;
	public:
		db_field(const pqxx::field& field) : field_{ field } {}
		const char* c_str() const { return field_.c_str(); }
		template <typename Type>
		Type as() const { return field_.as<Type>(); }
		bool is_null() const { return field_.is_null(); }
	};

	class db_row {
	private:
		pqxx::row row_;
	public:
		db_row(const pqxx::row& row) : row_{ row } {}
		std::size_t size() const { return row_.size(); }
		db_field operator[](std::size_t idx) const { return row_[(pqxx::row::size_type)idx]; }
	};

	class db_result {
	private:
		pqxx::result result_;
	public:
		class const_iterator {
		private:
			pqxx::result::const_iterator iterator_;
		public:
			const_iterator(
				const pqxx::result::const_iterator& iterator
			) : iterator_{ iterator } {}
			const_iterator& operator++() { ++iterator_; return *this; }
			bool operator==(const const_iterator& rhs) const { return iterator_ == rhs.iterator_; }
			bool operator!=(const const_iterator& rhs) const { return iterator_ != rhs.iterator_; }
			db_row operator*() const { return *iterator_; }
		};
		db_result() = default;
		db_result(const pqxx::result& result) : result_{ result } {}
		const_iterator begin() const { return result_.begin(); }
		const_iterator end() const { return result_.end(); }
		std::string query() const { return result_.query(); }
	};

	class db_connection_manager;

	class db_connection {
	private:
		db_connection_manager& mgr_;
		std::shared_ptr<raw_db_connection_type> conn_;
	public:
		db_connection(
			db_connection_manager& mgr,
			std::shared_ptr<raw_db_connection_type> conn)
			: mgr_{ mgr }, conn_{ conn } {}
		// non-copiable, non-assignable
		db_connection(const db_connection&) = delete;
		db_connection& operator=(const db_connection&) = delete;
		// during the destruction, it should put itself back to the 
		// manager's queue
		~db_connection();
		raw_db_connection_type& get() { return *conn_; }
	};

	// provides the database connection pool functionality
	class db_connection_manager {
	private:
		std::queue<std::shared_ptr<raw_db_connection_type>> queue_;
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
					std::make_shared<raw_db_connection_type>(conn_str));
		}
		// if there are no available database connections, this function
		// blocks until there is any;
		// otherwise, this function returns a pointer to `db_connection`.
		std::shared_ptr<db_connection> get_or_block();
	};

	// **************************************************************************

	class db_parameter {
	public:
		virtual ~db_parameter() = default;
		virtual std::string get_value(raw_db_transaction_type&) = 0;
	};

	class db_name : public db_parameter {
	private:
		std::string value_;
	public:
		db_name(const std::string& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type& tx) {
			return tx.quote_name(value_);
		}
	};

	template <typename Type>
	class db_value : public db_parameter {
	private:
		Type value_;
	public:
		db_value(const Type& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type&) {
			return std::to_string(value_);
		}
	};

	template <>
	class db_value<std::string> : public db_parameter {
	private:
		std::string value_;
	public:
		db_value(const std::string& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type& tx) {
			return tx.quote(value_);
		}
	};

	template <>
	class db_value<boost::json::string> : public db_parameter {
	private:
		std::string value_;
	public:
		db_value(const boost::json::string& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type& tx) {
			return tx.quote(value_);
		}
	};

	template <>
	class db_value<bool> : public db_parameter {
	private:
		bool value_;
	public:
		db_value(const bool& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type&) {
			return value_ ? "true" : "false";
		}
	};

	template <>
	class db_value<std::nullptr_t> : public db_parameter {
	public:
		db_value(const std::nullptr_t&) {}
		std::string get_value(raw_db_transaction_type&) {
			return "null";
		}
	};

	template <typename Type>
	class db_value<std::optional<Type>> : public db_parameter {
	private:
		std::optional<Type> value_;
	public:
		db_value(const std::optional<Type>& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type& tx) {
			return value_.has_value()
				? db_value<Type>{value_.value()}.get_value(tx)
				: "null";
		}
	};

	template <typename Type>
	class db_value<std::vector<Type>> : public db_parameter {
	private:
		std::vector<Type> value_;
	public:
		db_value(const std::vector<Type>& value)
			: value_{ value } {}
		std::string get_value(raw_db_transaction_type& tx) {
			std::string res;
			for (const auto& elem : value_) {
				if (res.size() != 0) res += ", ";
				res += db_value<Type>{elem}.get_value(tx);
			}
			return "ARRAY[" + res + "]";
		}
	};

	class unsupported_json_value_type : public std::exception {
	public:
		unsupported_json_value_type() = default;
		const char* what() const noexcept { return "unsupported json value type"; }
	};

	template <>
	class db_value<boost::json::value> : public db_parameter {
	private:
		boost::json::value value_;
		boost::json::value copy_json_value(
			const boost::json::value& value) const {
			if (value.is_bool()) {
				return value.as_bool();
			}
			else if (value.is_double()) {
				return value.as_double();
			}
			else if (value.is_int64()) {
				return value.as_int64();
			}
			else if (value.is_null()) {
				return nullptr;
			}
			else if (value.is_string()) {
				return value.as_string();
			}
			else if (value.is_uint64()) {
				return value.as_uint64();
			}
			else {
				throw unsupported_json_value_type{};
			}
		}
	public:
		db_value(const boost::json::value& value)
			: value_{ copy_json_value(value) } {}
		std::string get_value(raw_db_transaction_type& tx) {
			if (value_.is_bool()) {
				return db_value<bool>{value_.as_bool()}.get_value(tx);
			}
			else if (value_.is_double()) {
				return db_value<double>{value_.as_double()}.get_value(tx);
			}
			else if (value_.is_int64()) {
				return db_value<std::int64_t>{value_.as_int64()}.get_value(tx);
			}
			else if (value_.is_null()) {
				return db_value<std::nullptr_t>{nullptr}.get_value(tx);
			}
			else if (value_.is_string()) {
				return db_value<boost::json::string>{value_.as_string()}.get_value(tx);
			}
			else if (value_.is_uint64()) {
				return db_value<std::uint64_t>{value_.as_uint64()}.get_value(tx);
			}
			else {
				throw unsupported_json_value_type{};
			}
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
			raw_db_transaction_type& tx, std::shared_ptr<Params>... params) {
			return { params->get_value(tx)... };
		}

		// *************************************

		class db_field_holder {
		protected:
			std::string name_;
		public:
			db_field_holder(const std::string& name)
				: name_{ name } {}
			virtual ~db_field_holder() = default;
			virtual void add(
				const db_row& row, std::size_t field_idx,
				boost::json::object& obj) = 0;
		};

		template <typename Type>
		class db_field : public db_field_holder {
		public:
			using db_field_holder::db_field_holder;
			void add(
				const db_row& row, std::size_t field_idx,
				boost::json::object& obj) {
				obj[name_] = row[field_idx].as<Type>();
			}
		};

		template <>
		class db_field<std::string> : public db_field_holder {
		public:
			using db_field_holder::db_field_holder;
			void add(
				const db_row& row, std::size_t field_idx,
				boost::json::object& obj) {
				obj[name_] = row[field_idx].c_str();
			}
		};

		template <typename Type>
		class db_field<std::optional<Type>> : public db_field_holder {
		public:
			using db_field_holder::db_field_holder;
			void add(
				const db_row& row, std::size_t field_idx,
				boost::json::object& obj) {
				if (!row[field_idx].is_null()) {
					obj[name_] = row[field_idx].as<Type>();
				}
				else {
					obj[name_] = nullptr;
				}
			}
		};

		template <>
		class db_field<std::optional<std::string>> : public db_field_holder {
		public:
			using db_field_holder::db_field_holder;
			void add(
				const db_row& row, std::size_t field_idx,
				boost::json::object& obj) {
				if (!row[field_idx].is_null()) {
					obj[name_] = row[field_idx].c_str();
				}
				else {
					obj[name_] = nullptr;
				}
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
			: msg_{ msg } {}
		const char* what() const noexcept { return msg_.c_str(); }
	};

	class db_relation_to_object {
	private:
		std::vector<std::shared_ptr<db_internal::db_field_holder>> fields_;
	public:
		db_relation_to_object(
			const std::initializer_list<
			std::shared_ptr<db_internal::db_field_holder>>&fields)
			: fields_{ fields } {}
		boost::json::object convert_row(const db_row& row) {
			boost::json::object obj;
			for (std::size_t i = 0; i < fields_.size(); ++i)
				fields_[i]->add(row, i, obj);
			return obj;
		}
		std::vector<boost::json::object> convert_to_vector(
			const db_result& result) {
			std::vector<boost::json::object> results;
			for (const auto& row : result)
				results.emplace_back(convert_row(row));
			return results;
		}
		std::optional<boost::json::object> convert_to_optional(
			const db_result& result) {
			// result.size() == 0
			if (result.begin() == result.end()) return std::nullopt;
			auto iterator = result.begin();
			auto first = iterator;
			// result.size() == 1
			if (++iterator == result.end())
				return convert_row(*first);
			// result.size() > 1
			throw invalid_operation_exception{
				"too many objects to convert" };
		}
	};

	class db_transaction {
	private:
		raw_db_transaction_type tx_;
	public:
		db_transaction(
			std::shared_ptr<db_connection> connection_ptr
		) : tx_{ connection_ptr->get() } {}
		// non-copiable, non-assignable
		db_transaction(const db_transaction&) = delete;
		db_transaction& operator=(const db_transaction&) = delete;
		// Usage:
		// exec("select * from ? where ? = ? and first_name = 'Name??'",
		//      db_name("auth_user"), db_name("is_active"), db_value<bool>(true));
		// -> SQL: select * from "auth_user" where "is_active" = true and first_name = 'Name?'
		// ======================================================================================
		// exec("select * from ? where ? = ? and first_name = ?",
		//      db_name("auth_user"), db_name("is_active"), false, "Name??");
		// -> SQL: select * from "auth_user" where "is_active" = false and first_name = 'Name??'
		// ======================================================================================
		// Note: "?" is the placeholder for parameters, and "??" will be converted to "?" in SQL.
		//       But, "??" in the parameters remains.
		template <typename ...Params>
		db_result exec(const std::string& s, const Params&... params) {
			std::vector<std::string> param_vec =
				db_internal::convert_parameters(
					tx_, db_internal::convert_parameter(params)...);
			std::size_t idx = 0;
			std::string query;
			for (std::size_t i = 0; i < s.length(); ++i) {
				if (s[i] == '?') {
					if (i + 1 < s.length() && s[i + 1] == '?') {
						query += s[++i];
					}
					else {
						if (idx < param_vec.size()) {
							query += param_vec[idx++];
						}
						else throw std::out_of_range{ "too few parameters" };
					}
				}
				else query += s[i];
			}
			if (idx != param_vec.size())
				throw invalid_operation_exception{ "too many parameters" };
			return tx_.exec(query);
		}
		void commit() { tx_.commit(); }
		void abort() { tx_.abort(); }
	};


	// TODO: add support for time conversions between postgresql and c++, use timestamp?
	//       what about time zone?

}  // bserv

#endif  // _DATABASE_HPP