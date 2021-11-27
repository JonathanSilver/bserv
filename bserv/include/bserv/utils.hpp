#ifndef _UTILS_HPP
#define _UTILS_HPP

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>
#include <map>
#include <random>
#include <optional>

#include "client.hpp"

namespace bserv::utils {

	namespace internal {

		std::random_device::result_type get_rd_value();

	}  // internal

	std::string generate_random_string(std::size_t len);

	namespace security {

		bool constant_time_compare(const std::string& a, const std::string& b);

		std::string hash_password(
			const std::string& password,
			const std::string& salt,
			unsigned int iterations = 20000 /*320000*/);

		std::string encode_password(const std::string& password);

		bool check_password(const std::string& password,
			const std::string& encoded_password);

	}  // security

	// there can be exceptions (std::stoi)!
	std::string decode_url(const std::string& s);

	std::string encode_url(const std::string& s);

	// this function parses param list in the form of k1=v1&k2=v2...,
	// where '&' can be any delimiter.
	// ki and vi will be converted if they are percent-encoded,
	// which is why the returned values are `string`, not `string_view`.
	std::pair<
		std::map<std::string, std::string>,
		std::map<std::string, std::vector<std::string>>>
		parse_params(std::string& s, std::size_t start_pos = 0, char delimiter = '&');

	// this function parses url in the form of [url]?k1=v1&k2=v2...
	// this function will convert ki and vi if they are percent-encoded.
	// NOTE: don't misuse this function, it's going to modify
	//       the parameter `s` in place!
	std::tuple<std::string,
		std::map<std::string, std::string>,
		std::map<std::string, std::vector<std::string>>>
		parse_url(std::string& s);

	namespace file {

		class file_not_found : public std::exception {
		private:
			std::string msg_;
		public:
			file_not_found(const std::string& filename)
				: msg_{ std::string{ "'" } + filename + "' does not exist" } {}
			const char* what() const noexcept { return msg_.c_str(); }
		};

		std::string read_bin(const std::string& filename);

		std::nullopt_t serve(
			response_type& response,
			const std::string& filename);

	}  // file

}  // bserv::utils

#endif  // _UTILS_HPP