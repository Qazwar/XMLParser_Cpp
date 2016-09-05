//
//  main.cpp
//
//  Copyright © 2016 OTAKE Takayoshi. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include "assert.h"

#include "xml_document.h"

#define ENABLES_TEST false

#if ENABLES_TEST

void test_xml_declaration() {
    // Empty is error
    try {
        bbxml::parse_xml(R"()");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::no_xml_declaration);
    }
    
    // No version is error
    try {
        bbxml::parse_xml(R"(<?xml?>)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::no_xml_declaration);
    }
    
    // Must start with "<"
    try {
        bbxml::parse_xml(R"( <?xml version="1.0"?>)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::no_xml_declaration);
    }
    
    // Missing "?"
    try {
        bbxml::parse_xml(R"(<xml version="1.0">)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::no_xml_declaration);
    }
    
    // OK
    bbxml::parse_xml(R"(<?xml version="1.0"?>)");
}

void test_xml_version() {
    try {
        bbxml::parse_xml(R"(<?xml version="0.1"?>)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::unsupported_version);
    }
    
    // OK
    assert(bbxml::parse_xml(R"(<?xml version="1.0"?>)").version.compare("1.0") == 0);
}

void test_xml_no_escaped_character() {
    try {
        bbxml::parse_xml(R"(<?xml version="1.0"?><value>&</value>)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::no_escaped_character);
    }
    
    // OK
    bbxml::parse_xml(R"(<?xml version="1.0"?><value>&lt;&gt;&apos;&quot;&amp;</value>)");
}

void test_xml_comment() {
    try {
        bbxml::parse_xml(R"(<?xml version="1.0"?><!--)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::missing_closing_tag);
    }
    
    try {
        bbxml::parse_xml(R"(<?xml version="1.0"?><!---->
<!-- --)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::illegal_comment);
    }

    // OK
    bbxml::parse_xml(R"(<?xml version="1.0"?><!-- This is a comment -->    )");
    
    
    try {
        bbxml::parse_xml(R"(<?xml version="1.0"?><root/><!--)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::missing_closing_tag);
    }
    
    try {
        bbxml::parse_xml(R"(<?xml version="1.0"?><root/><!---->
                         <!-- --)");
        assert(false);
    }
    catch (const bbxml::xml_error& e) {
        assert(e.code() == bbxml::xml_error_code::illegal_comment);
    }
    
    // OK
    bbxml::parse_xml(R"(<?xml version="1.0"?><root/><!-- This is a comment -->    )");
}

#endif

int main(int argc, const char * argv[]) {
#if ENABLES_TEST
    test_xml_declaration();
    test_xml_version();
    test_xml_no_escaped_character();
    test_xml_comment();
#endif
    
    try {
#if 1
		auto doc = bbxml::parse_xml(R"(<?xml version="1.0" encoding="UTF-8"?>
<!--Comment stest stes ets etest -->
<node>
<!--Comment stest stes ets etest -->
    <test value="test">TEST<![CDATA[<ads> Scripting]]>TEST</test>
    <value>
        text before test tag&#160;&lt;&#x2663;
        <test value='"&apos;test"'/>
        text after test tag
    </value>
</node>
)");
#else
		std::ifstream ifs{"sample.xml", std::ios::in};
		std::ostringstream oss;
		oss << ifs.rdbuf();

		auto begin = std::chrono::system_clock::now();
		auto doc = bbxml::parse_xml(oss.str());
		auto end = std::chrono::system_clock::now();
		double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

		std::cout << elapsed << " ms" << std::endl;
#endif
		std::cout << doc.description() << std::endl;
        std::cout << doc.root_node->inner_text() << std::endl;
    }
    catch (const bbxml::xml_error& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
