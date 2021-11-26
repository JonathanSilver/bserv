#include "pch.h"
#include "bserv/utils.hpp"
#include "bserv/router.hpp"

#include <mutex>
#include <sstream>
#include <iomanip>
#include <fstream>

#include <cryptopp/cryptlib.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <cryptopp/base64.h>

namespace bserv::utils {

	namespace internal {

		// NOTE:
		// - `random_device` is implementation dependent.
		//   it doesn't work with GNU GCC on Windows.
		// - for thread-safety, do not directly use it.
		//   use `get_rd_value` instead.
		std::random_device rd;
		std::mutex rd_mutex;

		std::random_device::result_type get_rd_value() {
			std::lock_guard<std::mutex> lg{ rd_mutex };
			return rd();
		}

		// const std::string chars = "abcdefghijklmnopqrstuvwxyz"
		//                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		//                           "1234567890"
		//                           "!@#$%^&*()"
		//                           "`~-_=+[{]}\\|;:'\",<.>/? ";

		const std::string chars = "abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"1234567890";


		const std::string url_safe_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789-._~";

	}  // internal

	// https://www.boost.org/doc/libs/1_75_0/libs/random/example/password.cpp
	std::string generate_random_string(std::size_t len) {
		std::string s;
		std::mt19937 rng{ internal::get_rd_value() };
		std::uniform_int_distribution<> dist{ 0, (int)internal::chars.length() - 1 };
		for (std::size_t i = 0; i < len; ++i) s += internal::chars[dist(rng)];
		return s;
	}

	namespace security {

		// https://codahale.com/a-lesson-in-timing-attacks/
		bool constant_time_compare(const std::string& a, const std::string& b) {
			if (a.length() != b.length())
				return false;
			int result = 0;
			for (std::size_t i = 0; i < a.length(); ++i)
				result |= a[i] ^ b[i];
			return result == 0;
		}

		// https://cryptopp.com/wiki/PKCS5_PBKDF2_HMAC
		std::string hash_password(
			const std::string& password,
			const std::string& salt,
			unsigned int iterations) {
			using namespace CryptoPP;
			byte derived[SHA256::DIGESTSIZE];
			PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
			byte unused = 0;
			pbkdf.DeriveKey(derived, sizeof(derived), unused,
				(const byte*)password.c_str(), password.length(),
				(const byte*)salt.c_str(), salt.length(),
				iterations, 0.0f);
			std::string result;
			Base64Encoder encoder{ new StringSink{result}, false };
			encoder.Put(derived, sizeof(derived));
			encoder.MessageEnd();
			return result;
		}

		std::string encode_password(const std::string& password) {
			std::string salt = generate_random_string(16);
			std::string hashed_password = hash_password(password, salt);
			return salt + '$' + hashed_password;
		}

		bool check_password(const std::string& password,
			const std::string& encoded_password) {
			std::string salt, hashed_password;
			std::string* a = &salt, * b = &hashed_password;
			for (std::size_t i = 0; i < encoded_password.length(); ++i) {
				if (encoded_password[i] != '$') {
					(*a) += encoded_password[i];
				}
				else {
					std::swap(a, b);
				}
			}
			return constant_time_compare(
				hash_password(password, salt), hashed_password);
		}

	}  // security


	// reference for url:
	// https://www.ietf.org/rfc/rfc3986.txt

	// reserved    = gen-delims / sub-delims
	// gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"
	// sub-delims  = "!" / "$" / "&" / "'" / "(" / ")"
	//             / "*" / "+" / "," / ";" / "="

	// unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"

	// https://stackoverflow.com/questions/54060359/encoding-decoded-urls-in-c
	// there can be exceptions (std::stoi)!
	std::string decode_url(const std::string& s) {
		std::string r;
		for (std::size_t i = 0; i < s.length(); ++i) {
			if (s[i] == '%') {
				int v = std::stoi(s.substr(i + 1, 2), nullptr, 16);
				r.push_back(0xff & v);
				i += 2;
			}
			else if (s[i] == '+') r.push_back(' ');
			else r.push_back(s[i]);
		}
		return r;
	}

	std::string encode_url(const std::string& s) {
		std::ostringstream oss;
		for (auto& c : s) {
			if (internal::url_safe_characters.find(c) != std::string::npos) {
				oss << c;
			}
			else {
				oss << '%' << std::setfill('0') << std::setw(2) <<
					std::uppercase << std::hex << (0xff & c);
			}
		}
		return oss.str();
	}

	std::pair<
		std::map<std::string, std::string>,
		std::map<std::string, std::vector<std::string>>>
		parse_params(std::string& s, std::size_t start_pos, char delimiter) {
		std::map<std::string, std::string> dict_params;
		std::map<std::string, std::vector<std::string>> list_params;
		// we use the swap pointer technique
		// we will always append characters to *a only.
		std::string key, value, * a = &key, * b = &value;
		// append an extra `delimiter` so that the last key-value pair
		// is processed just like the other.
		s.push_back(delimiter);
		for (std::size_t i = start_pos; i < s.length(); ++i) {
			if (s[i] == '=') {
				std::swap(a, b);
			}
			else if (s[i] == delimiter) {
				// swap(a, b);
				a = &key;
				b = &value;
				// prevent ending with ' '
				while (!key.empty() && key.back() == ' ') key.pop_back();
				while (!value.empty() && value.back() == ' ') value.pop_back();
				if (key.empty() && value.empty())
					continue;
				key = decode_url(key);
				value = decode_url(value);
				// if `key` is in `list_params`, append `value`.
				if (list_params.find(key) != list_params.end()) {
					list_params[key].push_back(value);
				}
				else { // `key` is not in `list_params`
					auto p = dict_params.find(key);
					// if `key` is in `dict_params`, 
					// move previous value and `value` to `list_params`
					// and remove `key` in `dict_params`.
					if (p != dict_params.end()) {
						list_params[key] = { p->second, value };
						dict_params.erase(p);
					}
					else { // `key` is not in `dict_params`
						dict_params[key] = value;
					}
				}
				// clear `key` and `value`
				key = "";
				value = "";
			}
			else {
				// prevent beginning with ' '
				if (a->empty() && s[i] == ' ') {
					continue;
				}
				(*a) += s[i];
			}
		}
		// remove the last `delimiter` to restore `s` to what it was.
		s.pop_back();
		return std::make_pair(dict_params, list_params);
	}

	std::tuple<std::string,
		std::map<std::string, std::string>,
		std::map<std::string, std::vector<std::string>>>
		parse_url(std::string& s) {
		std::string url;
		std::size_t i = 0;
		for (; i < s.length(); ++i) {
			if (s[i] != '?') {
				url += s[i];
			}
			else {
				break;
			}
		}
		if (i == s.length())
			return std::make_tuple(url,
				std::map<std::string, std::string>{},
				std::map<std::string, std::vector<std::string>>{});
		auto&& [dict_params, list_params] = parse_params(s, i + 1);
		return std::make_tuple(url, dict_params, list_params);
	}

	namespace file {

		std::string read_bin(const std::string& filename) {
			std::ifstream fin(filename, std::ios_base::in | std::ios_base::binary);
			if (!fin.is_open()) throw file_not_found{ filename };
			std::string res;
			while (true) {
				char c = (char)fin.get();
				if (fin.eof()) break;
				res += c;
			}
			return res;
		}

		// returns a reasonable mime type based on the extension of a file.
		boost::beast::string_view
			mime_type(boost::beast::string_view path) {
			using boost::beast::iequals;
			auto const ext = [&path] {
				auto const pos = path.rfind(".");
				if (pos == boost::beast::string_view::npos)
					return boost::beast::string_view{};
				return path.substr(pos);
			}();
			if (iequals(ext, ".htm"))  return "text/html";
			if (iequals(ext, ".html")) return "text/html";
			if (iequals(ext, ".php"))  return "text/html";
			if (iequals(ext, ".css"))  return "text/css";
			if (iequals(ext, ".txt"))  return "text/plain";
			if (iequals(ext, ".js"))   return "application/javascript";
			if (iequals(ext, ".json")) return "application/json";
			if (iequals(ext, ".xml"))  return "application/xml";
			if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
			if (iequals(ext, ".flv"))  return "video/x-flv";
			if (iequals(ext, ".png"))  return "image/png";
			if (iequals(ext, ".jpe"))  return "image/jpeg";
			if (iequals(ext, ".jpeg")) return "image/jpeg";
			if (iequals(ext, ".jpg"))  return "image/jpeg";
			if (iequals(ext, ".gif"))  return "image/gif";
			if (iequals(ext, ".bmp"))  return "image/bmp";
			if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
			if (iequals(ext, ".tiff")) return "image/tiff";
			if (iequals(ext, ".tif"))  return "image/tiff";
			if (iequals(ext, ".svg"))  return "image/svg+xml";
			if (iequals(ext, ".svgz")) return "image/svg+xml";
			return "application/text";
		}

		std::nullopt_t serve(
			response_type& response,
			const std::string& filename) {
			response.set(bserv::http::field::content_type, mime_type(filename));
			try {
				response.body() = read_bin(filename);
			}
			catch (const file_not_found&) {
				throw url_not_found_exception{};
			}
			response.prepare_payload();
			return std::nullopt;
		}

	}  // file

}  // bserv::utils