/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/xml_proto/xml_protocol_version.h"
#include "ui/common/include/admindcommand.h"

#include <string.h>
#include <unit_testing.h>


ut_test(add_param_int)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);

    command.add_param("param1", 0);
    command.add_param("param2", -1);
    command.add_param("param3", 1);
    command.add_param("param4", 42);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\" value=\"0\"/>"
        "<param name=\"param2\" value=\"-1\"/>"
        "<param name=\"param3\" value=\"1\"/>"
        "<param name=\"param4\" value=\"42\"/>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());

    UT_ASSERT_EQUAL_STR("mycommand param1=\"0\" param2=\"-1\" param3=\"1\" param4=\"42\"", command.get_summary().c_str());
}


ut_test(add_param_bool)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);

    command.add_param("param1", true);
    command.add_param("param2", false);
    command.add_param("param3", 42 == 42);
    command.add_param("param4", 42 != 42);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\" value=\"TRUE\"/>"
        "<param name=\"param2\" value=\"FALSE\"/>"
        "<param name=\"param3\" value=\"TRUE\"/>"
        "<param name=\"param4\" value=\"FALSE\"/>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());
}


ut_test(add_param_string)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);

    command.add_param("param1", "");
    command.add_param("param2", "foo");
    command.add_param("param3", "bar");
    command.add_param("param4", "Hello World!");
    command.add_param("param5", std::string("gloubi"));
    command.add_param("param6", std::string("boulga"));

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\" value=\"\"/>"
        "<param name=\"param2\" value=\"foo\"/>"
        "<param name=\"param3\" value=\"bar\"/>"
        "<param name=\"param4\" value=\"Hello World!\"/>"
        "<param name=\"param5\" value=\"gloubi\"/>"
        "<param name=\"param6\" value=\"boulga\"/>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());
}


ut_test(add_param_double)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);

    command.add_param("param1", 3.14159);
    command.add_param("param2", 2.718);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\" value=\"3.14159\"/>"
        "<param name=\"param2\" value=\"2.718\"/>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());
}


ut_test(add_param_uuid)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);
    exa_uuid_t uuid1 = { {0xC, 0xA, 0xF, 0xE} };
    exa_uuid_t uuid2 = { {0xCCCCCCCC, 0xAAAAAAAA, 0xFFFFFFFF, 0xEEEEEEEE} };

    command.add_param("param1", uuid1);
    command.add_param("param2", uuid2);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\" value=\"0000000C:0000000A:0000000F:0000000E\"/>"
        "<param name=\"param2\" value=\"CCCCCCCC:AAAAAAAA:FFFFFFFF:EEEEEEEE\"/>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());
}


ut_test(add_param_xmlNodePtr)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);
    xmlNodePtr xml_root;
    xmlNodePtr xml_child;

    // Build the following xmlNodePtr:
    // <fromage><savoie nom="reblochon"/></fromage>
    xml_root  = xmlNewNode(NULL, BAD_CAST("fromage"));
    xml_child = xmlNewNode(NULL, BAD_CAST("savoie"));
    xmlSetProp(xml_child, BAD_CAST("nom"), BAD_CAST("reblochon"));
    xmlAddChild(xml_root, xml_child);

    command.add_param("param1", xml_root);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\"><fromage><savoie nom=\"reblochon\"/></fromage></param>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());

    xmlFreeNode(xml_root);
}


ut_test(add_param_xmlDocPtr)
{
    exa_uuid_t cluster_uuid = { {1, 2, 3, 4} };
    AdmindCommand command("mycommand", cluster_uuid);
    xmlDocPtr xml_doc;
    xmlNodePtr xml_root;
    xmlNodePtr xml_child;

    // Build the following XML document:
    // <?xml version="1.0"?><fromage><normandie nom="camembert"/></fromage>
    xml_doc = xmlNewDoc(BAD_CAST("1.0"));
    xml_root  = xmlNewNode(NULL, BAD_CAST("fromage"));
    xmlDocSetRootElement(xml_doc, xml_root);
    xml_child = xmlNewNode(NULL, BAD_CAST("normandie"));
    xmlSetProp(xml_child, BAD_CAST("nom"), BAD_CAST("camembert"));
    xmlAddChild(xml_root, xml_child);

    command.add_param("param1", xml_doc);

    std::string expected =
        "<?xml version=\"1.0\"?>\n"
        "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">"
        "<cluster uuid=\"00000001:00000002:00000003:00000004\"/>"
        "<command name=\"mycommand\">"
        "<param name=\"param1\"><fromage><normandie nom=\"camembert\"/></fromage></param>"
        "</command></Admind>";

    UT_ASSERT_EQUAL_STR(expected.c_str(),
                        command.get_xml_command(false).c_str());

    xmlFreeDoc(xml_doc);
}
