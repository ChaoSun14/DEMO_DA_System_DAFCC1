/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#ifndef MEM_MGT
#define MEM_MGT

#include <vector>
#include "common_utils.h"
#include "remap_grid_data_class.h"
#include "timer_mgt.h"


#define BUF_MARK_GRID_FIELD                      (-100)
#define BUF_MARK_AVERAGED_INNER                  ((int)(0xF00F0000))
#define BUF_MARK_AVERAGED_INTER                  ((int)(0xF0070000))
#define BUF_MARK_AVERAGED_BACKUP                 ((int)(0xF0090000))
#define BUF_MARK_UNIT_TRANS                      ((int)(0xF0F00000))
#define BUF_MARK_DATATYPE_TRANS                  ((int)(0xF0700000))
#define BUF_MARK_DATA_TRANSFER                   ((int)(0xFF000000))
#define BUF_MARK_IO_FIELD_REG                    ((int)(0xF7000000))
#define BUF_MARK_IO_FIELD_MIRROR                 ((int)(0xF7100000))
#define BUF_MARK_GATHER                          ((int)(0xF1000000))
#define BUF_MARK_REMAP_NORMAL                    ((int)(0xF0000000))
#define BUF_MARK_REMAP_FRAC                      ((int)(0xF2000000))
#define BUF_MARK_REMAP_DATATYPE_TRANS_SRC        ((int)(0xF0110000))
#define BUF_MARK_REMAP_DATATYPE_TRANS_DST        ((int)(0xF0750000))
#define BUF_MARK_HALO_EXCHANGE                   ((int)(0xF0760000))
#define BUF_MARK_ENS_DATA_TRANSFER               ((int)(0xF0770000))


#define REG_FIELD_TAG_NONE                       ((int)0)
#define REG_FIELD_TAG_CPL                        ((int)1)
#define REG_FIELD_TAG_REST                       ((int)2)
#define REG_FIELD_TAG_IO                         ((int)4)


class Field_mem_info
{
    private:
        char field_name[NAME_STR_SIZE];
        char field_unit[NAME_STR_SIZE];
        int field_instance_id;
        int decomp_id;
        int grid_id;
        int comp_or_grid_id;
        int comp_id;
        int host_comp_id;
        int buf_mark;
        int usage_tag;
        bool is_registered_model_buf;
        long last_define_time;
        long define_order_count;
        bool is_field_active;
        Remap_grid_data_class *grided_field_data;
        Time_mgt *host_comp_time_mgr;
        long last_checksum;
		long chunk_field_instance_size;
		int num_chunks;
		void **chunks_buf;
		int *chunks_data_buf_size;
		int total_dim_size_before_H2D;
		int total_dim_size_after_H2D;

    public:
        Field_mem_info(const char *, int, int, int, const char *, const char *, const char *, bool, bool);
        bool match_field_instance(const char *, int, int, int);
        bool match_field_mem(void*);
        bool get_is_registered_model_buf() { return is_registered_model_buf; }
		void empty_grided_field_data() { grided_field_data = NULL; }
        bool check_is_field_active() { return is_field_active; }
        void *get_data_buf() { return grided_field_data->get_grid_data_field()->data_buf; }
        Remap_grid_data_class *get_field_data() { return grided_field_data; }
		void reset_field_data(Remap_grid_data_class *);
		void set_usage_tag(int usage_tag) { this->usage_tag = usage_tag; }
        void set_field_unit(char *new_field_unit) { sprintf(this->field_unit, "%s", new_field_unit); }
        void reset_mem_buf(void *buf, bool, int);
		void reset_mem_buf(Field_mem_info *, int);
        const char *get_decomp_name();
        const char *get_field_name() const { return field_name; }
        long get_size_of_field();
        void reset_field_name(const char*);
        void change_datatype_to_double();
        void calculate_field_conservative_sum(Field_mem_info*);
        void check_field_sum(bool, bool, const char *);
        void define_field_values(bool);
        void use_field_values(const char*);
        bool field_has_been_defined();
        long get_last_define_time() const { return last_define_time; }
        void set_define_order_count(long count) { define_order_count = count; }
        long get_define_order_count() const { return define_order_count; }
        int get_field_instance_id() const { return field_instance_id; }
        int get_comp_id() { return comp_id; }
        int get_host_comp_id() { return host_comp_id; }
        int get_grid_id() { return grid_id; }
        const char *get_grid_name();
        const char *get_unit() const { return field_unit; }
        int get_buf_mark() const { return buf_mark; }
        int get_comp_or_grid_id() const { return comp_or_grid_id; }
        int get_decomp_id() const { return decomp_id; }
        void set_field_instance_id(int, const char*);
        const char *get_data_type();
        bool is_checksum_changed();
        void reset_checksum();
        bool is_CPL_field_inst() { return (usage_tag & REG_FIELD_TAG_CPL) == REG_FIELD_TAG_CPL; }
        bool is_REST_field_inst() { return (usage_tag & REG_FIELD_TAG_REST) == REG_FIELD_TAG_REST; }
        bool is_IO_field_inst() { return (usage_tag & REG_FIELD_TAG_IO) == REG_FIELD_TAG_IO; }		
		void get_total_dim_size_before_and_after_H2D(int &, int &);
		void finish_chunk_registration(const char*);
		void register_a_chunk(void*, int, const char *, const char *);
		int get_num_chunks() { return num_chunks; }
		void *get_chunk_buf(int i) { return chunks_buf[i]; }
		int get_chunk_data_buf_size(int i) { return chunks_data_buf_size[i]; }
		void transformation_between_chunks_array(bool);
		long calculate_overall_checksum();
		void confirm_overall_data_buf_for_chunks();
		void change_to_registered_without_data_buffers();
		void set_data_buf_from_model(void*,int);
		void copy_in_another_field_attributes(Field_mem_info *);
        ~Field_mem_info();
};


class Memory_mgt
{
    private:
        std::vector<Field_mem_info *> fields_mem;
        
    public: 
        Memory_mgt() {}
        Field_mem_info *alloc_mem(Field_mem_info*, int, int, const char*, bool, bool);
        Field_mem_info *alloc_mem(const char*, int, int, int, const char*, const char*, const char*, bool, bool);
         int register_external_field_instance(const char *, void *, int, int, int, int, int, const char *, const char *, const char *);
        Field_mem_info *search_field_via_data_buf(const void*, bool);
        void check_sum_of_all_registered_fields(int, bool, const char*);
        int get_num_fields() { return fields_mem.size(); }
        ~Memory_mgt();
        long get_field_size(void*, const char*);
        Field_mem_info *search_field_instance(const char *, int, int, int);
        bool check_is_legal_field_instance_id(int);
        Field_mem_info *get_field_instance(int);
        void copy_field_data_values(Field_mem_info *, Field_mem_info*);
		void get_comp_existing_registered_field_insts(std::vector<Field_mem_info *>&, int);
		void delete_field_inst(Field_mem_info*);
};

#endif
