/***************************************************************
  *  Copyright (c) 2019, Tsinghua University.
  *  This is a source file of PatCC.
  *  This file was initially finished by Dr. Li Liu and
  *  Haoyu Yang. If you have any problem,
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include "patcc_common_utils.h"
#include "coordinate_hash.h"
#include "global_data.h"
#include <cstdio>

Coord_Hash::Coord_Hash(int size)
    : capacity(size)
    , unique_values(0)
    , range_min(0.0)
    , range_len(0.0)
    , table(NULL)
	, latest_val(NULL_COORD_VALUE)
{
    table = new Entry_Node[size]();
    for (int i = 0; i < size; i++)
        table[i].next = &table[i];
};


void Coord_Hash::set_hashing_params(double min_v, double max_v)
{
    range_min = min_v;
	range_max = max_v;
    range_len = max_v - min_v;
}


void Coord_Hash::put(double val)
{
    if (val == latest_val)
        return;

    int key = hash(val);
    latest_val = val;
    latest_key = key;

    insert_to_list(key, val);
}


void Coord_Hash::insert_to_list(int key, double val)
{
    if (table[key].next == &table[key]) {
        table[key].next = NULL;
        table[key].value = val;
		unique_values ++;
    } else {
        Entry_Node *node, *last = NULL;

        for (node = &table[key]; node; node = node->next) {
			if (node->value == val)
				return;
            if (node->value > val)
                break;
            last = node;
        }

        if (last) {
            last->next = new Entry_Node(val, node);
        } else {
            Entry_Node* n = new Entry_Node(table[key].value, table[key].next);
            table[key].value = val;
            table[key].next = n;
        }
		unique_values ++;
    }
}


int Coord_Hash::get_index(double val)
{
    if (latest_val == val)
        return latest_idx;

    int key = hash(val);

    EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, table[key].next != &table[key]);

    Entry_Node *node;
    for (node = &table[key]; node; node = node->next)
        if (node->value == val)
            break;

    EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, node);

    latest_val = val;
    latest_idx = node->index;
    return latest_idx;
}


void Coord_Hash::make_sorted_index()
{
    int idx = 0;

    for (int i = 0; i < capacity; i++) {
        if (table[i].next == &table[i])
           continue;

        Entry_Node *node;
        for (node = &table[i]; node; node = node->next)
            node->index = idx++;
    }

    latest_val = range_min - range_len;
}


int Coord_Hash::hash(double val)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, val >= range_min && val <= range_max, "Software error in Coord_Hash::hash: %lf  %lf  %lf", val, range_min, range_max);
	int key = (val - range_min) * capacity / range_len;
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, key >= 0 && key <= capacity, "Software error in Coord_Hash::hash");
	if (key == capacity)
		key --;
    return key;
}
