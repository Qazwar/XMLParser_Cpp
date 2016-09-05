//
//  xml_document.h
//
//  Copyright © 2016 OTAKE Takayoshi. All rights reserved.
//

#ifndef xml_document_h
#define xml_document_h

#include <exception>
#include <string>
#include <regex>
#include <memory>
#include <map>
#include <vector>

namespace bbxml {

    struct xml_node {
        std::weak_ptr<xml_node> parent;
        std::string name;
        std::map<std::string, std::string> attributes;
        std::string value;
        std::vector<std::shared_ptr<xml_node>> nodes;
        
        std::string inner_text() const noexcept;
    };

    struct xml_document {
        std::string version;
        std::map<std::string, std::string> attributes;
        std::shared_ptr<xml_node> root_node;

		std::string description() const noexcept;
    };
    
    class xml_error : public std::exception {};
    
    extern xml_document parse_xml(const std::string& text);
    
}

#endif /* xml_document_h */
