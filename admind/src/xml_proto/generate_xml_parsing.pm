#!/usr/bin/perl

use File::Basename;

sub generate_param_struct
{
  foreach my $field_desc (@_) {
    if ($field_desc->{kind} eq 'array') {
      print "  $field_desc->{type} $field_desc->{field}\[$field_desc->{size}\];";
    } else {
      print "  $field_desc->{type} $field_desc->{field};";
    }
    if ($field_desc->{optional}) {
      print " /* declared as optional */";
    }
    print "\n";
  }
}

sub gen_common_get_field_string
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  print " /* get the node inside the xml tree */\n";
  print "  node = get_command_param(doc, \"$name\");\n";
  print "  str = node != NULL ? xml_get_prop(node, \"value\") : NULL;\n";
  if (!$field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Could not get $name\");\n";
    print "    return;\n";
    print "  }\n";
  }
}

sub gen_array_char
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};
  my $size = $field_desc->{size};

  gen_common_get_field_string($field_desc);

  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    str = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  if (strlcpy(params->$name, str, $size /*sizeof(params->$name)*/) >= $size)\n";
  print "  {\n";
  print "    set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"parameter for '$name' is too long\");\n";
  print "    return;\n";
  print "  }\n\n";
}

sub gen_xmlDocPtr
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  print "  node = get_command_param(doc, \"$name\");\n";

  # next line is ugly hack for gcc not to complain about str unused in this
  # function....
  print "  (void)str;\n";

  print "  if (node == NULL)\n";
  print "  {\n";
  if ($field_desc->{optional})
  {
    #careful, default value may not be NULL
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n"
  }
  else
  {
    print "    set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Could not get $name\");\n";
    print "    return;\n";
  }
  print "  }\n";

  print " else\n";
  print "   {\n";
  print "     if (node->children == NULL || node->children != node->last)\n";
  print "     {\n";
  print "       set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Subtree '$name' is malformed\");\n";
  print "       return;\n";
  print "     }\n";
  print "     params->$name = xmlNewDoc(BAD_CAST(\"1.0\"));\n";
  print "     xmlDocSetRootElement(params->$name, xmlCopyNodeList(node->children));\n";
  print "  }\n";
  print "\n";

}


sub gen_uint32_t
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  gen_common_get_field_string($field_desc);
  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  else\n";
  print "  {\n";
  print "    unsigned long long _temp;\n";
  print "    char *__ptr = NULL;\n";
  print "    errno = 0;\n";
  # Careful, here we cannot use strtoul because ul is architecture dependant
  # and does not afford that it fits on 32 bits
  print "    _temp = strtoull(str, &__ptr, 0);\n";
  print "    if (!__ptr || *__ptr)\n";
  print "      errno = EINVAL;\n";
  print "    if (errno != 0)\n";
  print "    {\n";
  print "      set_error(err_desc, -errno, \"Invalid value for $name '%s': %s\", str, strerror(errno));\n";
  print "      return;\n";
  print "    }\n";
  print "    if (_temp > UINT32_MAX)\n";
  print "    {\n";
  print "      set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Value '%s' for $name is out of range of uint32_t\", str);\n";
  print "      return;\n";
  print "    }\n";
  print "    params->$name = _temp;\n";
  print "  }\n";

}

sub gen_int32_t
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  gen_common_get_field_string($field_desc);
  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  else\n";
  print "  {\n";
  print "    long long _temp;\n";
  print "    errno = 0;\n";
  # Careful, here we cannot use strtoul because ul is architecture dependant
  # and does not afford that it fits on 32 bits
  print "    _temp = strtoll(str, NULL, 0);\n";
  print "    if (errno == ERANGE || errno == EINVAL)\n";
  print "    {\n";
  print "      set_error(err_desc, -errno, \"Invalid value for $name '%s': %s\", str, strerror(errno));\n";
  print "      return;\n";
  print "    }\n";
  print "    if (_temp > INT32_MAX || _temp < INT32_MIN)\n";
  print "    {\n";
  print "      set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Value '%s' for $name is out of range for int32_t\", str);\n";
  print "      return;\n";
  print "    }\n";
  print "    params->$name = ($field_desc->{type})_temp;\n";
  print "  }\n";

}

sub gen_uint64_t
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  gen_common_get_field_string($field_desc);
  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  else\n";
  print "  {\n";
  print "    errno = 0;\n";
  print "    params->$name = strtoull(str, NULL, 0);\n";
  print "    if (errno == ERANGE || errno == EINVAL)\n";
  print "    {\n";
  print "      set_error(err_desc, -errno, \"Invalid $name '%s': %s\", str, strerror(errno));\n";
  print "      return;\n";
  print "    }\n";
  print "  }\n";
}

sub gen_int64_t
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  gen_common_get_field_string($field_desc);
  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  else\n";
  print "  {\n";
  print "    errno = 0;\n";
  print "    params->$name = strtoll(str, NULL, 0);\n";
  print "    if (errno == ERANGE || errno == EINVAL)\n";
  print "    {\n";
  print "      set_error(err_desc, -errno, \"Invalid $name '%s': %s\", str, strerror(errno));\n";
  print "      return;\n";
  print "    }\n";
  print "  }\n";

}

sub gen_bool
{
  my ($field_desc) = @_;
  my $name = $field_desc->{field};

  gen_common_get_field_string($field_desc);
  if ($field_desc->{optional})
  {
    print "  if (str == NULL)\n";
    print "  {\n";
    print "    /* $name was defined optional, set the default value */\n";
    print "    params->$name = $field_desc->{default};\n";
    print "  }\n";
  }
  print "  else\n";
  print "  {\n";
  print "    if (strcmp(str, ADMIND_PROP_TRUE) != 0\n";
  print "        && strcmp(str, ADMIND_PROP_FALSE) != 0)\n";
  print "    {\n";
  print "        set_error(err_desc, -EXA_ERR_INVALID_PARAM, \"Invalid boolean value '%s' for $name\", str);\n";
  print "        return;\n";
  print "    }\n";
  print "    params->$name = (strcmp(str, ADMIND_PROP_TRUE) == 0);\n";
  print "  }\n\n";

}

my %known_array_types = ("char" => \&gen_array_char);

my %known_atomic_types = ("xmlDocPtr"  => \&gen_xmlDocPtr,
                          "uint32_t"   => \&gen_uint32_t,
			  "int32_t"    => \&gen_int32_t,
			  "uint64_t"   => \&gen_uint64_t,
			  "int64_t"    => \&gen_int64_t,
			  "bool"       => \&gen_bool);

sub generate_parsing_functions
{
  my ($param, $names) = @_;
  foreach my $export (@{$param}) {
    if (!defined($export->{struct}))
      {
	next;
      }

    print "struct $export->{struct} {\n";
    generate_param_struct(@{$export->{fields}});
    print "};\n";

    my $cmd_id = $names->{$export->{symbol}};

    print "\nstatic void\n";
    print "${cmd_id}_parse(struct _xmlDoc *doc, void *data, cl_error_desc_t *err_desc)\n";
    print "{\n";
    print "  struct $export->{struct} *params = data;\n";
    print "  const char *str;\n";
    print "  xmlNodePtr node;\n\n";
    print "  exalog_debug(\"parsing $cmd_id\");\n\n";

    foreach my $field_desc (@{$export->{fields}}) {
      #call generation function associated to type
      my $fun;
      if ($field_desc->{kind} eq 'array') {
	$fun = $known_array_types{$field_desc->{type}};
      } else {
	$fun = $known_atomic_types{$field_desc->{type}};
      }
      $fun->($field_desc);
      print " /* parsing was ok for the node, lets remove it from the tree */\n";
      print "  xmlUnlinkNode(node);\n";
      print "  xmlFreeNode(node);\n\n\n";
    }
    print "  set_success(err_desc);\n";
    print "}\n\n";
  }

#  known_atomic_types->{"xmlDocPtr"}(
}

sub check_parameters_type
{
  my ($param) = @_;
  foreach my $export (@{$param}) {
    foreach my $field_desc (@{$export->{fields}}) {
      if ($field_desc->{kind} eq 'array') {
	if (!grep /$field_desc->{type}/, keys %known_array_types) {
	  print "$export->{symbol}: Arrays of type '$field_desc->{type}' are not allowed\n";
	  exit -1;
	}
      } else {
	if (!grep /$field_desc->{type}/, keys %known_atomic_types) {
	  print "$export->{symbol}: Parameters of type '$field_desc->{type}' are not allowed\n";
	  exit -1;
	}
      }
    }
  }
}

sub generate_commands_array
{
  my ($param, $names) = @_;
  print "xml_parser_t xml_parser_list [] =\n{\n";
  foreach my $export (@{$param}) {
    my $cmd_name = $names->{$export->{symbol}};
    print "  {\n";
    print "    .cmd_code = $export->{symbol},\n";
    print "    .cmd_name = \"$cmd_name\",\n";
    if (!defined($export->{struct}))
    {
      print "    .parse    = NULL,\n";
      print "    .parsed_params_size = 0\n";
    }
    else
    {
      print "    .parse    = ${cmd_name}_parse,\n";
      print "    .parsed_params_size = sizeof(struct $export->{struct})\n";
    }
    print "  },\n";

  }
  print "  {\n";
  print "    .cmd_code = EXA_ADM_INVALID\n";
  print "  }\n";
  print "};\n";

}

# Entry point of this lib
sub generate_code
{
  my $name = basename($0);
  my ($export, $names) = @_;

  print "/*\n * Autogenerated code by '$name', do not edit\n */\n";
  print "#include \"os/include/os_inttypes.h\"\n";
  print "#include <string.h>\n";
  print "#include \"os/include/os_error.h\"\n";
  print "#include \"common/include/exa_constants.h\"\n";
  print "#include \"common/include/exa_conversion.h\"\n";
  print "#include \"admind/src/service_parameter.h\"\n";
  print "#include \"os/include/strlcpy.h\"\n";
  print "#include \"admind/src/adm_command.h\"\n";
  print "#include \"log/include/log.h\"\n";
  print "#include \"admind/src/xml_proto/xml_proto.h\"\n\n";

  print "/* helper function for parsing just a wrapper around xpath stuff */\n";
  print "static xmlNodePtr\n";
  print "get_command_param(xmlDoc *command_doc_ptr, const char *param_name)\n";
  print "{\n";
  print "    return xml_conf_xpath_singleton(command_doc_ptr,\n";
  print "                  XPATH_TO_GET_PARAM \"[\@name='%s']\",\n";
  print "                  param_name);\n";
  print "}\n";


  check_parameters_type($export);

  generate_parsing_functions($export, $names);

  generate_commands_array($export, $names);
}

#generate_code(\@exports);

my $EXPORTS;
my $NAMES;
if (open(STDIN, '-'))
{
    eval(join("\n", <STDIN>));
    close(STDIN);
}

generate_code($EXPORTS, $NAMES);

