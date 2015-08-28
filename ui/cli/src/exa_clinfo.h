/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLINFO_H__
#define __EXA_CLINFO_H__

#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/cli.h"

#include "common/include/exa_config.h"
#include "common/include/exa_assert.h"

#include "os/include/os_string.h"

class exa_clinfo : public exa_clcommand
{

public:

    static const std::string OPT_ARG_ONLY_ITEM;
    static const std::string OPT_ARG_WRAPPING_N;

    exa_clinfo(int argc, char *argv[]);
    ~exa_clinfo();

    void init_options();
    void init_see_alsos();

    void run();

    struct exa_cmp_xmlnode_lt
    {
        bool operator()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
        {
            const char *c1(xml_get_prop((xmlNodePtr)item1, "name"));
            const char *c2(xml_get_prop((xmlNodePtr)item2, "name"));

            EXA_ASSERT(c1);
            EXA_ASSERT(c2);

            return os_strverscmp(c1, c2) < 0;
        }
    };

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;
  void dump_output_section(std::ostream& out, bool show_hidden) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);


 private:

    bool volumes_info;
    bool groups_info;
    bool disks_info;
#ifdef WITH_FS
    bool filesystems_info;
    bool display_filesystem_size;
#endif
    bool softwares_info;
    uint wrapping_in_nodes;
    bool xml_dump;
    bool show_unassigned_rdev;
    bool force_kilo;
    bool display_group_config;
    bool iscsi_details;

    uint node_column_width;
    boost::shared_ptr<xmlDoc> config_ptr;
    std::string filter_only;

 protected:
    void exa_display_clinfo(boost::shared_ptr<xmlDoc>);

    void reset_all_info_flags();

    boost::shared_ptr<xmlDoc> get_config(exa_error_code &error_code, bool &in_recovery);
    void display_node_status(const std::set<std::string> &nodelist,
                             const std::string& color,
                             const std::string& status);
    std::string display_nodes(const std::set<std::string> &nodelist,
                              uint indent = 0);
    std::set<std::string> get_downnodes(boost::shared_ptr<xmlDoc> config_doc_ptr,
                                        std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &);
    std::set<std::string> nodelist_remove(std::set<std::string> &list1,
                                          std::set<std::string> &list2);
    std::set<std::string> nodelist_intersect(std::set<std::string> &list1,
                                             std::set<std::string> &list2);
    std::set<std::string> nodestring_split(const char *instr);

    void exa_display_softs(boost::shared_ptr<xmlDoc>,
                           std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &,
                           bool license_has_ha);
#ifdef WITH_MONITORING
    void exa_display_monitoring(boost::shared_ptr<xmlDoc>);
#endif
    void exa_display_rdevs(boost::shared_ptr<xmlDoc>);
    int  exa_display_modules(boost::shared_ptr<xmlDoc>,
                             std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &,
                             std::string &resultstring);
    int  exa_display_daemons(boost::shared_ptr<xmlDoc>,
                             std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &,
                             std::string &resultstring);
    void exa_display_token_manager(boost::shared_ptr<xmlDoc>,
                                   std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &);

    void exa_display_groups(boost::shared_ptr<xmlDoc>,
                            std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &);
    void exa_display_components_in_group(boost::shared_ptr<xmlDoc>,
                                         xmlNodePtr groupptr);
    void exa_display_group_configurations(std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &grplist);
    void exa_display_volume(boost::shared_ptr<xmlDoc> config_doc_ptr,
                            xmlNodePtr &, const std::string &group_status);
    void exa_display_volumes_status(boost::shared_ptr<xmlDoc> config_doc_ptr,
                                    std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &nodelist);

    void exa_display_export_status(xmlNodePtr volume_node, xmlNodePtr export_node,
				   const std::string& group_status);

    void exa_display_one_fs_status(boost::shared_ptr<xmlDoc> config_doc_ptr,
                                   xmlNodePtr fs, const std::string& name);
    void exa_display_gulm_nodes(boost::shared_ptr<xmlDoc>config_doc_ptr);
    void exa_display_fs_status(boost::shared_ptr<xmlDoc>,
                               std::set<xmlNodePtr, exa_cmp_xmlnode_lt> &);

    bool summSize(xmlNodeSetPtr, uint64_t *, uint64_t *);
    void display_total_used(uint64_t sizeT, uint64_t sizeU, uint disk_count,
	                    uint rdevpath_maxlen);
    void display_total(uint64_t sizeT, uint disk_count,
	               uint rdevpath_maxlen);
    void init_column_width(boost::shared_ptr<xmlDoc> config_doc_ptr);
    std::string exa_format_human_readable_string(const char* config_string, bool force_kilo);
};


#endif /* __EXA_CLINFO_H__ */
