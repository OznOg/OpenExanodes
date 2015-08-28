/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unit_testing.h>
#include <libxml/tree.h>
#include "common/include/exa_error.h"
#include "admind/src/xml_proto/xml_protocol_version.h"
#include "admind/src/xml_proto/xml_proto_api.h"
#include "admind/src/adm_command.h"
#include "admind/src/commands/command_api.h"

#include "os/include/os_stdio.h"


/* implement dummy commands */
__export(EXA_ADM_CLDELETE) struct dummy_params
  {
    __optional bool optional __default(false);
    uint32_t param1;
    uint64_t param2;
    __optional int32_t param3 __default(0);
    __optional char string[8] __default("");
    __optional xmlDocPtr doc __default(NULL);
  };

/**
 * Definition of the dummy command.
 */
const AdmCommand exa_dummy = {
  .code            = EXA_ADM_CLDELETE, /* Yes I am not using a specific id... */
  .msg             = "my_command",
};
/********************************************************/
#define XML_COMMAND_HEADER "<?xml version=\"1.0\"?> <Admind protocol_release=\"%s\">"   \
                            "<cluster uuid=\"" UUID_FMT "\"/> <command name=\"%s\"> "

#define XML_COMMAND_PARAM1  "<param name=\"param1\" value=\"4321\"/>"
#define XML_COMMAND_PARAM2  "<param name=\"param2\" value=\"1234\"/>"
#define XML_COMMAND_STRING  "<param name=\"string\" value=\"hello!\"/>"
#define XML_COMMAND_XML     "<param name=\"doc\">"  \
                            "<nice_tag> <tag1/> bla bla <tag2></tag2> </nice_tag></param>"

#define XML_COMMAND_TRAILER " </command> </Admind>"
#define XML_COMMAND_FMT XML_COMMAND_HEADER \
                        XML_COMMAND_PARAM1 \
                        XML_COMMAND_PARAM2 \
                        XML_COMMAND_STRING \
                        XML_COMMAND_XML \
                        XML_COMMAND_TRAILER

cl_error_desc_t err_desc;
adm_command_code_t cmd_code;
void *data;
size_t data_size;
const exa_uuid_t ref_uuid = {{ 0x6EE96367, 0x26C79563, 0x448BC52C, 0x6FEF6F92 }};
exa_uuid_t cluster_uuid;
char buffer[sizeof(XML_COMMAND_FMT) + UUID_STR_LEN
	    + 100 /* well 100 is large enougth to put stuff needed by tests.. */];


UT_SECTION(xml_chunk_is_conform_to_protocol)

ut_test(parsing_invalid_buffer)
{
    /* Put junk into buffer */
    os_snprintf(buffer, sizeof(buffer), "Actually, this is not really what I call a valid XML chunk");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

    UT_ASSERT(err_desc.code == -EXA_ERR_CMD_PARSING);
}

ut_test(bad_protocol_version)
{
    /* Make a buffer with a bad xml protocol version */
    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_FMT, "000000",
	     UUID_VAL(&ref_uuid), "a command name");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

    //ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_BAD_PROTOCOL);
}

ut_test(missing_tags)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), "%s", "<?xml version=\"1.0\"?> <Admind protocol_release=\"%s\">"
	                            "<clus"
				    "YERK!"
				    "ter uuid=\"0:0:0:0\"/> <command name=\"%s\"> "
				    "<param name=\"toto\" value=\"371\"/> </command> </Admind>");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_CMD_PARSING);


    os_snprintf(buffer, sizeof(buffer), "%s", "<?xml version=\"1.0\"?> <Ad"
	                            "YERK!"
				    "mind protocol_release=\"%s\">"
	                            "<cluster uuid=\"0:0:0:0\"/> <command name=\"%s\"> "
				    "<param name=\"toto\" value=\"371\"/> </command> </Admind>");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_CMD_PARSING);
}


UT_SECTION(xml_chunk_has_valid_data)

ut_test(bad_command_name)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_FMT, XML_PROTOCOL_VERSION,
	     UUID_VAL(&cluster_uuid), "a command name that do not exist");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_CMD_PARSING);
}

ut_test(parsing_a_valid_xml_command_with_optional)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                            "<param name=\"optional\" value=\"TRUE\"/>"
				    XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

    UT_ASSERT_EQUAL(0, err_desc.code);

    UT_ASSERT(uuid_is_equal(&ref_uuid, &cluster_uuid));
    UT_ASSERT(cmd_code == EXA_ADM_CLDELETE);
}

ut_test(parsing_a_valid_xml_command_without_optional)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_FMT, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

    UT_ASSERT(err_desc.code == 0);

    UT_ASSERT(uuid_is_equal(&ref_uuid, &cluster_uuid));
    UT_ASSERT(cmd_code == EXA_ADM_CLDELETE);

     //ut_printf("I get %\n", error_msg);
}

ut_test(extra_parameters_in_xml_chuck_are_detected)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                            "<param name=\"unintended\" value=\"I got you !\"/>"
				    XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
				    UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

UT_SECTION(generated_parser_detects_error_in_data_content)

ut_test(invalid_boolean_parameter)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                            "<param name=\"optional\" value=\"1268\"/>"
				    XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

ut_test(invalid_uint32_t_parameter)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
	                             "<param name=\"param1\" value=\"1686junk\"/>"
				     XML_COMMAND_PARAM2
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EINVAL);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
	                             "<param name=\"param1\" value=\"bad value\"/>"
				     XML_COMMAND_PARAM2
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EINVAL);
}

ut_test(uint32_t_value_out_of_range)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
	                             "<param name=\"param1\" value=\"%" PRIu64"\"/>"
				     XML_COMMAND_PARAM2
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command", (uint64_t)(UINT32_MAX + 50ULL));

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

ut_test(int32_t_value_out_of_range)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
				     XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                             "<param name=\"param3\" value=\"%" PRId64"\"/>"
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command", (int64_t)(INT32_MAX + 50LL));

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

ut_test(string_is_too_big)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
				     XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                             "<param name=\"string\" value=\"%s\"/>"
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
	     UUID_VAL(&ref_uuid), "my_command", "This string is really too long");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

ut_test(invalid_xml_subtree)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
				     XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                             "<param name=\"doc\"/>"
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
				     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

ut_test(xml_subtree_with_no_single_root_element)
{
    uuid_zero(&cluster_uuid);

    os_snprintf(buffer, sizeof(buffer), XML_COMMAND_HEADER
				     XML_COMMAND_PARAM1 XML_COMMAND_PARAM2
	                             "<param name=\"doc\">"
				     "<nice_tag> <tag1/> bla bla <tag2></tag2> </nice_tag>"
				     "<nice_tag2> <tag1/> bla bla <tag2></tag2> </nice_tag2>"
				     "</param>"
				     XML_COMMAND_TRAILER, XML_PROTOCOL_VERSION,
				     UUID_VAL(&ref_uuid), "my_command");

    xml_command_parse(buffer, &cmd_code, &cluster_uuid, &data, &data_size, &err_desc);

//    ut_printf("Result is: %s (%d)", err_desc.msg, err_desc.code);
    UT_ASSERT(err_desc.code == -EXA_ERR_INVALID_PARAM);
}

