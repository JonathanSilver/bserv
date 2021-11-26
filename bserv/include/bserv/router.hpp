#ifndef _ROUTER_HPP
#define _ROUTER_HPP

#include <boost/asio/spawn.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <string>
#include <regex>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>
#include <optional>

#include <pqxx/pqxx>

#include "client.hpp"
#include "database.hpp"
#include "session.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "websocket.hpp"
#include "logging.hpp"

namespace bserv {

	namespace beast = boost::beast;
	namespace http = beast::http;

	struct server_resources {
		std::shared_ptr<session_manager_base> session_mgr;
		std::shared_ptr<db_connection_manager> db_conn_mgr;
	};

	struct request_resources {
		server_resources& resources;

		asio::io_context& ioc;
		asio::yield_context& yield;
		std::shared_ptr<websocket_session> ws_session;
		const std::vector<std::string>& url_params;
		request_type& request;
		response_type& response;

		std::shared_ptr<session_type> session_ptr;
		std::shared_ptr<db_connection> db_connection_ptr;
		std::shared_ptr<http_client> http_client_ptr;
		std::shared_ptr<websocket_server> websocket_server_ptr;
	};

	namespace placeholders {

		template <int N>
		struct placeholder {};

#define make_place_holder(x) constexpr placeholder<x> _##x

		make_place_holder(1);
		make_place_holder(2);
		make_place_holder(3);
		make_place_holder(4);
		make_place_holder(5);
		make_place_holder(6);
		make_place_holder(7);
		make_place_holder(8);
		make_place_holder(9);

#undef make_place_holder

		// std::shared_ptr<bserv::session_type>
		constexpr placeholder<-1> session;
		// bserv::request_type&
		constexpr placeholder<-2> request;
		// bserv::response_type&
		constexpr placeholder<-3> response;
		// boost::json::object&&
		constexpr placeholder<-4> json_params;
		// std::shared_ptr<bserv::db_connection>
		constexpr placeholder<-5> db_connection_ptr;
		// std::shared_ptr<bserv::http_client>
		constexpr placeholder<-6> http_client_ptr;
		// std::shared_ptr<bserv::websocket_server>
		constexpr placeholder<-7> websocket_server_ptr;

	}  // placeholders

	class bad_request_exception : public std::exception {
	public:
		bad_request_exception() = default;
		const char* what() const noexcept { return "bad request"; }
	};

	namespace router_internal {

		template <typename ...Types>
		struct parameter_pack;

		template <>
		struct parameter_pack<> {};

		template <typename Head, typename ...Tail>
		struct parameter_pack<Head, Tail...>
			: parameter_pack<Tail...> {
			Head head_;
			template <typename Head2, typename ...Tail2>
			parameter_pack(Head2&& head, Tail2&& ...tail)
				: parameter_pack<Tail...>{ static_cast<Tail2&&>(tail)... },
				head_{ static_cast<Head2&&>(head) } {}
		};

		template <int Idx, typename ...Types>
		struct get_parameter_pack;

		template <int Idx, typename Head, typename ...Tail>
		struct get_parameter_pack<Idx, Head, Tail...>
			: get_parameter_pack<Idx - 1, Tail...> {};

		template <typename Head, typename ...Tail>
		struct get_parameter_pack<0, Head, Tail...> {
			using type = parameter_pack<Head, Tail...>;
		};

		template <int Idx, typename ...Types>
		decltype(auto) get_parameter_value(parameter_pack<Types...>& params) {
			return (static_cast<
				typename get_parameter_pack<Idx, Types...>::type&
			>(params).head_);
		}

		template <int Idx, typename ...Types>
		struct get_parameter;

		template <int Idx, typename Head, typename ...Tail>
		struct get_parameter<Idx, Head, Tail...>
			: get_parameter<Idx - 1, Tail...> {};

		template <typename Head, typename ...Tail>
		struct get_parameter<0, Head, Tail...> {
			using type = Head;
		};

		template <typename Type>
		Type&& get_parameter_data(
			request_resources&, Type&& val) {
			return static_cast<Type&&>(val);
		}

		template <int N, std::enable_if_t<(N >= 0), int> = 0>
		const std::string& get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<N>) {
			return resources.url_params[N];
		}

		inline std::shared_ptr<session_type> get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-1>) {
			if (resources.session_ptr != nullptr)
				return resources.session_ptr;
			std::string cookie_str{ resources.request[http::field::cookie] };
			auto&& [cookie_dict, cookie_list]
				= utils::parse_params(cookie_str, 0, ';');
			boost::ignore_unused(cookie_list);
			std::string session_id;
			std::shared_ptr<session_type> session_ptr;
			if (cookie_dict.count(SESSION_NAME) != 0) {
				session_id = cookie_dict[SESSION_NAME];
			}
			else if (cookie_list.count(SESSION_NAME) != 0) {
				std::vector<std::string>& session_ids = cookie_list[SESSION_NAME];
				for (auto& sess_id : session_ids) {
					if (resources.resources.session_mgr->try_get(sess_id, session_ptr)) {
						session_id = sess_id;
						break;
					}
				}
			}
			if (session_ptr == nullptr
				&& resources.resources.session_mgr->get_or_create(session_id, session_ptr)) {
				resources.response.set(http::field::set_cookie, SESSION_NAME + "=" + session_id + "; Path=/");
			}
			resources.session_ptr = session_ptr;
			return session_ptr;
		}

		inline request_type& get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-2>) {
			return resources.request;
		}

		inline response_type& get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-3>) {
			return resources.response;
		}

		inline boost::json::object get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-4>) {
			boost::json::object body;
			auto add_to_body = [&body](
				const std::map<std::string, std::string>& dict_param,
				const std::map<std::string, std::vector<std::string>>& list_param) {
					for (auto& [k, v] : dict_param) {
						if (!body.contains(k)) {
							body[k] = v;
						}
					}
					for (auto& [k, vs] : list_param) {
						if (!body.contains(k)) {
							boost::json::array a;
							for (auto& v : vs) {
								a.push_back(boost::json::string{ v });
							}
							body[k] = a;
						}
					}
			};
			if (!resources.request.body().empty()) {
				auto& content_type = resources.request[http::field::content_type];
				/*
					for reference:
					Content-Type: text/html; charset=UTF-8
					Content-Type: multipart/form-data; boundary=something
				*/
				std::string media_type;
				for (auto& c : content_type) {
					if (c == ' ') {
						continue;
					}
					else if (c == ';') {
						break;
					}
					media_type += c;
				}
				if (media_type == "application/json") {
					try {
						body = boost::json::parse(resources.request.body()).as_object();
					}
					catch (const std::exception& /*e*/) {
						throw bad_request_exception{};
					}
				}
				else if (media_type == "application/x-www-form-urlencoded") {
					std::string copied_body{ resources.request.body() };
					auto&& [dict_params, list_params] = utils::parse_params(copied_body);
					add_to_body(dict_params, list_params);
				}
			}
			std::string target{ resources.request.target() };
			auto&& [url, dict_params, list_params] = utils::parse_url(target);
			boost::ignore_unused(url);
			add_to_body(dict_params, list_params);
			return body;
		}

		inline std::shared_ptr<db_connection> get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-5>) {
			if (resources.db_connection_ptr == nullptr)
				resources.db_connection_ptr =
				resources.resources.db_conn_mgr->get_or_block();
			return resources.db_connection_ptr;
		}

		inline std::shared_ptr<http_client> get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-6>) {
			if (resources.http_client_ptr == nullptr)
				resources.http_client_ptr =
				std::make_shared<http_client>(resources.ioc, resources.yield);
			return resources.http_client_ptr;
		}

		inline std::shared_ptr<websocket_server> get_parameter_data(
			request_resources& resources,
			placeholders::placeholder<-7>) {
			if (resources.websocket_server_ptr == nullptr)
				resources.websocket_server_ptr =
				std::make_shared<websocket_server>(*resources.ws_session, resources.yield);
			return resources.websocket_server_ptr;
		}

		template <int Idx, typename Func, typename Params, typename ...Args>
		struct path_handler;

		template <int Idx, typename Ret, typename ...Args, typename ...Params>
		struct path_handler<Idx, Ret(*)(Args ...), parameter_pack<Params...>> {
			Ret invoke(request_resources& resources,
				Ret(*pf)(Args ...), parameter_pack<Params...>& params) {
				// suppress msvc warning
				boost::ignore_unused(params);
				boost::ignore_unused(resources);
				if constexpr (Idx == 0) return (*pf)();
				else return static_cast<path_handler<
					Idx - 1, Ret(*)(Args ...), parameter_pack<Params...>,
					typename get_parameter<Idx - 1, Params...>::type>*
				>(this)->invoke2(resources, pf, params,
					get_parameter_data(resources,
						get_parameter_value<Idx - 1>(params)));
			}
		};

		template <int Idx, typename Ret, typename ...Args,
			typename ...Params, typename Head, typename ...Tail>
			struct path_handler<Idx, Ret(*)(Args ...),
			parameter_pack<Params...>, Head, Tail...>
			: path_handler<Idx + 1, Ret(*)(Args ...),
			parameter_pack<Params...>, Tail...> {
			template <
				typename Head2, typename ...Tail2,
				std::enable_if_t<sizeof...(Tail2) == sizeof...(Tail), int> = 0>
				Ret invoke2(request_resources& resources,
					Ret(*pf)(Args ...), parameter_pack<Params...>& params,
					Head2&& head2, Tail2&& ...tail2) {
				// suppress msvc warning
				boost::ignore_unused(params);
				if constexpr (Idx == 0)
					return (*pf)(static_cast<Head2&&>(head2),
						static_cast<Tail2&&>(tail2)...);
				else return static_cast<path_handler<
					Idx - 1, Ret(*)(Args ...), parameter_pack<Params...>,
					typename get_parameter<Idx - 1, Params...>::type, Head, Tail...>*
				>(this)->invoke2(resources, pf, params,
					get_parameter_data(resources,
						get_parameter_value<Idx - 1>(params)),
					static_cast<Head2&&>(head2), static_cast<Tail2&&>(tail2)...);
			}
		};

		const std::vector<std::pair<std::regex, std::string>> url_regex_mapping{
			{std::regex{"<int>"}, "([0-9]+)"},
			{std::regex{"<str>"}, R"(([A-Za-z0-9_\.\-]+))"},
			{std::regex{"<path>"}, R"(([A-Za-z0-9_/\.\-]+))"}
		};

		inline std::string get_re_url(const std::string& url) {
			std::string re_url = url;
			for (auto& [r, s] : url_regex_mapping)
				re_url = std::regex_replace(re_url, r, s);
			return re_url;
		}

		struct path_holder : std::enable_shared_from_this<path_holder> {
			path_holder() = default;
			virtual ~path_holder() = default;
			virtual bool match(
				const std::string&,
				std::vector<std::string>&) const = 0;
			virtual std::optional<boost::json::value> invoke(
				request_resources&) = 0;
		};

		template <typename Func, typename Params>
		class path;

		template <typename Ret, typename ...Args, typename ...Params>
		class path<Ret(*)(Args ...), parameter_pack<Params...>>
			: public path_holder {
		private:
			std::regex re_;
			Ret(*pf_)(Args ...);
			parameter_pack<Params...> params_;
			path_handler<0, Ret(*)(Args ...), parameter_pack<Params...>, Params...> handler_;
		public:
			path(const std::string& url, Ret(*pf)(Args ...), Params&& ...params)
				: re_{ get_re_url(url) }, pf_{ pf },
				params_{ static_cast<Params&&>(params)... } {}
			bool match(const std::string& url, std::vector<std::string>& result) const {
				std::smatch r;
				bool matched = std::regex_match(url, r, re_);
				if (matched) {
					result.clear();
					for (auto& sub : r)
						result.push_back(sub.str());
				}
				return matched;
			}
			std::optional<boost::json::value> invoke(
				request_resources& resources) {
				return handler_.invoke(
					resources, pf_, params_);
			}
		};

	} // router_internal

	template <typename Ret, typename ...Args, typename ...Params>
	std::shared_ptr<router_internal::path<Ret(*)(Args ...),
		router_internal::parameter_pack<Params...>>> make_path(
			const std::string& url, Ret(*pf)(Args ...), Params&& ...params) {
		return std::make_shared<
			router_internal::path<Ret(*)(Args ...),
			router_internal::parameter_pack<Params...>>
			>(url, pf, static_cast<Params&&>(params)...);
	}

	template <typename Ret, typename ...Args, typename ...Params>
	std::shared_ptr<router_internal::path<Ret(*)(Args ...),
		router_internal::parameter_pack<Params...>>> make_path(
			const char* url, Ret(*pf)(Args ...), Params&& ...params) {
		return std::make_shared<
			router_internal::path<Ret(*)(Args ...),
			router_internal::parameter_pack<Params...>>
			>(url, pf, static_cast<Params&&>(params)...);
	}

	class url_not_found_exception : public std::exception {
	public:
		url_not_found_exception() = default;
		const char* what() const noexcept { return "url not found"; }
	};

	class router {
	private:
		using path_holder_type = std::shared_ptr<router_internal::path_holder>;
		std::vector<path_holder_type> paths_;
		std::shared_ptr<server_resources> resources_;
	public:
		router(const std::initializer_list<path_holder_type>& paths)
			: paths_{ paths } {}
		void set_resources(std::shared_ptr<server_resources> resources) {
			resources_ = resources;
		}
		std::optional<boost::json::value> operator()(
			asio::io_context& ioc, asio::yield_context& yield,
			std::shared_ptr<websocket_session> ws_session,
			const std::string& url, request_type& request, response_type& response) {
			std::vector<std::string> url_params;
			for (auto& ptr : paths_) {
				if (ptr->match(url, url_params)) {
					lgtrace << "router: received request: " << url;
					request_resources resources{
						*resources_,

						ioc,
						yield,
						ws_session,
						url_params,
						request,
						response,

						nullptr,
						nullptr,
						nullptr,
						nullptr
					};
					return ptr->invoke(resources);
				}
			}
			throw url_not_found_exception{};
		}
	};

}  // bserv

#endif  // _ROUTER_HPP