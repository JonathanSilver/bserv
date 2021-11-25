#include "rendering.h"

#include <fstream>

#include <boost/beast.hpp>
#include <inja/inja.hpp>

std::string template_root_;
std::string static_root_;

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
	response.body() = inja::Environment{}.render_file(template_root_ + template_file, data);
	response.prepare_payload();
	return std::nullopt;
}

std::nullopt_t serve(
	bserv::response_type& response,
	const std::string& file) {
	return bserv::utils::file::serve(response, static_root_ + file);
}
