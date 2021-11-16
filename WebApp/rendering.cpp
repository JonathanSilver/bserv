#include "rendering.h"

#include <fstream>

#include <boost/beast.hpp>
#include <inja/inja.hpp>

std::string template_root_;
std::string static_root_;
inja::Environment env;

void init_rendering(const std::string& template_root) {
	template_root_ = template_root;
	if (template_root_[template_root_.size() - 1] != '/')
		template_root_.push_back('/');
}

void init_static_root(const std::string& static_root) {
	static_root_ = static_root;
	if (static_root_[static_root_.size() - 1] != '/')
		static_root_.push_back('/');
}

std::nullopt_t render(
	bserv::response_type& response,
	const std::string& template_file,
	const boost::json::object& context) {
	response.set(bserv::http::field::content_type, "text/html");
	inja::json data = inja::json::parse(boost::json::serialize(context));
	response.body() = env.render_file(template_root_ + template_file, data);
	response.prepare_payload();
	return std::nullopt;
}

// Return a reasonable mime type based on the extension of a file.
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

std::string read_bin(const std::string& file) {
	std::ifstream fin(file, std::ios_base::in | std::ios_base::binary);
	std::string res;
	while (true) {
		char c = (char)fin.get();
		if (fin.eof()) break;
		res += c;
	}
	return res;
}

std::nullopt_t serve(
	bserv::response_type& response,
	const std::string& file) {
	response.set(bserv::http::field::content_type, mime_type(file));
	response.body() = read_bin(static_root_ + file);
	response.prepare_payload();
	return std::nullopt;
}
