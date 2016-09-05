//
//  xml_document.cpp
//
//  Copyright © 2016 OTAKE Takayoshi. All rights reserved.
//

#include "xml_document.h"

#include <assert.h>
#include <iostream>
#include <sstream>
#include <queue>

using namespace bbxml;

//*/
#define make_xml_error(what) xml_error_impl(what, __FILE__, __LINE__)
/*/
#define make_xml_error(what) xml_error_impl(what, nullptr, 0)
//*/

namespace {
    inline std::string position_of(std::string::const_iterator itr, std::string::const_iterator from);
    inline bool is_space(std::string::const_iterator itr, std::string::const_iterator end);
}

namespace bb {
    struct string_cursor {
        std::string::const_iterator begin;
        std::string::const_iterator end;
        std::string::const_iterator current;
    };
    
    string_cursor make_string_cursor(const std::string& str) {
        return {str.cbegin(), str.cend(), str.cbegin()};
    }
    
    bool regex_search(bb::string_cursor& cursor, std::smatch& m, const std::regex& e) {
        if (std::regex_search(cursor.current, cursor.end, m, e)) {
            cursor.current += m[0].length();
            return true;
        }
        return false;
    }
}

namespace {
	inline std::map<std::string, std::string> parse_xml_attributes(std::string::const_iterator itr, std::string::const_iterator end);
    inline void validate_tag_name(std::string::const_iterator itr, std::string::const_iterator end);
    inline std::string unescape_xml_inner_text(std::string::const_iterator itr, std::string::const_iterator end);
	inline std::string unescape_xml_attribute_value(std::string::const_iterator itr, std::string::const_iterator end);
	inline std::string unescape_xml_attribute_value_with_apos(std::string::const_iterator itr, std::string::const_iterator end);
	inline std::string _unescape_xml_entity(std::string::const_iterator itr, std::string::const_iterator end, const std::regex& illegal_re);
}

struct xml_error_impl : public xml_error {
public:
    mutable std::string what_;
    xml_error_impl(const std::string& what, const char* file, const int line) {
        std::ostringstream oss;
        oss << what;
        if (file) {
            oss << std::endl << "  at " << file << ":" << line;
        }
        what_ = oss.str();
    }
    
    virtual const char* what() const noexcept override {
        return what_.c_str();
    }
};

xml_document bbxml::parse_xml(const std::string& text) {
    auto cursor = bb::make_string_cursor(text);

    std::string doc_version;
    std::map<std::string, std::string> doc_attributes;
    {
        // Searches a XML declaration -> (version, attributes?)
        static const std::regex re{R"(^<\?xml\s+version=\"(.+?)\"([\s\S]*?)\?>)"};
        std::smatch m;
        if (!bb::regex_search(cursor, m, re)) {
            throw make_xml_error("No XML declaration: " + position_of(cursor.current, cursor.begin));
        }
        
        std::string version = m[1];
        if (version.compare("1.0") != 0) {
            std::ostringstream oss;
            oss << "Unsupported XML version \"" << version << "\": " << position_of(m[1].first, cursor.begin);
            throw make_xml_error(oss.str());
        }
        
        doc_version.assign(version);
        try {
            doc_attributes = parse_xml_attributes(m[2].first, m[2].second);
        }
        catch (const std::string::const_iterator& itr) {
            throw make_xml_error("Illegal attributes: " + position_of(itr, cursor.begin));
        }
    }
    

    auto top_node = std::make_shared<xml_node>();
    auto current_node = top_node;
    {
        std::string inner_text_before_tag;
        while (cursor.current < cursor.end) {
            // Searches the head of an tag -> (inner_text?, tag_name?)
            std::smatch::value_type tag_name;    // may be CDATA section name "![CDATA["
            {
                std::smatch m;
                static const std::regex re{R"(^([^<]*?)<((!--|!\[CDATA\[|[^>\s]*)))"};
                if (!bb::regex_search(cursor, m, re)) {
                    break;
                }
                try {
                    inner_text_before_tag += unescape_xml_inner_text(m[1].first, m[1].second);
                }
                catch (const std::string::const_iterator& itr) {
                    throw make_xml_error("Found an unescaped character or an undefined entity: " + position_of(itr, cursor.begin));
                }
                tag_name = m[2];
            }
            
            if (tag_name.compare("!--") == 0) {
                // Searches an end of the the comment section; Skips the comment
                std::smatch m;
                static const std::regex re{R"(^[^-]*(--[\s\S]?))"};
                if (!bb::regex_search(cursor, m, re)) {
                    throw make_xml_error("Missing an end of the comment section: " + position_of(tag_name.first, text.cbegin()));
                }
                if (m[1].compare("-->") != 0) {
                    throw make_xml_error("Two dashes in the middle of a comment are not allowed: " + position_of(m[1].first, text.cbegin()));
                }
            }
            else if (tag_name.compare("![CDATA[") == 0) {
                // Searches an end of the CDATA section
                std::smatch m;
                static const std::regex re{R"(^([\s\S]*?\]\]>))"};
                if (!bb::regex_search(cursor, m, re)) {
                    throw make_xml_error("Missing an end of the CDATA section: " + position_of(tag_name.first, text.cbegin()));
                }
                
				/*/
                // Includes "<![CDATA[" and "]]>"
                inner_text_before_tag += std::string(tag_name.first - 1, m[1].second);
				/*/
				// Excludes "<![CDATA[" and "]]>"
				inner_text_before_tag += std::string(tag_name.second, m[1].second - 3);
				//*/
            }
            else {
                if (tag_name.length() == 0) {
                    throw make_xml_error("Found a no name tag: " + position_of(tag_name.first, text.cbegin()));
                }
                validate_tag_name(tag_name.first, tag_name.second);
                
                // Searches ">" -> (attributes?, "/"?)
                std::smatch m;
                static const std::regex re{R"(^([\s\S]*?)(/?)>)"};
                if (!bb::regex_search(cursor, m, re)) {
                    throw make_xml_error("Missing \">\" for the tag \"" + std::string(tag_name) + "\": " + position_of(tag_name.first, text.cbegin()));
                }
				std::map<std::string, std::string> attributes;
                try {
					attributes = parse_xml_attributes(m[1].first, m[1].second);
                }
                catch (const std::string::const_iterator& itr) {
                    throw make_xml_error("Illegal attributes: " + position_of(itr, cursor.begin));
                }
                const auto& independent_mark = m[2];
                
                // text
                if (!is_space(inner_text_before_tag.cbegin(), inner_text_before_tag.cend())) {
                    auto text_node = std::make_shared<xml_node>();
                    text_node->parent = current_node;
                    text_node->name = "#text";
                    text_node->value = std::move(inner_text_before_tag);
                    current_node->nodes.push_back(text_node);
                }
                else {
                    inner_text_before_tag.clear();
                }
                
                if (*tag_name.first == '/') { // Closing tag
                    if (independent_mark.length() != 0) {
                        throw make_xml_error("Closing tag can not end with \"/>\", \"" + std::string(tag_name) + "\": " + position_of(independent_mark.first, text.cbegin()));
                    }
                    if (current_node->name.empty()) {
                        if (tag_name.compare("/" + current_node->name) != 0) {
                            throw make_xml_error("Missing an opening tag for the tag \"" + std::string(tag_name) + "\": " + position_of(tag_name.first, text.cbegin()));
                        }
                    }
                    else {
                        if (tag_name.compare("/" + current_node->name) != 0) {
                            throw make_xml_error("Missing an closing tag for the tag \"" + current_node->name + "\": " + position_of(tag_name.first, text.cbegin()));
                        }
                    }
					if (!attributes.empty()) {
						throw make_xml_error("Closing tag can not have attributes, \"" + std::string(tag_name) + "\": " + position_of(tag_name.first, text.cbegin()));
					}
					if (current_node->nodes.size() == 1 && current_node->nodes[0]->name.compare("#text") == 0) {
                        current_node->value = std::move(current_node->nodes[0]->value);
                        current_node->nodes.clear();
					}
                    current_node = current_node->parent.lock();
                    assert(current_node != nullptr);
                }
                else if (independent_mark.length() != 0) { // Independent tag
                    auto node = std::make_shared<xml_node>();
                    node->parent = current_node;
                    node->name = std::move(tag_name);
					node->attributes = std::move(attributes);
                    current_node->nodes.push_back(node);
                }
                else { // Opening tag
                    auto node = std::make_shared<xml_node>();
                    node->parent = current_node;
                    node->name = std::move(tag_name);
					node->attributes = std::move(attributes);
                    current_node->nodes.push_back(node);
                    current_node = std::move(node);
                }
                
                assert(inner_text_before_tag.empty());
            }
        }
        
        // Skips comments if existed
        {
            std::smatch m;
            static const std::regex re{R"(^([^<]*?)(<!--))"};
            while (bb::regex_search(cursor, m, re)) {
                const auto& tag_name = m[2];
                
                static const std::regex re{R"(^[^-]*(--[\s\S]?))"};
                std::smatch m;
                if (!bb::regex_search(cursor, m, re)) {
                    throw make_xml_error("Missing an end of the comment section: " + position_of(tag_name.first, text.cbegin()));
                }
                if (m[1].compare("-->") != 0) {
                    throw make_xml_error("Two dashes in the middle of a comment are not allowed: " + position_of(m[1].first, text.cbegin()));
                }
            }
        }
        // Ignores spaces if existed
        if (!is_space(cursor.current, cursor.end)) {
            throw make_xml_error("Illegal format: " + position_of(cursor.current, cursor.begin));
        }
    }
    // XML document has exactly one single root element.
    std::shared_ptr<xml_node> root_node;
    if (top_node->nodes.size() > 0) {
        root_node = top_node->nodes.front();
        root_node->parent.reset();
    }
    return xml_document { doc_version, doc_attributes, root_node };
}


std::string xml_node::inner_text() const noexcept {
    std::ostringstream oss;
    oss << value;
    for (auto node : nodes) {
        oss << node->inner_text();
    }
    return oss.str();
}

namespace {
	inline std::string description(const xml_node& node, int indent) noexcept {
		std::ostringstream oss;
		oss << std::string(indent, ' ') << "+ " << node.name;
		for (auto attribute : node.attributes) {
			oss << ", " << attribute.first << "=" << attribute.second;
		}
		if (!node.value.empty()) {
			oss << ", " << node.value;
		}
		oss << std::endl;
		for (auto child : node.nodes) {
			oss << ::description(*child, indent + 1);
		}
		return oss.str();
	}
}

std::string xml_document::description() const noexcept {
	std::ostringstream oss;
	oss << "XML version=" << version << std::endl;
	oss << ::description(*root_node, 0);
	return oss.str();
}

namespace {
    
    inline std::string position_of(std::string::const_iterator itr, std::string::const_iterator from) {
        size_t line = 1;
        std::regex re{R"(.*\n)"};
        std::smatch m;
        while (std::regex_search(from, itr, m ,re)) {
            line += 1;
            from += m[0].length();
        }
        
        return "on line " + std::to_string(line) + " at column " + std::to_string(itr - from + 1);
    }
    
    inline bool is_space(std::string::const_iterator itr, std::string::const_iterator end) {
        if (itr >= end) {
            return true;
        }
        static const std::regex re{R"(^\s*$)"};
        return std::regex_match(itr, end, re);
    }


	/**
	 * e.g.
	 * R"(key1="value1" key2='value2')" => { { key1, value1 }, { key2, value2 } }
	 */
	inline std::map<std::string, std::string> parse_xml_attributes(std::string::const_iterator itr, std::string::const_iterator end) {
		std::map<std::string, std::string> attributes;

		std::smatch m;
		static const std::regex re{ R"(\s+([^=]+)=([\"'])([\s\S]*?)\2)" };
		while (std::regex_search(itr, end, m, re)) {
			std::string value;
			if (m[2].compare("\"") == 0) {
				value = unescape_xml_attribute_value(m[3].first, m[3].second);
			}
			else { // m[2] == "'"
				value = unescape_xml_attribute_value_with_apos(m[3].first, m[3].second);
			}
			attributes[m[1]] = value;
			itr += m[0].length();
		}
		if (itr < end) {
			static const std::regex re{ R"(^\s+$)" };
			if (!std::regex_match(itr, end, re)) {
				throw itr;
			}
		}

		return std::move(attributes);
	}
    
    /**
     * TODO: Validates XML tag name
     */
    inline void validate_tag_name(std::string::const_iterator itr, std::string::const_iterator end) {
        std::smatch m;
        static const std::regex re{R"([<>'\"&])"};
        if (std::regex_search(itr, end, m, re)) {
            throw m[0].first;
        }
    }
    
    /**
     * Validates XML escaping, and Unescapes escaped XML
     *
     * "&lt;" -> "<"
     * "&gt;" -> ">"
     * "&apos;" -> "'"
     * "&quot;" -> "\""
     * "&amp;" -> "&"
     */
    inline std::string unescape_xml_inner_text(std::string::const_iterator itr, std::string::const_iterator end) {
		//static const std::regex re{R"([<>'\"])"};
		static const std::regex re{ R"([<])" };	// HACK: ">", "'", "\"" are not always escaped
		return _unescape_xml_entity(itr, end, re);
    }

	inline std::string unescape_xml_attribute_value(std::string::const_iterator itr, std::string::const_iterator end) {
		static const std::regex re{ R"([<'\"])" };
		return _unescape_xml_entity(itr, end, re);
	}

	inline std::string unescape_xml_attribute_value_with_apos(std::string::const_iterator itr, std::string::const_iterator end) {
		static const std::regex re{ R"([<'])" };
		return _unescape_xml_entity(itr, end, re);
	}

	/**
	 * @param illegal_re illegal regex is not related with "&"
	 */
	inline std::string _unescape_xml_entity(std::string::const_iterator itr, std::string::const_iterator end, const std::regex& illegal_re) {
		// Validates (1)
		{
			std::smatch m;
			if (std::regex_search(itr, end, m, illegal_re)) {
				throw m[0].first;
			}
		}
		// Validates (2)
		{
			std::smatch m;
			static const std::regex re{ R"(&(?!lt;|gt;|apos;|quot;|amp;|#\d+;|#x[0-9a-fA-F]{4};))" };
			if (std::regex_search(itr, end, m, re)) {
				throw m[0].first;
			}
		}
		// Unescapes XML entities
		auto replaced = std::string(itr, end);
		replaced = std::regex_replace(replaced, std::regex{ R"(&lt;)" }, "<");
		replaced = std::regex_replace(replaced, std::regex{ R"(&gt;)" }, ">");
		replaced = std::regex_replace(replaced, std::regex{ R"(&apos;)" }, "'");
		replaced = std::regex_replace(replaced, std::regex{ R"(&quot;)" }, "\"");
		replaced = std::regex_replace(replaced, std::regex{ R"(&amp;)" }, "&");
		return replaced;
	}

}