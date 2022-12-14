/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu.
  *  If you have any problem,
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include <mpi.h>
#include "timer_mgt.h"
#include "global_data.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>


int elapsed_days_on_start_of_month_of_nonleap_year[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
int elapsed_days_on_start_of_month_of_leap_year[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
int num_days_of_month_of_nonleap_year[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int num_days_of_month_of_leap_year[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


bool common_is_timer_on(const char *frequency_unit, int frequency_count, int local_lag_count, int current_year,
                        int current_month, int current_day, int current_second, int current_num_elapsed_day,
                        int start_year, int start_month, int start_day, int start_second, int start_num_elapsed_day)
{
	long num_elapsed_time;


	if (frequency_count == 0) {
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, words_are_the_same(frequency_unit, "freq_pseudo"), "C-Coupler software error in common_is_timer_on");
		return true;
	}
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, frequency_count > 0, "C-Coupler software error: the frequency count must be larger than 0\n");

	if (IS_TIME_UNIT_SECOND(frequency_unit)) {
		num_elapsed_time = ((long)(current_num_elapsed_day - start_num_elapsed_day)) * SECONDS_PER_DAY + current_second - start_second;
	}
	else if (IS_TIME_UNIT_DAY(frequency_unit)) {
		if (current_second != 0)
			return false;
		num_elapsed_time = current_num_elapsed_day - start_num_elapsed_day;
	}
	else if (IS_TIME_UNIT_MONTH(frequency_unit)) {
		if (current_second != 0 || current_day != 1)
			return false;
		num_elapsed_time = (current_year - start_year) * NUM_MONTH_PER_YEAR + current_month - start_month;
	}
	else if (IS_TIME_UNIT_YEAR(frequency_unit)) {
		if (current_second != 0 || current_day != 1 || current_month != 1)
			return false;
		num_elapsed_time = current_year - start_year;
	}
	else EXECUTION_REPORT(REPORT_ERROR, -1, false, "C-Coupler software error: frequency unit %s is unsupported\n", frequency_unit);

	return num_elapsed_time >= local_lag_count && ((num_elapsed_time - local_lag_count) % frequency_count) == 0;
}


Periodical_timer::Periodical_timer(int comp_id, int timer_id, const char *freq_unit, int freq_count, int local_lag_count, int remote_lag_count, const char *annotation)
{
	strcpy(frequency_unit, freq_unit);
	this->frequency_count = freq_count;
	this->local_lag_count = local_lag_count;
	this->remote_lag_count = remote_lag_count;
	this->timer_id = timer_id;
	this->comp_id = comp_id;
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	EXECUTION_REPORT(REPORT_ERROR, -1, comp_time_mgr != NULL, "Software error in Periodical_timer::Periodical_timer, with annotation \"%s\"", annotation);
	comp_time_mgr->check_timer_format(frequency_unit, frequency_count, local_lag_count, remote_lag_count, true, annotation);
	if (IS_TIME_UNIT_MINUTE(frequency_unit)) {
		frequency_count *= 60;
		this->local_lag_count *= 60;
		this->remote_lag_count *= 60;
		strcpy(frequency_unit, FREQUENCY_UNIT_SECONDS);
	}
	if (IS_TIME_UNIT_HOUR(frequency_unit)) {
		frequency_count *= 3600;
		this->local_lag_count *= 3600;
		this->remote_lag_count *= 3600;
		strcpy(frequency_unit, FREQUENCY_UNIT_SECONDS);
	}
	if (IS_TIME_UNIT_STEP(freq_unit)) {
		EXECUTION_REPORT(REPORT_ERROR, -1, comp_time_mgr->get_time_step_in_second() > 0, "Software error in Periodical_timer::Periodical_timer: uninitialized time step");
		strcpy(frequency_unit, FREQUENCY_UNIT_SECONDS);
		frequency_count *= comp_time_mgr->get_time_step_in_second();
		this->local_lag_count *= comp_time_mgr->get_time_step_in_second();
		this->remote_lag_count *= comp_time_mgr->get_time_step_in_second();
	}
}


Periodical_timer::Periodical_timer(int comp_id, int timer_id, Periodical_timer *existing_timer)
{
	frequency_count = existing_timer->frequency_count;
	local_lag_count = existing_timer->local_lag_count;
	remote_lag_count = existing_timer->remote_lag_count;
	strcpy(frequency_unit, existing_timer->frequency_unit);
	this->timer_id = timer_id;
	this->comp_id = comp_id;
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
}


Periodical_timer::Periodical_timer(const char *array_buffer, long &buffer_content_iter, int comp_id, bool report_check, bool &successful)
{
	long str_size;


	load_string(frequency_unit, str_size, NAME_STR_SIZE, array_buffer, buffer_content_iter, NULL);
	successful = read_data_from_array_buffer(&frequency_count, sizeof(int), array_buffer, buffer_content_iter, report_check);
	successful = successful && read_data_from_array_buffer(&local_lag_count, sizeof(int), array_buffer, buffer_content_iter, report_check);
	successful = successful && read_data_from_array_buffer(&remote_lag_count, sizeof(int), array_buffer, buffer_content_iter, report_check);
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
}


Periodical_timer::~Periodical_timer()
{
}


void Periodical_timer::write_timer_into_array(char **array_buffer, long &buffer_max_size, long &buffer_content_size)
{
	write_data_into_array_buffer(&remote_lag_count, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&local_lag_count, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&frequency_count, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	dump_string(frequency_unit, -1, array_buffer, buffer_max_size, buffer_content_size);
}


bool Periodical_timer::is_timer_on(int current_year, int current_month, int current_day, int current_second, int current_num_elapsed_day,
                                   int start_year, int start_month, int start_day, int start_second, int start_num_elapsed_day)
{
	return common_is_timer_on(frequency_unit, frequency_count, local_lag_count, current_year,
	                          current_month, current_day, current_second, current_num_elapsed_day,
	                          start_year, start_month, start_day, start_second, start_num_elapsed_day);
}


void Periodical_timer::get_time_of_next_timer_on(Time_mgt *time_mgr, int current_year, int current_month, int current_day, int current_second, int current_num_elapsed_days, int time_step_in_second, int &next_timer_num_elapsed_days, int &next_timer_date, int &next_timer_second, bool advance)
{
	if (advance)
		time_mgr->advance_time(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_step_in_second);
	while (!is_timer_on(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_mgr->get_start_year(),
	                    time_mgr->get_start_month(), time_mgr->get_start_day(), time_mgr->get_start_second(), time_mgr->get_start_num_elapsed_day()))
		time_mgr->advance_time(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_step_in_second);

	next_timer_num_elapsed_days = current_num_elapsed_days;
	next_timer_date = current_year * 10000 + current_month * 100 + current_day;
	next_timer_second = current_second;
}


bool Periodical_timer::is_timer_on()
{
	return comp_time_mgr->is_timer_on(frequency_unit, frequency_count, local_lag_count);
}


bool Periodical_timer::is_the_same_with(Periodical_timer *another)
{
	if (this->frequency_count != another->frequency_count)
		return false;
	if (this->local_lag_count != another->local_lag_count)
		return false;
	if (this->remote_lag_count != another->remote_lag_count)
		return false;
	if (!words_are_the_same(this->frequency_unit, another->frequency_unit))
		return false;

	return true;
}


int Time_slots_points::compare_time_format_and_frequency_unit(long time_format_ID, const char *frequency_unit)
{
	int time_segment_values[13], index_lowest_unit_in_time_format = -1, index_frequency_unit;
	bool time_segment_masks[13];


	get_time_segment_values(time_format_ID, 0, time_segment_masks, time_segment_values, false);
	if (time_segment_masks[0])
		index_lowest_unit_in_time_format = 0;
	for (int i = 1; i < 7; i ++)
		if (index_lowest_unit_in_time_format == -1 && time_segment_masks[i])
			index_lowest_unit_in_time_format = i - 1;
	for (int i = 7; i < 13; i ++)
		if (index_lowest_unit_in_time_format == -1 && time_segment_masks[i])
			index_lowest_unit_in_time_format = i - 7;
	if (IS_TIME_UNIT_YEAR(frequency_unit))
		index_frequency_unit = 5;
	else if (IS_TIME_UNIT_MONTH(frequency_unit))
		index_frequency_unit = 4;
	else if (IS_TIME_UNIT_DAY(frequency_unit))
		index_frequency_unit = 3;
	else if (IS_TIME_UNIT_HOUR(frequency_unit))
		index_frequency_unit = 2;
	else if (IS_TIME_UNIT_MINUTE(frequency_unit))
		index_frequency_unit = 1;
	else if (IS_TIME_UNIT_SECOND(frequency_unit))
		index_frequency_unit = 0;
	else {
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, false, "Software error in Time_slots_points::compare_time_format_and_frequency_unit");
	}

	if (index_lowest_unit_in_time_format < index_frequency_unit)
		return -1;
	if (index_lowest_unit_in_time_format == index_frequency_unit)
		return 0;
	return 1;
}


Time_slots_points::Time_slots_points(std::vector<std::pair<long, long> > &slots_or_points, bool is_slot_nor_point, const char *time_format, const char *parent_frequency_unit, const char *annotation)
{
	int time_segment_values[13], index_lowest_unit_in_time_format = -1;
	std::pair<int*, int*> slot_or_point;


	this->time_format_ID = get_time_format_ID(time_format);
	this->time_format = strdup(time_format);
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, time_format_ID != -1, "Error happens when registering a set of time slots or points: \"%s\" is not a valid time format. Please %s.", time_format, annotation);
	this->is_slot_nor_point = is_slot_nor_point;
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, get_time_segment_values(time_format_ID, 0, time_segment_masks, time_segment_values, false), "Software error in Time_slots_points::Time_slots_points");

	if (parent_frequency_unit != NULL)
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, compare_time_format_and_frequency_unit(this->time_format_ID, parent_frequency_unit) < 0, "Error happens when registering a set of time slots or points: the time format \"%s\" does not match the parent frequency unit \"%s\". Please %s.", time_format, parent_frequency_unit, annotation);

	for (int i = 0; i < slots_or_points.size(); i ++) {
		slot_or_point.first = new int [13];
		slot_or_point.second = NULL;
		if (!get_time_segment_values(time_format_ID, slots_or_points[i].first, time_segment_masks, slot_or_point.first, true))
			EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, false, "Error happens when registering a set of time slots or points: the first time value %ld for a time slot or point does not match the given time format \"%s\". Please %s.", slots_or_points[i].first, time_format, annotation);
		if (is_slot_nor_point) {
			slot_or_point.second = new int [13];
			if (!get_time_segment_values(time_format_ID, slots_or_points[i].second, time_segment_masks, slot_or_point.second, true))
				EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, false, "Error happens when registering a set of time slots or points: the second time value %ld for a time slot does not match the given time format \"%s\". Please %s.", slots_or_points[i].second, time_format, annotation);
			EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, slots_or_points[i].first <= slots_or_points[i].second, "Error happens when registering a set of time slots or points: a time slot [%ld, %ld] does not match the rule that the end time value is not smaller than the start time value. Please %s.", slots_or_points[i].first, slots_or_points[i].second, annotation);
		}
		this->slots_or_points.push_back(slot_or_point);
	}
}


Time_slots_points::~Time_slots_points()
{
	delete [] time_format;
	for (int i = 0; i < slots_or_points.size(); i ++) {
		delete [] slots_or_points[i].first;
		if (is_slot_nor_point) {
			EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, slots_or_points[i].second != NULL, "Software error in Time_slots_points::~Time_slots_points");
			delete [] slots_or_points[i].second;
		}
	}
}


bool Time_slots_points::get_time_segment_values(long time_format_ID, long time_value, bool *time_segment_masks, int *time_segment_values, bool having_value)
{
	int seg_time_num_digitals;
	long time_value_max_bound = 1, last_time_value_max_bound;


	if ((time_format_ID & (0x00FF0000)) != 0 && time_value <= 0 && having_value)
		return false;
	else if (time_value < 0 && having_value)
		return false;

	for (int i = 0; i < 13; i ++) {
		time_segment_values[i] = 0;
		time_segment_masks[i] = false;
	}

	for (int i = 0; i < 7; i ++) {
		if ((time_format_ID & (((long)(0x0000000F)) << (i * 4))) == 0)
			continue;
		time_segment_masks[i] = true;
		last_time_value_max_bound = time_value_max_bound;
		if (i == 0)
			seg_time_num_digitals = 5;
		else if (i == 6)
			seg_time_num_digitals = 4;
		else seg_time_num_digitals = 2;
		for (int j = 0; j < seg_time_num_digitals; j ++)
			time_value_max_bound *= 10;
		if (having_value)
			time_segment_values[i] = (time_value % time_value_max_bound) / last_time_value_max_bound;
	}

	for (int i = 7; i < 13; i ++) {
		if ((time_format_ID & (((long)(0x0000000F)) << (i * 4))) == 0)
			continue;
		time_segment_masks[i] = true;
		if (having_value)
			time_segment_values[i] = (int) time_value;
	}

	if (having_value) {
		if ((time_format_ID & ((long)(0xFFFFFF0000000))) == 0)
			return check_calendar_time_segment_values(time_segment_masks, time_segment_values);
		if (time_value < 0)
			return false;
	}

	return true;
}


bool Time_slots_points::check_calendar_time_segment_values(const bool *time_segment_masks, const int *time_segment_values)
{
	if (time_segment_values[0] >= 86400)
		return false;
	if (time_segment_values[1] >= 60 || time_segment_values[2] >= 60 || time_segment_values[3] >= 24)
		return false;
	if (time_segment_masks[4] && (time_segment_values[4] == 0 || time_segment_values[4] > 31))
		return false;
	if (time_segment_masks[5] && (time_segment_values[5] == 0 || time_segment_values[5] > 12))
		return false;
	if (time_segment_values[4] > 0 && time_segment_values[5] > 0)
		if (num_days_of_month_of_leap_year[time_segment_values[5] - 1] < time_segment_values[4])
			return false;

	return true;
}


long Time_slots_points::get_time_format_ID(const char *time_format)
{
	long time_format_ID;

	if (words_are_the_same(time_format, "YYYY"))
		time_format_ID = TIME_FORMAT_YYYY;
	else if (words_are_the_same(time_format, "YYYYMM") || words_are_the_same(time_format, "YYYY-MM"))
		time_format_ID = TIME_FORMAT_YYYYMM;
	else if (words_are_the_same(time_format, "YYYYMMDD") || words_are_the_same(time_format, "YYYY-MM-DD"))
		time_format_ID = TIME_FORMAT_YYYYMMDD;
	else if (words_are_the_same(time_format, "YYYYMMDD.SSSSS") || words_are_the_same(time_format, "YYYY-MM-DD.SSSSS") ||
	         words_are_the_same(time_format, "YYYYMMDD-SSSSS") || words_are_the_same(time_format, "YYYY-MM-DD-SSSSS") ||
	         words_are_the_same(time_format, "YYYYMMDDSSSSS"))
		time_format_ID = TIME_FORMAT_YYYYMMDDSSSSS;
	else if (words_are_the_same(time_format, "YYYYMMDD.HHMMSS") || words_are_the_same(time_format, "YYYY-MM-DD.HH-MM-SS") ||
	         words_are_the_same(time_format, "YYYYMMDD.HH-MM-SS") || words_are_the_same(time_format, "YYYY-MM-DD.HHMMSS") ||
	         words_are_the_same(time_format, "YYYY-MM-DD-HH-MM-SS") || words_are_the_same(time_format, "YYYYMMDDHHMMSS"))
		time_format_ID = TIME_FORMAT_YYYYMMDDHHMMSS;
	else if (words_are_the_same(time_format, "SSSSS"))
		time_format_ID = TIME_FORMAT_SSSSS;
	else if (words_are_the_same(time_format, "HHMMSS") || words_are_the_same(time_format, "HH-MM-SS") || words_are_the_same(time_format, "HH:MM:SS"))
		time_format_ID = TIME_FORMAT_HHMMSS;
	else if (words_are_the_same(time_format, "MMo")) {
		EXECUTION_REPORT(REPORT_ERROR, -1, false, "time_format %s is not supported currently", time_format);
		time_format_ID = TIME_FORMAT_MMO;
	}
	else if (words_are_the_same(time_format, "MMDD.HHMMSS") || words_are_the_same(time_format, "MMDD-HHMMSS") || words_are_the_same(time_format, "MM-DD.HH-MM-SS") ||
	         words_are_the_same(time_format, "MM-DD-HH-MM-SS") || words_are_the_same(time_format, "MMDDHHMMSS"))
		time_format_ID = TIME_FORMAT_MMDDHHMMSS;
	else if (words_are_the_same(time_format, "MMDD.SSSSS") || words_are_the_same(time_format, "MMDD-SSSSS") || words_are_the_same(time_format, "MM-DD.SSSSS") ||
	         words_are_the_same(time_format, "MM-DD-SSSSS") || words_are_the_same(time_format, "MMDDSSSSS"))
		time_format_ID = TIME_FORMAT_MMDDSSSSS;
	else if (words_are_the_same(time_format, "DD"))
		time_format_ID = TIME_FORMAT_DD;
	else if (words_are_the_same(time_format, "DD.HHMMSS") || words_are_the_same(time_format, "DD-HHMMSS") || words_are_the_same(time_format, "DD.HH-MM-SS") ||
	         words_are_the_same(time_format, "DD-HH-MM-SS") || words_are_the_same(time_format, "DDHHMMSS"))
		time_format_ID = TIME_FORMAT_DDHHMMSS;
	else if (words_are_the_same(time_format, "DD.SSSSS") || words_are_the_same(time_format, "DD-SSSSS") || words_are_the_same(time_format, "DDSSSSS"))
		time_format_ID = TIME_FORMAT_DDSSSSS;
	else if (words_are_the_same(time_format, "YYYYMMDD.HHMM") || words_are_the_same(time_format, "YYYY-MM-DD.HH-MM") ||
	         words_are_the_same(time_format, "YYYYMMDD.HH-MM") || words_are_the_same(time_format, "YYYY-MM-DD.HHMM") ||
	         words_are_the_same(time_format, "YYYY-MM-DD-HH-MM") || words_are_the_same(time_format, "YYYYMMDDHHMM"))
		time_format_ID = TIME_FORMAT_YYYYMMDDHHMM;
	else if (words_are_the_same(time_format, "HHMM") || words_are_the_same(time_format, "HH-MM") || words_are_the_same(time_format, "HH:MM"))
		time_format_ID = TIME_FORMAT_HHMM;
	else if (words_are_the_same(time_format, "MMDD.HHMM") || words_are_the_same(time_format, "MMDD-HHMM") || words_are_the_same(time_format, "MM-DD.HH-MM") ||
	         words_are_the_same(time_format, "MM-DD-HH-MM") || words_are_the_same(time_format, "MMDDHHMM"))
		time_format_ID = TIME_FORMAT_MMDDHHMM;
	else if (words_are_the_same(time_format, "DD.HHMM") || words_are_the_same(time_format, "DD-HHMM") || words_are_the_same(time_format, "DD.HH-MM") ||
	         words_are_the_same(time_format, "DD-HH-MM") || words_are_the_same(time_format, "DDHHMM"))
		time_format_ID = TIME_FORMAT_DDHHMM;
	else if (words_are_the_same(time_format, "YYYYMMDD.HH") || words_are_the_same(time_format, "YYYY-MM-DD.HH") ||
	         words_are_the_same(time_format, "YYYYMMDD.HH") || words_are_the_same(time_format, "YYYY-MM-DD.HH") ||
	         words_are_the_same(time_format, "YYYY-MM-DD-HH") || words_are_the_same(time_format, "YYYYMMDDHH"))
		time_format_ID = TIME_FORMAT_YYYYMMDDHH;
	else if (words_are_the_same(time_format, "HH"))
		time_format_ID = TIME_FORMAT_HH;
	else if (words_are_the_same(time_format, "MMDD") || words_are_the_same(time_format, "MM-DD"))
		time_format_ID = TIME_FORMAT_MMDD;
	else if (words_are_the_same(time_format, "MMDD.HH") || words_are_the_same(time_format, "MMDD-HH") || words_are_the_same(time_format, "MM-DD.HH") ||
	         words_are_the_same(time_format, "MM-DD-HH") || words_are_the_same(time_format, "MMDDHH"))
		time_format_ID = TIME_FORMAT_MMDDHH;
	else if (words_are_the_same(time_format, "DD.HH") || words_are_the_same(time_format, "DD-HH") || words_are_the_same(time_format, "DDHH"))
		time_format_ID = TIME_FORMAT_DDHH;
	else if (IS_TIME_UNIT_SECOND(time_format))
		time_format_ID = TIME_FORMAT_SECONDS;
	else if (IS_TIME_UNIT_MINUTE(time_format))
		time_format_ID = TIME_FORMAT_NMINUTES;
	else if (IS_TIME_UNIT_HOUR(time_format))
		time_format_ID = TIME_FORMAT_NHOURS;
	else if (IS_TIME_UNIT_DAY(time_format))
		time_format_ID = TIME_FORMAT_NDAYS;
	else if (IS_TIME_UNIT_MONTH(time_format))
		time_format_ID = TIME_FORMAT_NMONTHS;
	else if (IS_TIME_UNIT_YEAR(time_format))
		time_format_ID = TIME_FORMAT_NYEARS;
	else time_format_ID = -1;

	return time_format_ID;
}


void Time_slots_points::get_time_segment_values_from_full_time(long full_time, int *time_segment_values)
{
	time_segment_values[0] = full_time % 100000;
	time_segment_values[1] = time_segment_values[0] % 60;
	time_segment_values[2] = (time_segment_values[0] % 3600) / 60;
	time_segment_values[3] = time_segment_values[0] / 3600;
	time_segment_values[4] = (full_time % 10000000) / 100000;
	time_segment_values[5] = (full_time % 1000000000) / 10000000;
	time_segment_values[6] = full_time / 1000000000;
}


bool Time_slots_points::is_slot_point_on(int comp_id, long slot_start_full_time, long current_full_time, int index)
{
	int slot_start_time_seg_values[13], current_time_seg_values[13];


	get_time_segment_values_from_full_time(current_full_time, current_time_seg_values);
	if ((time_format_ID & ((long)(0x00000FFFFFFF))) != 0) {
		if (is_slot_nor_point) {
			bool no_smaller_than_lower_bound = true, no_bigger_than_upper_bound = true;
			for (int i = 6; i >= 0; i --) {
				if (!time_segment_masks[i])
					continue;
				if (slots_or_points[index].first[i] < current_time_seg_values[i])
					break;
				if (slots_or_points[index].first[i] > current_time_seg_values[i]) {
					no_smaller_than_lower_bound = false;
					break;
				}
			}
			for (int i = 6; i >= 0; i --) {
				if (!time_segment_masks[i])
					continue;
				if (slots_or_points[index].second[i] > current_time_seg_values[i])
					break;
				if (slots_or_points[index].second[i] < current_time_seg_values[i]) {
					no_bigger_than_upper_bound = false;
					break;
				}
			}
			return (no_smaller_than_lower_bound && no_bigger_than_upper_bound);
		}
		else {
			for (int i = 6; i >= 0; i --) {
				if (!time_segment_masks[i])
					continue;
				if (slots_or_points[index].first[i] != current_time_seg_values[i])
					return false;
			}
			return true;
		}
	}
	else {
		get_time_segment_values_from_full_time(slot_start_full_time, slot_start_time_seg_values);
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, !time_segment_masks[12], "Software error in Time_slots_points::is_timer_on");
		int time_difference, key_segment_time_index;
		if (time_segment_masks[11]) {
			time_difference = (current_time_seg_values[6] - slot_start_time_seg_values[6]) * 12 + current_time_seg_values[5] - slot_start_time_seg_values[5];
			key_segment_time_index = 11;
		}
		else {
			long current_num_elapsed_day = components_time_mgrs->get_time_mgr(comp_id)->calculate_elapsed_day(current_time_seg_values[6], current_time_seg_values[5], current_time_seg_values[4]);
			long slot_start_num_elapsed_day = components_time_mgrs->get_time_mgr(comp_id)->calculate_elapsed_day(slot_start_time_seg_values[6], slot_start_time_seg_values[5], slot_start_time_seg_values[4]);
			if (time_segment_masks[10]) {
				time_difference = current_num_elapsed_day - slot_start_num_elapsed_day;
				key_segment_time_index = 10;
			}
			else if (time_segment_masks[9]) {
				time_difference = (current_num_elapsed_day - slot_start_num_elapsed_day) * 24 + current_time_seg_values[3] - slot_start_time_seg_values[3];
				key_segment_time_index = 9;
			}
			else if (time_segment_masks[8]) {
				time_difference = ((current_num_elapsed_day - slot_start_num_elapsed_day) * 24 + current_time_seg_values[3] - slot_start_time_seg_values[3]) * 60 + current_time_seg_values[2] - slot_start_time_seg_values[2];
				key_segment_time_index = 8;
			}
			else if (time_segment_masks[7]) {
				time_difference = (current_num_elapsed_day - slot_start_num_elapsed_day) * 24 * 3600 + current_time_seg_values[0] - slot_start_time_seg_values[0];
				key_segment_time_index = 7;
			}
			else {
				EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, false, "Software error in Time_slots_points::is_timer_on");
			}
			if (is_slot_nor_point) {
				return slots_or_points[index].second[key_segment_time_index] >= time_difference && slots_or_points[index].first[key_segment_time_index] <= time_difference;
			}
			else return slots_or_points[index].first[key_segment_time_index] == time_difference;
		}
	}
}


bool Time_slots_points::is_on(int comp_id, long slot_start_full_time, long current_full_time)
{
	for (int i = 0; i < slots_or_points.size(); i ++)
		if (is_slot_point_on(comp_id, slot_start_full_time, current_full_time, i))
			return true;

	return false;
}


Coupling_timer::Coupling_timer(int comp_id, int timer_id, const char *freq_unit, int freq_count, int local_lag_count, int remote_lag_count, const char *annotation)
{
	periodical_timer = new Periodical_timer(comp_id, timer_id, freq_unit, freq_count, local_lag_count, remote_lag_count, annotation);
	this->timer_id = timer_id;
	this->comp_id = comp_id;
	this->time_slots_points = NULL;
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	this->time_window_start_full_time = comp_time_mgr->get_current_full_time();
	EXECUTION_REPORT(REPORT_ERROR, -1, comp_time_mgr != NULL, "Software error in Coupling_timer::Coupling_timer, with annotation \"%s\"", annotation);
	annotation_mgr->add_annotation(timer_id, "define timer", annotation);
}


Coupling_timer::Coupling_timer(int comp_id, int timer_id, Coupling_timer *existing_timer)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, existing_timer->periodical_timer != NULL, "Software error in Coupling_timer::Coupling_timer");
	this->periodical_timer = new Periodical_timer(comp_id, timer_id, existing_timer->periodical_timer);
	this->timer_id = timer_id;
	this->comp_id = comp_id;
	this->time_slots_points = NULL;
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	this->time_window_start_full_time = existing_timer->time_window_start_full_time;
}


Coupling_timer::Coupling_timer(int comp_id, int timer_id, Time_slots_points *time_slots_points, Periodical_timer *periodical_timer, const char *annotation)
{
	this->periodical_timer = NULL;
	this->timer_id = timer_id;
	this->comp_id = comp_id;
	this->time_slots_points = time_slots_points;
	this->periodical_timer = periodical_timer;
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	this->time_window_start_full_time = comp_time_mgr->get_current_full_time();
	if (time_slots_points != NULL && !time_slots_points->get_is_slot_nor_point())
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, comp_id, periodical_timer == NULL, "Error happens when defining a coupling timer: a periodical timer cannot follow a time point timer. Please %s.", annotation);
	if (time_slots_points != NULL && periodical_timer != NULL)
		EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, comp_id, time_slots_points->compare_time_format_and_frequency_unit(time_slots_points->get_time_format_ID(), periodical_timer->get_frequency_unit()) >= 0, "Error happens when defining a coupling timer based on a periodical timer and a time slot timer: the frequency unit (\"%s\") of the periodical timer does not match the time format (\"%s\") of the time slot timer. Please %s.", periodical_timer->get_frequency_unit(), time_slots_points->get_time_format(), annotation);
}


Coupling_timer::Coupling_timer(const char *array_buffer, long &buffer_content_iter, int comp_id, bool report_check, bool &successful)
{
	periodical_timer = new Periodical_timer(array_buffer, buffer_content_iter, comp_id, report_check, successful);
	comp_time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	timer_mgr->add_timer(this);
	this->time_slots_points = NULL;
	this->time_window_start_full_time = comp_time_mgr->get_current_full_time();
}


Coupling_timer::~Coupling_timer()
{
	if (periodical_timer != NULL)
		delete periodical_timer;
	for (int i = 0; i < children_coupling_timers.size(); i ++)
		delete children_coupling_timers[i];
	if (time_slots_points != NULL)
		delete time_slots_points;
}


int Coupling_timer::get_frequency_count()
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::get_frequency_count");
	return periodical_timer->get_frequency_count();
}


int Coupling_timer::get_local_lag_count()
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::get_local_lag_count");
	return periodical_timer->get_local_lag_count();
}


int Coupling_timer::get_remote_lag_count()
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::get_remote_lag_count");
	return periodical_timer->get_remote_lag_count();
}


const char *Coupling_timer::get_frequency_unit()
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::get_frequency_unit");
	return periodical_timer->get_frequency_unit();
}


void Coupling_timer::reset_remote_lag_count()
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::reset_remote_lag_count");
	periodical_timer->reset_remote_lag_count();
}


void Coupling_timer::write_timer_into_array(char **array_buffer, long &buffer_max_size, long &buffer_content_size)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL, "Software error in Coupling_timer::write_timer_into_array");
	periodical_timer->write_timer_into_array(array_buffer, buffer_max_size, buffer_content_size);
}


bool Coupling_timer::is_timer_on(int current_year, int current_month, int current_day, int current_second, int current_num_elapsed_day,
                                 int start_year, int start_month, int start_day, int start_second, int start_num_elapsed_day)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL && time_slots_points == NULL && children_coupling_timers.size() == 0, "Software error in Coupling_timer::is_timer_on");
	if (periodical_timer != NULL)
		return periodical_timer->is_timer_on(current_year, current_month, current_day, current_second, current_num_elapsed_day,
		                                     start_year, start_month, start_day, start_second, start_num_elapsed_day);
}


void Coupling_timer::get_time_of_next_timer_on(Time_mgt *time_mgr, int current_year, int current_month, int current_day, int current_second, int current_num_elapsed_days, int time_step_in_second, int &next_timer_num_elapsed_days, int &next_timer_date, int &next_timer_second, bool advance)
{
	if (advance)
		time_mgr->advance_time(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_step_in_second);
	while (!is_timer_on(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_mgr->get_start_year(),
	                    time_mgr->get_start_month(), time_mgr->get_start_day(), time_mgr->get_start_second(), time_mgr->get_start_num_elapsed_day()))
		time_mgr->advance_time(current_year, current_month, current_day, current_second, current_num_elapsed_days, time_step_in_second);

	next_timer_num_elapsed_days = current_num_elapsed_days;
	next_timer_date = current_year * 10000 + current_month * 100 + current_day;
	next_timer_second = current_second;
}


bool Coupling_timer::is_empty_timer()
{
	return time_slots_points == NULL && periodical_timer == NULL && children_coupling_timers.size() == 0;
}


bool Coupling_timer::is_timer_on(long window_start_time)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, !is_empty_timer(), "Software error in Coupling_timer::is_timer_on");

	long current_time_window_start_full_time = window_start_time != -1 ? window_start_time : comp_time_mgr->get_start_full_time();
	if (time_slots_points != NULL && !time_slots_points->is_on(comp_id, current_time_window_start_full_time, comp_time_mgr->get_current_full_time())) {
		EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "The coupling timer is not on because the time_slots_points is not on: %ld vs %ld vs %ld", current_time_window_start_full_time, comp_time_mgr->get_current_full_time(), window_start_time);
		return false;
	}

	if (periodical_timer != NULL) {
		if (children_coupling_timers.size() == 0)
			return periodical_timer->is_timer_on();
		if (periodical_timer->is_timer_on()) {
			EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "change time_window_start_full_time from %ld to %ld", time_window_start_full_time, comp_time_mgr->get_current_full_time());
			time_window_start_full_time = comp_time_mgr->get_current_full_time();
		}
	}

	if (children_coupling_timers.size() > 0) {
		for (int i = 0; i < children_coupling_timers.size(); i ++)
			if (children_coupling_timers[i]->is_timer_on(time_window_start_full_time))
				return true;
		return false;
	}

	return true;
}


bool Coupling_timer::is_the_same_with(Coupling_timer *another)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, periodical_timer != NULL && another->periodical_timer != NULL, "Software error in Coupling_timer::is_the_same_with");
	return this->periodical_timer->is_the_same_with(another->periodical_timer);
}


void Coupling_timer::add_child_coupling_timer(Coupling_timer *child_coupling_timer, const char *annotation)
{
	EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, comp_id, !(time_slots_points != NULL && !time_slots_points->get_is_slot_nor_point()), "Error happens when add a child timer to a coupling timer: a coupling timer of time point cannnot have children timers. Please %s.", annotation);
	children_coupling_timers.push_back(child_coupling_timer);
}


Timer_mgt::~Timer_mgt()
{
	for (int i = 0; i < timers.size(); i ++) {
		delete timers[i];
	}
}


void Timer_mgt::add_timer(Coupling_timer *timer)
{
	timers.push_back(timer);
}


bool Timer_mgt::check_is_legal_timer_id(int timer_id)
{
	if ((timer_id & TYPE_ID_PREFIX_MASK) != TYPE_TIMER_ID_PREFIX)
		return false;

	return (timer_id & TYPE_ID_SUFFIX_MASK) < timers.size();
}


Coupling_timer *Timer_mgt::get_timer(int timer_id)
{
	if (!check_is_legal_timer_id(timer_id))
		return NULL;

	return timers[timer_id & TYPE_ID_SUFFIX_MASK];
}


int Timer_mgt::define_pseudo_timer(int comp_id)
{
	timers.push_back(new Coupling_timer(comp_id, TYPE_TIMER_ID_PREFIX | timers.size(), "freq_pseudo", 0, 0, 0, ""));
	return timers[timers.size() - 1]->get_timer_id();
}


int Timer_mgt::define_timer(int comp_id, Time_slots_points *time_slots_points, Periodical_timer *periodical_timer, const char *annotation)
{
	timers.push_back(new Coupling_timer(comp_id, TYPE_TIMER_ID_PREFIX | timers.size(), time_slots_points, periodical_timer, annotation));
	return timers[timers.size() - 1]->get_timer_id();
}


int Timer_mgt::define_timer(int comp_id, const char *freq_unit, int freq_count, int local_lag_count, int remote_lag_count, const char *annotation)
{
	timers.push_back(new Coupling_timer(comp_id, TYPE_TIMER_ID_PREFIX | timers.size(), freq_unit, freq_count, local_lag_count, remote_lag_count, annotation));
	return timers[timers.size() - 1]->get_timer_id();
}


int Timer_mgt::define_timer(int comp_id, Coupling_timer *existing_timer)
{
	Coupling_timer *new_timer = new Coupling_timer(comp_id, TYPE_TIMER_ID_PREFIX | timers.size(), existing_timer);
	timers.push_back(new_timer);
	return new_timer->get_timer_id();
}


bool Timer_mgt::is_timer_on(int timer_id, const char *annotation)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, check_is_legal_timer_id(timer_id), "The timer id is wrong when checking whether a timer is on. Please check the model code with the annotation \"%s\"", annotation);
	return timers[timer_id & TYPE_ID_SUFFIX_MASK]->is_timer_on();
}


bool Time_mgt::check_is_time_legal(int year, int month, int day, int second, const char *report_label)
{
	if (report_label != NULL) {
		EXECUTION_REPORT(REPORT_ERROR, -1, year >= 0, "The time format (%d-%d-%d-%d) is wrong: the year of simulation run can not be negative. Please check the model code with the annotation \"%s\"", year, month, day, second, report_label);
		EXECUTION_REPORT(REPORT_ERROR, -1, second >= 0 && second <= SECONDS_PER_DAY, "The time format (%d-%d-%d-%d) is wrong: the second of simulation run must between 0 and SECONDS_PER_DAY. Please check the model code with the annotation \"%s\"", year, month, day, second, report_label);
		EXECUTION_REPORT(REPORT_ERROR, -1, month >= 1 && month <= 12, "The time format (%d-%d-%d-%d) is wrong: the month must be between 1 and 12. Please check the model code with the annotation \"%s\"", year, month, day, second, report_label);
		if (leap_year_on && is_a_leap_year(year))
			EXECUTION_REPORT(REPORT_ERROR, -1, day >= 1 && day <= num_days_of_month_of_leap_year[month - 1], "The time format (%d-%d-%d-%d) is wrong: the day must be between 1 and %d. Please check the model code with the annotation \"%s\"", year, month, day, second, num_days_of_month_of_leap_year[month - 1], report_label);
		else EXECUTION_REPORT(REPORT_ERROR, -1, day >= 1 && day <= num_days_of_month_of_nonleap_year[month - 1], "The time format (%d-%d-%d-%d) is wrong: the day must be between 1 and %d. Please check the model code with the annotation \"%s\"", year, month, day, second, num_days_of_month_of_nonleap_year[month - 1], report_label);
		return true;
	}
	else {
		if (!(year >= 0) || !(second >= 0 && second <= SECONDS_PER_DAY) || !(month >= 1 && month <= 12))
			return false;
		if (leap_year_on && is_a_leap_year(year)) {
			if (!(day >= 1 && day <= num_days_of_month_of_leap_year[month - 1]))
				return false;
		}
		else {
			if (!(day >= 1 && day <= num_days_of_month_of_nonleap_year[month - 1]))
				return false;
		}
		return true;
	}
}


void Time_mgt::calculate_stop_time(int start_year, int start_month, int start_day, int start_second)
{
	long num_total_seconds;


	if (IS_TIME_UNIT_YEAR(stop_option)) {
		stop_year = start_year + stop_n;
		stop_month = start_month;
		stop_day = start_day;
		stop_second = start_second;
		if (start_month == 2 && start_day == 29 && !is_a_leap_year(stop_year))
			stop_day = 28;
	}
	else if (IS_TIME_UNIT_MONTH(stop_option)) {
		stop_year = start_year + stop_n / 12;
		if (start_month + (stop_n % 12) > 12) {
			stop_year ++;
			stop_month = (start_month + (stop_n % 12)) - 12;
		}
		else stop_month = (start_month + (stop_n % 12));
		stop_day = start_day;
		stop_second = start_second;
		if (is_a_leap_year(stop_year) && num_days_of_month_of_leap_year[stop_month - 1] < stop_day)
			stop_day = num_days_of_month_of_leap_year[stop_month - 1];
		if (!is_a_leap_year(stop_year) && num_days_of_month_of_nonleap_year[stop_month - 1] < stop_day)
			stop_day = num_days_of_month_of_nonleap_year[stop_month - 1];
	}
	else {
		int num_days = 0, num_hours = 0, num_minutes = 0, num_seconds = 0;
		if (IS_TIME_UNIT_DAY(stop_option)) {
			num_days = stop_n;
			num_total_seconds = stop_n * SECONDS_PER_DAY;
		}
		else if (IS_TIME_UNIT_HOUR(stop_option)) {
			num_days = stop_n / 24;
			num_hours = stop_n % 24;
			num_total_seconds = stop_n * 3600;
		}
		else if (IS_TIME_UNIT_MINUTE(stop_option)) {
			num_days = stop_n / 1440;
			num_hours = (stop_n % 1440) / 60;
			num_minutes = stop_n % 60;
			num_total_seconds = stop_n * 60;
		}
		else {
			num_days = stop_n / SECONDS_PER_DAY;
			num_hours = (stop_n % SECONDS_PER_DAY) / 3600;
			num_minutes = (stop_n % 3600) / 60;
			num_seconds = stop_n % 60;
			num_total_seconds = stop_n;
		}
		this->stop_year = -1;
		this->stop_month = -1;
		this->stop_day = -1;
		this->stop_second = -1;
		this->stop_num_elapsed_day = -1;
		Time_mgt *cloned_time_mgr = clone_time_mgr(NULL, comp_id);
		cloned_time_mgr->set_time_step_in_second(SECONDS_PER_DAY, "C-Coupler creates the time manager of a component", true);
		for (int i = 0; i < num_days; i ++)
			cloned_time_mgr->advance_model_time("in Time_mgt(...)", false);
		cloned_time_mgr->set_time_step_in_second(3600, "C-Coupler creates the time manager of a component", true);
		for (int i = 0; i < num_hours; i ++)
			cloned_time_mgr->advance_model_time("in Time_mgt(...)", false);
		cloned_time_mgr->set_time_step_in_second(60, "C-Coupler creates the time manager of a component", true);
		for (int i = 0; i < num_minutes; i ++)
			cloned_time_mgr->advance_model_time("in Time_mgt(...)", false);
		cloned_time_mgr->set_time_step_in_second(1, "C-Coupler creates the time manager of a component", true);
		for (int i = 0; i < num_seconds; i ++)
			cloned_time_mgr->advance_model_time("in Time_mgt(...)", false);
		this->stop_year = cloned_time_mgr->current_year;
		this->stop_month = cloned_time_mgr->current_month;
		this->stop_day = cloned_time_mgr->current_day;
		this->stop_second = cloned_time_mgr->current_second;
		delete cloned_time_mgr;
		EXECUTION_REPORT(REPORT_ERROR, -1, num_total_seconds == (calculate_elapsed_day(stop_year, stop_month, stop_day) - current_num_elapsed_day) * ((long)SECONDS_PER_DAY) + stop_second - start_second,
		                 "Software error in Time_mgt::Time_mgt: fail to caculate stop time according to stop_n");
	}
}


Time_mgt::Time_mgt(int comp_id, const char *XML_file_name, bool is_for_root_comp)
{
	int line_number;

	time_step_in_second = -1;
	case_desc[0] = '\0';

	EXECUTION_REPORT(REPORT_ERROR, -1, comp_comm_group_mgt_mgr->is_legal_local_comp_id(comp_id, false), "Software error in Time_mgt::Time_mgt: wrong component id");
	this->comp_id = comp_id;
	this->restart_timer = NULL;
	this->advance_time_synchronized = false;
	this->time_has_been_advanced = false;
	{
		int start_date, stop_date, reference_date, rest_freq_count, time_step;
		long num_total_seconds;
		TiXmlDocument XML_file(XML_file_name);
		EXECUTION_REPORT(REPORT_ERROR, -1, XML_file.LoadFile(MPI_COMM_WORLD), "Fail to read XML file \"%s\" with the time information setting. The XML file may not exist or may not be a legal XML file. Please check.", XML_file_name);
		TiXmlElement *XML_element = XML_file.FirstChildElement();
		const char *exp_model_name = get_XML_attribute(-1, CCPL_NAME_STR_LEN, XML_element, "model_name", XML_file_name, line_number, "the name of the model for the simulation", "the overall parameters to run the model", true);
		strcpy(this->exp_model_name, exp_model_name);
		const char *case_name = get_XML_attribute(-1, CCPL_NAME_STR_LEN, XML_element, "case_name", XML_file_name, line_number, "the name of the simulation", "the overall parameters to run the model", true);
		strcpy(this->case_name, case_name);
		const char *case_desc = XML_element->Attribute("case_description", &line_number);
		if (case_desc != NULL) {
			check_XML_attribute_value_string_length(-1, 1000, "case_description", case_desc, XML_file_name, line_number);
			strcpy(this->case_desc, case_desc);
		}
		EXECUTION_REPORT(REPORT_WARNING, -1, case_desc != NULL, "The description of the current simulation is unset or the format of the XML file is wrong. ");
		const char *run_type = get_XML_attribute(-1, -1, XML_element, "run_type", XML_file_name, line_number, "the type to run the model", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(run_type, RUNTYPE_INITIAL) || words_are_the_same(run_type, RUNTYPE_CONTINUE) || words_are_the_same(run_type, RUNTYPE_BRANCH) || words_are_the_same(run_type, RUNTYPE_HYBRID),
		                 "Run_type (%s) is wrong. It must be one of the four options: \"initial\", \"continue\", \"branch\" and \"hybrid\". Please check the XML file \"%s\" arround the line_number %d", run_type, XML_file_name, line_number);
		strcpy(this->run_type, run_type);
		if (words_are_the_same(run_type, RUNTYPE_INITIAL))
			runtype_mark = RUNTYPE_MARK_INITIAL;
		else if (words_are_the_same(run_type, RUNTYPE_CONTINUE))
			runtype_mark = RUNTYPE_MARK_CONTINUE;
		else if (words_are_the_same(run_type, RUNTYPE_BRANCH))
			runtype_mark = RUNTYPE_MARK_BRANCH;
		else runtype_mark = RUNTYPE_MARK_HYBRID;
		if (words_are_the_same(run_type, RUNTYPE_BRANCH) || words_are_the_same(run_type, RUNTYPE_HYBRID)) {
			const char *rest_refcase = get_XML_attribute(-1, CCPL_NAME_STR_LEN, XML_element, "rest_ref_case", XML_file_name, line_number, "the name of the reference case for branch run of hybrid run", "the overall parameters to run the model", true);
			strcpy(this->rest_refcase, rest_refcase);
			const char *refdate_string = get_XML_attribute(-1, -1, XML_element, "rest_ref_date", XML_file_name, line_number, "the date of the reference case for branch run of hybrid run", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(refdate_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, refdate_string, "rest_ref_date", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(refdate_string, "%d", &rest_refdate) == 1, "Software error in Time_mgt::Time_mgt");
			EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(rest_refdate / 10000, (rest_refdate % 10000) / 100, rest_refdate % 100, 0, NULL), "The date of the reference case for branch run of hybrid run is a wrong date. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
			const char *refsecond_string = get_XML_attribute(-1, -1, XML_element, "rest_ref_second", XML_file_name, line_number, "The second of the reference case for branch run of hybrid run", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(refsecond_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, refsecond_string, "rest_ref_second", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(refsecond_string, "%d", &rest_refsecond) == 1, "Software error in Time_mgt::Time_mgt");
			EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(rest_refdate / 10000, (rest_refdate % 10000) / 100, rest_refdate % 100, rest_refsecond, NULL), "The \"rest_ref_second\" (%d) specified is a wrong second number in a day. Please check the XML file \"%s\" arround the line_number %d", rest_refsecond, XML_file_name, line_number);
		}
		else {
			rest_refcase[0] = '\0';
			rest_refdate = -1;
			rest_refsecond = -1;
		}
		const char *leap_year_string = get_XML_attribute(-1, -1, XML_element, "leap_year", XML_file_name, line_number, "whether leap year is on in the simulation", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(leap_year_string, "on") || words_are_the_same(leap_year_string, "off"),
		                 "The value of leap year wrong. Its value must be \"on\" or \"off\". Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
		if (words_are_the_same(leap_year_string, "on"))
			leap_year_on = true;
		else leap_year_on = false;
		const char *start_date_string = get_XML_attribute(-1, -1, XML_element, "start_date", XML_file_name, line_number, "the start date to run the simulation", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(start_date_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, start_date_string, "start_date", line_number);
		EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(start_date_string, "%d", &start_date) == 1, "Software error in Time_mgt::Time_mgt");
		restart_second = -1;
		restart_num_elapsed_day = -1;
		restart_full_time = -1;
		common_restart_full_time = -1;
		start_year = start_date / 10000;
		start_month = (start_date % 10000) / 100;
		start_day = start_date % 100;
		EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(start_year, start_month, start_day, 0, NULL), "The start date specified is a wrong date. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
		const char *start_second_string = get_XML_attribute(-1, -1, XML_element, "start_second", XML_file_name, line_number, "the start second to run the simulation", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(start_second_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, start_second_string, "start_second", line_number);
		EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(start_second_string, "%d", &this->start_second) == 1, "Software error in Time_mgt::Time_mgt");
		EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(start_year, start_month, start_day, start_second, NULL), "The start second (%d) specified is a wrong second number in a day. Please check the XML file \"%s\" arround the line_number %d", start_second, XML_file_name, line_number);
		current_num_elapsed_day = calculate_elapsed_day(start_year, start_month, start_day);
		current_year = start_year;
		current_month = start_month;
		current_day = start_day;
		current_second = start_second;
		current_num_elapsed_day = calculate_elapsed_day(start_year, start_month, start_day);
		start_num_elapsed_day = current_num_elapsed_day;
		const char *reference_date_string = XML_element->Attribute("reference_date", &line_number);
		if (reference_date_string != NULL) {
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(reference_date_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, reference_date_string, "reference_date", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(reference_date_string, "%d", &reference_date) == 1, "Software error in Time_mgt::Time_mgt");
			reference_year = reference_date / 10000;
			reference_month = (reference_date % 10000) / 100;
			reference_day = reference_date % 100;
			EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(reference_year, reference_month, reference_day, 0, NULL), "The reference date specified is a wrong date. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
		}
		else {
			reference_year = 0;
			reference_month = 1;
			reference_day = 1;
		}
		const char *rest_freq_unit = get_XML_attribute(-1, -1, XML_element, "rest_freq_unit", XML_file_name, line_number, "the unit of the frequency of writing restart data files", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(rest_freq_unit, "none") || IS_TIME_UNIT_YEAR(rest_freq_unit) || IS_TIME_UNIT_SECOND(rest_freq_unit) || IS_TIME_UNIT_DAY(rest_freq_unit) || IS_TIME_UNIT_MONTH(rest_freq_unit),
		                 "The time unit for the frequency of writing restart files (rest_freq_unit) must be one of the following options: \"none\", %s, %s, %s, %s. Please check the XML file \"%s\" arround the line_number %d", TIME_UNIT_STRING_SECOND, TIME_UNIT_STRING_DAY, TIME_UNIT_STRING_MONTH, TIME_UNIT_STRING_YEAR, XML_file_name, line_number);
		strcpy(this->rest_freq_unit, rest_freq_unit);
		this->rest_freq_count = 0;
		if (!words_are_the_same(rest_freq_unit, "none")) {
			const char *rest_freq_count_string = get_XML_attribute(-1, -1, XML_element, "rest_freq_count", XML_file_name, line_number, "the count of the frequency of writing restart data files", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(rest_freq_count_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, rest_freq_count_string, "rest_freq_count", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(rest_freq_count_string, "%d", &rest_freq_count) == 1, "Software error in Time_mgt::Time_mgt");
			EXECUTION_REPORT(REPORT_ERROR, -1, rest_freq_count > 0, "The count of time unit for the frequency of writing restart files (rest_freq_count) must be a possitive value. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
			this->rest_freq_count = rest_freq_count;
		}
		const char *stop_option = get_XML_attribute(-1, -1, XML_element, "stop_option", XML_file_name, line_number, "the option to specify the end of the simulation", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(stop_option, "date") || IS_TIME_UNIT_SECOND(stop_option) || IS_TIME_UNIT_MINUTE(stop_option) || IS_TIME_UNIT_HOUR(stop_option) || IS_TIME_UNIT_DAY(stop_option) || IS_TIME_UNIT_MONTH(stop_option) || IS_TIME_UNIT_YEAR(stop_option),
		                 "The stop option is wrong. It must be one of the following options: \"date\", %s, %s, %s, %s, %s, %s. Please check the XML file \"%s\" arround the line_number %d", TIME_UNIT_STRING_SECOND, TIME_UNIT_STRING_MINUTE, TIME_UNIT_STRING_HOUR, TIME_UNIT_STRING_DAY, TIME_UNIT_STRING_MONTH, TIME_UNIT_STRING_YEAR, XML_file_name, line_number);
		strcpy(this->stop_option, stop_option);
		if (words_are_the_same(stop_option, "date")) {
			const char *stop_date_string = get_XML_attribute(-1, -1, XML_element, "stop_date", XML_file_name, line_number, "the date to stop the simulation", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(stop_date_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, stop_date_string, "stop_date", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(stop_date_string, "%d", &stop_date), "Software error in Time_mgt::Time_mgt");
			stop_year = stop_date / 10000;
			stop_month = (stop_date % 10000) / 100;
			stop_day = stop_date % 100;
			EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(stop_year, stop_month, stop_day, 0, NULL), "The stop date specified is a wrong date. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
			const char *stop_second_string = get_XML_attribute(-1, -1, XML_element, "stop_second", XML_file_name, line_number, "the second to stop the simulation", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(stop_second_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, stop_second_string, "stop_second", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(stop_second_string, "%d", &this->stop_second), "Software error in Time_mgt::Time_mgt");
			EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(stop_year, stop_month, stop_day, stop_second, NULL), "The stop second specified is a wrong second number in a day. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
			num_total_seconds = (calculate_elapsed_day(stop_year, stop_month, stop_day) - current_num_elapsed_day) * ((long)SECONDS_PER_DAY) + stop_second - start_second;
			EXECUTION_REPORT(REPORT_ERROR, -1, num_total_seconds > 0, "The stop time of simulation is wrong. It must be after the start time. Please check the XML file \"%s\" arround the line_number %d", XML_file_name, line_number);
		}
		else {
			const char *stop_n_string = get_XML_attribute(-1, -1, XML_element, "stop_n", XML_file_name, line_number, "the count for stopping the simulation", "the overall parameters to run the model", true);
			EXECUTION_REPORT(REPORT_ERROR, -1, is_string_decimal_number(stop_n_string), "Error happens when using the XML configuration file \"%s\": the value (\"%s\") of the attribute \"%s\" is not a decimal integer. Please verify the XML file around the line %d", XML_file_name, stop_n_string, "stop_n", line_number);
			EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(stop_n_string, "%d", &stop_n), "Software error in Time_mgt::Time_mgt");
			if (stop_n == -999)
				stop_year = stop_month = stop_day = stop_second = -1;
			else {
				calculate_stop_time(start_year, start_month, start_day, start_second);
				EXECUTION_REPORT(REPORT_ERROR, -1, check_is_time_legal(stop_year, stop_month, stop_day, stop_second, "Software error in Time_mgt::Time_mgt: wrong stop time generated by C-Coupler."));
			}
			EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, "The stop time generated by C-Coupler according to the users' specification is %04d%02d%02d-%05d", stop_year, stop_month, stop_day, stop_second);
		}
#ifdef USE_PARALLEL_IO
		int max_num_pio_proc;
		const char *pio_max_num_proc_str = get_XML_attribute(-1, CCPL_NAME_STR_LEN, XML_element, "max_num_pio_proc", XML_file_name, line_number, "maximum number of processes for handling parallel I/O", "the overall parameters to run the model", true);
		EXECUTION_REPORT(REPORT_ERROR, -1, sscanf(pio_max_num_proc_str, "%d", &max_num_pio_proc) == 1 && max_num_pio_proc > 0, "Error happens when configuring parallel I/O : the value (currently is \"%s\") of the XML attribute \"max_num_pio_proc\" is not a positive integer. Please verify the XML configuration file \"%s\".", pio_max_num_proc_str, XML_file_name);
		comp_comm_group_mgt_mgr->set_max_num_of_PIO_procs(max_num_pio_proc);
		EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, "max_num_pio_proc is %d", max_num_pio_proc);
#endif
	}

	num_total_steps = -1;
	initialize_to_start_time();

	if (is_for_root_comp && runtype_mark == RUNTYPE_MARK_CONTINUE) {
		if (comp_comm_group_mgt_mgr->get_current_proc_global_id() == 0)
			common_restart_full_time = determine_continue_run_restart_time();
		MPI_Bcast(&common_restart_full_time, 1, MPI_LONG, 0, MPI_COMM_WORLD);
		EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "The restart time determined by the rpointer files is %ld", common_restart_full_time);
		EXECUTION_REPORT(REPORT_ERROR, comp_id, common_restart_full_time != -1, "Error happens when starting the continue run: fail to find a common restart time according to the rpointer files among a component models. Please check the rpointer files.");
	}
}


void Time_mgt::initialize_to_start_time()
{
	previous_year = start_year;
	previous_month = start_month;
	previous_day = start_day;
	previous_second = start_second;
	current_year = start_year;
	current_month = start_month;
	current_day = start_day;
	current_second = start_second;
	current_num_elapsed_day = calculate_elapsed_day(start_year, start_month, start_day);
	start_num_elapsed_day = current_num_elapsed_day;
	if (stop_year != -1)
		stop_num_elapsed_day = calculate_elapsed_day(stop_year, stop_month, stop_day);
	else stop_num_elapsed_day = -1;
	current_step_id = 0;
	restarted_step_id = -1;
	restart_second = -1;
	restart_num_elapsed_day = -1;
	restart_full_time = -1;
	common_restart_full_time = -1;
	time_has_been_advanced = false;
}


void Time_mgt::build_restart_timer()
{
	if (!words_are_the_same(rest_freq_unit, "none"))
		restart_timer = timer_mgr->get_timer(timer_mgr->define_timer(comp_id, rest_freq_unit, rest_freq_count, 0, 0, "C-Coupler define restart timer"));
}


Time_mgt::~Time_mgt()
{
}


int Time_mgt::get_current_num_days_in_year()
{
	if (leap_year_on && is_a_leap_year(current_year))
		return elapsed_days_on_start_of_month_of_leap_year[current_month - 1] + current_day;
	return elapsed_days_on_start_of_month_of_nonleap_year[current_month - 1] + current_day;
}


long Time_mgt::calculate_elapsed_day(int year, int month, int day)
{
	int num_leap_year;


	check_is_time_legal(year, month, day, 0, "(at calculate_elapsed_day)");

	if (!leap_year_on)
		return year * NUM_DAYS_PER_NONLEAP_YEAR + elapsed_days_on_start_of_month_of_nonleap_year[month - 1] + day - 1;

	num_leap_year = (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400;

	if (year > 0)
		num_leap_year ++;   // year 0 is a leap year

	if (is_a_leap_year(year))
		return year * NUM_DAYS_PER_NONLEAP_YEAR + num_leap_year + elapsed_days_on_start_of_month_of_leap_year[month - 1] + day - 1;

	return year * NUM_DAYS_PER_NONLEAP_YEAR + num_leap_year + elapsed_days_on_start_of_month_of_nonleap_year[month - 1] + day - 1;
}


long Time_mgt::get_elapsed_day_from_full_time(long full_time)
{
	int year = full_time / 1000000000;
	int month = full_time % ((long)1000000000) / 10000000;
	int day = full_time % ((long)10000000) / 100000;

	return calculate_elapsed_day(year, month, day);
}


void Time_mgt::advance_time(int &current_year, int &current_month, int &current_day, int &current_second, int &current_num_elapsed_day, int time_step_in_second)
{
	int i, num_days_in_current_month;


	if (&current_year == &(this->current_year))
		time_has_been_advanced = true;
	current_second += time_step_in_second;
	for (i = 0; i < current_second / SECONDS_PER_DAY; i ++) {
		current_num_elapsed_day ++;
		if (leap_year_on && is_a_leap_year(current_year))
			num_days_in_current_month = num_days_of_month_of_leap_year[current_month - 1];
		else num_days_in_current_month = num_days_of_month_of_nonleap_year[current_month - 1];
		current_day ++;
		if (current_day > num_days_in_current_month) {
			current_month ++;
			current_day = 1;
		}
		if (current_month > 12) {
			current_month = 1;
			current_year ++;
		}
	}
	current_second = current_second % SECONDS_PER_DAY;
	//datamodel_mgr->handle_implicit_output("handle all implicit handlers for datamodel");
}


void Time_mgt::advance_model_time(const char *annotation, bool from_external_model)
{
	int i, num_days_in_current_month;


	EXECUTION_REPORT(REPORT_WARNING, comp_id, !is_time_out_of_execution(((long)current_num_elapsed_day) * 100000 + current_second), "Warning happens when advancing the model time at the model code with the annotation \"%s\": the current model time is out of the bounds of the integration period and the model coupling will not executed again. Please make sure that the component model and C-Coupler are consistent in time step, time advancing and integration period (e.g., start time and stop time).", annotation);

	EXECUTION_REPORT(REPORT_ERROR, comp_id, time_step_in_second != -1, "Cannot advance the time of the component \"%s\" at the model code with the annotation \"%s\", because the time step has not been specified.",
	                 comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "")->get_comp_name(), annotation);
	if (from_external_model && !advance_time_synchronized) {
		synchronize_comp_processes_for_API(comp_id, API_ID_TIME_MGT_ADVANCE_TIME, comp_comm_group_mgt_mgr->get_comm_group_of_local_comp(comp_id, "C-Coupler code in Time_mgt::advance_model_time"), "advance the time of a component", annotation);
		advance_time_synchronized = true;
		comp_comm_group_mgt_mgr->search_global_node(comp_id)->get_performance_timing_mgr()->performance_timing_output();
		comp_comm_group_mgt_mgr->search_global_node(comp_id)->get_performance_timing_mgr()->performance_timing_reset();
		if (get_runtype_mark() != RUNTYPE_MARK_INITIAL) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "")->get_restart_mgr()->check_restart_read_started(), "Error happens in a \"%s\" run where restart data should be read in: the API \"CCPL_start_!restart_read_IO\" has not been called before advancing the model time. Please verify.", run_type);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "")->get_restart_mgr()->get_are_all_restarted_fields_read(), "Error happens in a \"%s\" run where restart data should be read in: the API \"CCPL_restart_read_fields_all\" has not been called before advancing the model time. Please verify.", run_type);
		}
	}

	previous_year = current_year;
	previous_month = current_month;
	previous_day = current_day;
	previous_second = current_second;
	current_step_id ++;
	advance_time(current_year, current_month, current_day, current_second, current_num_elapsed_day, time_step_in_second);
}


double Time_mgt::get_double_current_calendar_time(int shift_second, const char *annotation)
{
	double calday;


	EXECUTION_REPORT(REPORT_ERROR, -1, shift_second >= 0, "Error happens when calling the API \"CCPL_get_current_calendar_time\": the parameter \"shift_second\" (currently is %d) cannot be a negative value. Please verify the model code with the annotation \"%s\".", shift_second, annotation);

	if (leap_year_on && is_a_leap_year(current_year)) {
		calday = elapsed_days_on_start_of_month_of_leap_year[current_month - 1] + current_day + ((double)(current_second + shift_second)) / SECONDS_PER_DAY;
		if (calday > (NUM_DAYS_PER_LEAP_YEAR + 1))
			calday = calday - (NUM_DAYS_PER_LEAP_YEAR + 1);
	}
	else {
		calday = elapsed_days_on_start_of_month_of_nonleap_year[current_month - 1] + current_day + ((double)(current_second + shift_second)) / SECONDS_PER_DAY;
		if (calday > (NUM_DAYS_PER_NONLEAP_YEAR + 1))
			calday = calday - (NUM_DAYS_PER_NONLEAP_YEAR + 1);
	}

	return calday;
}


float Time_mgt::get_float_current_calendar_time(int shift_second, const char *annotation)
{
	return (float) get_double_current_calendar_time(shift_second, annotation);
}


bool Time_mgt::is_timer_on(const char *frequency_unit, int frequency_count, int local_lag_count)
{
	long num_elapsed_time;


	return common_is_timer_on(frequency_unit, frequency_count, local_lag_count, current_year,
	                          current_month, current_day, current_second, current_num_elapsed_day,
	                          start_year, start_month, start_day, start_second, start_num_elapsed_day);
}


bool Time_mgt::check_is_model_run_finished()
{
	EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "check_is_model_run_finished %d %ld", current_step_id, num_total_steps);
	if (num_total_steps == -1)
		return false;
	return current_step_id >= num_total_steps;
}


bool Time_mgt::check_is_coupled_run_restart_time()
{
	return restart_timer->is_timer_on();
}


long Time_mgt::get_start_full_time()
{
	return (long)start_second + (long)start_day * 100000 + (long)start_month * 10000000 + (long)start_year * 1000000000;
}



long Time_mgt::get_previous_full_time()
{
	return (long)previous_second + (long)previous_day * 100000 + (long)previous_month * 10000000 + (long)previous_year * 1000000000;
}


long Time_mgt::get_current_full_time()
{
	return (long)current_second + (long)current_day * 100000 + (long)current_month * 10000000 + (long)current_year * 1000000000;
}


int Time_mgt::get_current_date()
{
	return (int) (current_year * 10000 + current_month * 100 + current_day);
}


void Time_mgt::check_timer_format(const char *frequency_unit, int frequency_count, int local_lag_count, int remote_lag_count, bool check_value, const char *annotation)
{
	if (time_step_in_second > 0) {
		EXECUTION_REPORT(REPORT_ERROR, comp_id, words_are_the_same(frequency_unit, "freq_pseudo") || IS_TIME_UNIT_STEP(frequency_unit) || IS_TIME_UNIT_SECOND(frequency_unit) || IS_TIME_UNIT_DAY(frequency_unit) || IS_TIME_UNIT_MONTH(frequency_unit) || IS_TIME_UNIT_YEAR(frequency_unit) || IS_TIME_UNIT_HOUR(frequency_unit) || IS_TIME_UNIT_MINUTE(frequency_unit),
		                 "Error happens when calling the API \"CCPL_define_single_timer\": the period unit is \"%s\", not one of %s, %s, %s, %s, %s, %s, %s. Please check the model code with the annotation \"%s\"", frequency_unit, TIME_UNIT_STRING_STEP, TIME_UNIT_STRING_SECOND, TIME_UNIT_STRING_MINUTE, TIME_UNIT_STRING_HOUR, TIME_UNIT_STRING_DAY, TIME_UNIT_STRING_MONTH, TIME_UNIT_STRING_YEAR, annotation);
		if (!words_are_the_same(frequency_unit, "freq_pseudo"))
			EXECUTION_REPORT(REPORT_ERROR, comp_id, frequency_count > 0, "Error happers when calling the API \"CCPL_define_single_timer\": \"period_count\" must be a positive number. Please verify the model code with the annotation \"%s\"", annotation);
		if (IS_TIME_UNIT_SECOND(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, frequency_count % time_step_in_second == 0, "Error happens when defining a timer: the frequency count (%d) in timer is not a multiple of the time step (%d) of the component when the frequency unit is \"%s\". Please check the model code with the annotation \"%s\"", frequency_count, time_step_in_second, frequency_unit, annotation);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, local_lag_count % time_step_in_second == 0, "Error happens when defining a timer: the remote lag count (%d) in a timer is not a multiple of the time step (%d) of the component when the frequency unit is \"%s\". Please check the model code with the annotation \"%s\"", local_lag_count, time_step_in_second, frequency_unit, annotation);
		}
		if (IS_TIME_UNIT_MINUTE(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (frequency_count * 60) % time_step_in_second == 0, "Error happens when defining a timer: the frequency (%d minutes) in timer is not a multiple of the time step (%d seconds) of the component. Please check the model code with the annotation \"%s\"", frequency_count, time_step_in_second, annotation);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (local_lag_count * 60) % time_step_in_second == 0, "Error happens when defining a timer: the remote lag (%d minutes) in a timer is not a multiple of the time step (%d seconds) of the component. Please check the model code with the annotation \"%s\"", local_lag_count, time_step_in_second, annotation);
		}
		if (IS_TIME_UNIT_HOUR(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (frequency_count * 3600) % time_step_in_second == 0, "Error happens when defining a timer: the frequency (%d minutes) in timer is not a multiple of the time step (%d seconds) of the component. Please check the model code with the annotation \"%s\"", frequency_count, time_step_in_second, annotation);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (local_lag_count * 3600) % time_step_in_second == 0, "Error happens when defining a timer: the remote lag (%d minutes) in a timer is not a multiple of the time step (%d seconds) of the component. Please check the model code with the annotation \"%s\"", local_lag_count, time_step_in_second, annotation);
		}
		if (local_lag_count != 0)
			EXECUTION_REPORT(REPORT_ERROR, comp_id, !IS_TIME_UNIT_MONTH(frequency_unit) && !IS_TIME_UNIT_YEAR(frequency_unit), "Error happens when defining a timer: the local lag count cannot be set when the frequency unit is \"%s\". Please check the model code with the annotation \"%s\"", frequency_unit, annotation);
		if (remote_lag_count != 0)
			EXECUTION_REPORT(REPORT_ERROR, comp_id, !IS_TIME_UNIT_MONTH(frequency_unit) && !IS_TIME_UNIT_YEAR(frequency_unit), "Error happens when defining a timer: the remote lag count cannot be set when the frequency unit is \"%s\". Please check the model code with the annotation \"%s\"", frequency_unit, annotation);
	}
}


void Time_mgt::check_timer_format_XML(const char *frequency_unit, int frequency_count, int local_lag_count, int remote_lag_count, bool check_value, const char *inst_name, const char *XML_file_name, int line_number)
{
	if (time_step_in_second > 0) {
		EXECUTION_REPORT(REPORT_ERROR, comp_id, words_are_the_same(frequency_unit, "freq_pseudo") || IS_TIME_UNIT_STEP(frequency_unit) || IS_TIME_UNIT_SECOND(frequency_unit) || IS_TIME_UNIT_DAY(frequency_unit) || IS_TIME_UNIT_MONTH(frequency_unit) || IS_TIME_UNIT_YEAR(frequency_unit) || IS_TIME_UNIT_HOUR(frequency_unit) || IS_TIME_UNIT_MINUTE(frequency_unit),
		                 "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the period unit set in the XML configuration file is \"%s\", not one of %s, %s, %s, %s, %s, %s, %s. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, frequency_unit, TIME_UNIT_STRING_STEP, TIME_UNIT_STRING_SECOND, TIME_UNIT_STRING_MINUTE, TIME_UNIT_STRING_HOUR, TIME_UNIT_STRING_DAY, TIME_UNIT_STRING_MONTH, TIME_UNIT_STRING_YEAR, XML_file_name, line_number);
		if (!words_are_the_same(frequency_unit, "freq_pseudo"))
			EXECUTION_REPORT(REPORT_ERROR, comp_id, frequency_count > 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\":  \"period_count\" must be a positive number. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, XML_file_name, line_number);
		if (IS_TIME_UNIT_SECOND(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, frequency_count % time_step_in_second == 0, "Error happens when defining a timer: the frequency count (%d) in timer is not a multiple of the time step (%d) of the component when the frequency unit is \"%s\". Please verify the XML file (\"%s\") arround the line number %d.", frequency_count, time_step_in_second, frequency_unit, XML_file_name, line_number);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, local_lag_count % time_step_in_second == 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the remote lag count (%d) in a timer is not a multiple of the time step (%d) of the component when the frequency unit is \"%s\". Please verify the XML file (\"%s\") arround the line number %d.", inst_name, local_lag_count, time_step_in_second, frequency_unit, XML_file_name, line_number);
		}
		if (IS_TIME_UNIT_MINUTE(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (frequency_count * 60) % time_step_in_second == 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the frequency (%d minutes) in timer is not a multiple of the time step (%d seconds) of the component. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, frequency_count, time_step_in_second, XML_file_name, line_number);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (local_lag_count * 60) % time_step_in_second == 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the remote lag (%d minutes) in a timer is not a multiple of the time step (%d seconds) of the component. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, local_lag_count, time_step_in_second, XML_file_name, line_number);
		}
		if (IS_TIME_UNIT_HOUR(frequency_unit) && check_value) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (frequency_count * 3600) % time_step_in_second == 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the frequency (%d minutes) in timer is not a multiple of the time step (%d seconds) of the component. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, frequency_count, time_step_in_second, XML_file_name, line_number);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, (local_lag_count * 3600) % time_step_in_second == 0, "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the remote lag (%d minutes) in a timer is not a multiple of the time step (%d seconds) of the component. Please verify the XML file (\"%s\") arround the line number %d.", inst_name, local_lag_count, time_step_in_second, XML_file_name, line_number);
		}
		if (local_lag_count != 0)
			EXECUTION_REPORT(REPORT_ERROR, comp_id, !IS_TIME_UNIT_MONTH(frequency_unit) && !IS_TIME_UNIT_YEAR(frequency_unit), "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the local lag count cannot be set when the frequency unit is \"%s\". Please verify the XML file (\"%s\") arround the line number %d.", inst_name, frequency_unit, XML_file_name, line_number);
		if (remote_lag_count != 0)
			EXECUTION_REPORT(REPORT_ERROR, comp_id, !IS_TIME_UNIT_MONTH(frequency_unit) && !IS_TIME_UNIT_YEAR(frequency_unit), "Error happers when calling the API \"CCPL_ensemble_procedures_inst_init\" to initialize an instance \"%s\": the remote lag count cannot be set when the frequency unit is \"%s\". Please verify the XML file (\"%s\") arround the line number %d.", inst_name, frequency_unit, XML_file_name, line_number);
	}
}


bool Time_mgt::check_time_consistency_between_components(long full_time)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, false, "to be implemented: Time_mgt::check_time_consistency_between_components");

}


void Time_mgt::get_elapsed_days_from_start_date(int *num_days, int *num_seconds)
{
	long current_num_elapsed_days, start_num_elapsed_days;

	current_num_elapsed_days = calculate_elapsed_day(current_year, current_month, current_day);
	start_num_elapsed_days = calculate_elapsed_day(start_year, start_month, start_day);
	*num_days = current_num_elapsed_days - start_num_elapsed_days;
	*num_seconds = current_second;
}


void Time_mgt::get_elapsed_days_from_reference_date(int *num_days, int *num_seconds)
{
	long current_num_elapsed_days, reference_num_elapsed_days;

	current_num_elapsed_days = calculate_elapsed_day(current_year, current_month, current_day);
	reference_num_elapsed_days = calculate_elapsed_day(reference_year, reference_month, reference_day);
	*num_days = current_num_elapsed_days - reference_num_elapsed_days;
	*num_seconds = current_second;
}


void Time_mgt::get_current_time(int &year, int &month, int &day, int &second, int shift_second, const char *annotation)
{
	int num_days_in_current_month;


	EXECUTION_REPORT(REPORT_ERROR, -1, shift_second >= 0, "Error happens when calling the API \"CCPL_get_current_time\": the parameter \"shift_second\" (currently is %d) cannot be a negative value. Please verify the model code with the annotation \"%s\".", shift_second, annotation);

	year = current_year;
	month = current_month;
	day = current_day;
	second = current_second + shift_second;

	while (second >= SECONDS_PER_DAY) {
		second -= SECONDS_PER_DAY;
		if (leap_year_on && is_a_leap_year(year))
			num_days_in_current_month = num_days_of_month_of_leap_year[month - 1];
		else num_days_in_current_month = num_days_of_month_of_nonleap_year[month - 1];
		day ++;
		if (day > num_days_in_current_month) {
			month ++;
			day = 1;
		}
		if (month > 12) {
			month = 1;
			year ++;
		}
	}
}


int Time_mgt::get_current_num_time_step()
{
//    if (words_are_the_same(compset_communicators_info_mgr->get_running_case_mode(), "hybrid") && current_step_id < SECONDS_PER_DAY/time_step_in_second)
//        return current_step_id + SECONDS_PER_DAY/time_step_in_second;
	return current_step_id;
}


Time_mgt *Time_mgt::clone_time_mgr(Time_mgt *new_time_mgr, int comp_id)
{
	bool has_input_time_mgr = (new_time_mgr != NULL);


	if (!has_input_time_mgr)
		new_time_mgr = new Time_mgt();

	new_time_mgr->restart_second = this->restart_second;
	new_time_mgr->restart_num_elapsed_day = this->restart_num_elapsed_day;
	new_time_mgr->restart_full_time = this->restart_full_time;
	new_time_mgr->common_restart_full_time = this->common_restart_full_time;
	new_time_mgr->restarted_step_id = this->restarted_step_id;
	new_time_mgr->start_year = this->start_year;
	new_time_mgr->start_month = this->start_month;
	new_time_mgr->start_day = this->start_day;
	new_time_mgr->start_second = this->start_second;
	new_time_mgr->previous_year = this->previous_year;
	new_time_mgr->previous_month = this->previous_month;
	new_time_mgr->previous_day = this->previous_day;
	new_time_mgr->previous_second = this->previous_second;
	new_time_mgr->current_year = this->current_year;
	new_time_mgr->current_month = this->current_month;
	new_time_mgr->current_day = this->current_day;
	new_time_mgr->current_second = this->current_second;
	new_time_mgr->reference_year = this->reference_year;
	new_time_mgr->reference_month = this->reference_month;
	new_time_mgr->reference_day = this->reference_day;
	new_time_mgr->stop_year = this->stop_year;
	new_time_mgr->stop_month = this->stop_month;
	new_time_mgr->stop_day = this->stop_day;
	new_time_mgr->stop_second = this->stop_second;
	if (!has_input_time_mgr) {
		new_time_mgr->time_step_in_second = -1;
		new_time_mgr->current_step_id = 0;
		new_time_mgr->num_total_steps = 0;
		new_time_mgr->restart_timer = NULL;
		new_time_mgr->time_has_been_advanced = false;
		new_time_mgr->advance_time_synchronized = false;
	}
	else {
		new_time_mgr->time_step_in_second = this->time_step_in_second;
		new_time_mgr->current_step_id = this->current_step_id;
		new_time_mgr->num_total_steps = this->num_total_steps;
		new_time_mgr->restart_timer = this->restart_timer;
		new_time_mgr->time_has_been_advanced = this->time_has_been_advanced;
		new_time_mgr->advance_time_synchronized = this->advance_time_synchronized;
	}
	new_time_mgr->leap_year_on = this->leap_year_on;
	new_time_mgr->comp_id = comp_id;
	new_time_mgr->current_num_elapsed_day = this->current_num_elapsed_day;
	new_time_mgr->start_num_elapsed_day = this->start_num_elapsed_day;
	new_time_mgr->stop_num_elapsed_day = this->stop_num_elapsed_day;
	new_time_mgr->stop_n = this->stop_n;
	new_time_mgr->runtype_mark = this->runtype_mark;
	strcpy(new_time_mgr->case_name, this->case_name);
	strcpy(new_time_mgr->exp_model_name, this->exp_model_name);
	strcpy(new_time_mgr->case_desc, this->case_desc);
	strcpy(new_time_mgr->run_type, this->run_type);
	strcpy(new_time_mgr->rest_refcase, this->rest_refcase);
	strcpy(new_time_mgr->stop_option, this->stop_option);
	strcpy(new_time_mgr->rest_freq_unit, this->rest_freq_unit);
	new_time_mgr->rest_freq_count = this->rest_freq_count;
	new_time_mgr->rest_refdate = this->rest_refdate;
	new_time_mgr->rest_refsecond = this->rest_refsecond;

	return new_time_mgr;
}


bool Time_mgt::set_time_step_in_second(int time_step_in_second, const char *annotation, bool check_error)
{
	this->time_step_in_second = time_step_in_second;
	EXECUTION_REPORT(REPORT_ERROR, comp_id, time_step_in_second > 0, "The value of the time step is wrong when setting the time step of the component \"%s\". It must be a positive value. Please check the model code with the annotation \"%s\"",
	                 comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "get comp name in Time_mgt::set_time_step_in_second")->get_comp_name(), annotation);
	if (stop_year != -1) {
		long total_seconds = (stop_num_elapsed_day - current_num_elapsed_day) * ((long)SECONDS_PER_DAY) + stop_second - start_second;
		if (!check_error && total_seconds % ((long)time_step_in_second) != 0)
			return false;
		EXECUTION_REPORT(REPORT_ERROR, comp_id, total_seconds % ((long)time_step_in_second) == 0, "The time step set at model code with the annotation \"%s\" does not match the start time and the stop time of the simulation. Please check the model code and the XML file \"env_run.xml\"", annotation);
		num_total_steps = total_seconds / time_step_in_second;
	}
	else num_total_steps = -1;
	if (restart_timer != NULL) {
		long rest_freq;
		if (IS_TIME_UNIT_DAY(rest_freq_unit))
			rest_freq = SECONDS_PER_DAY * rest_freq_count;
		else if (IS_TIME_UNIT_MONTH(rest_freq_unit) || IS_TIME_UNIT_YEAR(rest_freq_unit))
			rest_freq = SECONDS_PER_DAY;
		else if (IS_TIME_UNIT_SECOND(rest_freq_unit))
			rest_freq = rest_freq_count;
		EXECUTION_REPORT(REPORT_ERROR, comp_id, rest_freq % ((long)time_step_in_second) == 0, "The time step set at model code with the annotation \"%s\" does not match the frequency of writing restart data files. Please check the model code and the XML file \"env_run.xml\"", annotation);
	}

	return true;
}


bool Time_mgt::is_a_leap_year(int year)
{
	return ((year % 4) == 0 && (year % 100) != 0) || (year % 400) == 0;
}


void Time_mgt::check_consistency_of_current_time(int date, int second, const char *annotation)
{
	EXECUTION_REPORT(REPORT_ERROR, comp_id, date == get_current_date() && second == get_current_second(), "the model time is different from the time managed by C-Coupler. Please verify the model code according to the annotation \"%s\"", annotation);
}


bool Time_mgt::is_time_out_of_execution(long another_time)
{
	if (stop_num_elapsed_day == -1)
		return another_time < ((long)start_num_elapsed_day) * 100000 + start_second;

	return another_time < ((long)start_num_elapsed_day) * 100000 + start_second || another_time >= ((long)stop_num_elapsed_day) * 100000 + stop_second;
}


void Time_mgt::write_time_mgt_into_array(char **array_buffer, long &buffer_max_size, long &buffer_content_size)
{
	int temp_int;


	write_data_into_array_buffer(&start_year, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&start_month, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&start_day, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&start_second, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&previous_year, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&previous_month, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&previous_day, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&previous_second, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&current_year, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&current_month, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&current_day, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&current_second, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&time_step_in_second, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	temp_int = 1;
	write_data_into_array_buffer(&temp_int, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&current_step_id, sizeof(int), array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&leap_year_on, sizeof(bool), array_buffer, buffer_max_size, buffer_content_size);
	dump_string(comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "")->get_full_name(), -1, array_buffer, buffer_max_size, buffer_content_size);
	dump_string(case_name, -1, array_buffer, buffer_max_size, buffer_content_size);
}


void Time_mgt::import_restart_data(const char *temp_array_buffer, long &buffer_content_iter, const char *file_name, bool check_existing_data)
{
	int restart_start_year, restart_start_month, restart_start_day, restart_start_second, restart_previous_year, restart_previous_month, restart_previous_day, restart_previous_second;
	int restart_current_year, restart_current_month, restart_current_day, restart_current_second, restart_time_step_in_second, restart_current_step_id;
	int temp_int;
	bool restart_leap_year_on;
	char restart_comp_full_name[NAME_STR_SIZE], restart_case_name[NAME_STR_SIZE];
	long str_size;


	load_string(restart_case_name, str_size, NAME_STR_SIZE, temp_array_buffer, buffer_content_iter, file_name);
	load_string(restart_comp_full_name, str_size, NAME_STR_SIZE, temp_array_buffer, buffer_content_iter, file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_leap_year_on, sizeof(bool), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_current_step_id, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&temp_int, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_time_step_in_second, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_current_second, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_current_day, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_current_month, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_current_year, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_previous_second, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_previous_day, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_previous_month, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_previous_year, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_start_second, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_start_day, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_start_month, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&restart_start_year, sizeof(int), temp_array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong, or the information it includes is not complete. Please try a different restart time with complete restart data files.", file_name);

	if (check_existing_data) {
		if (words_are_the_same(RUNTYPE_CONTINUE, run_type))
			EXECUTION_REPORT(REPORT_ERROR, comp_id, words_are_the_same(restart_case_name, case_name), "Error happens when importing the restart data from the file \"%s\": the current case name (\"%s\") is different from the case name (\"%s\") imported from the restart data when it is a \"continue\" run. Please verify.", file_name, case_name, restart_case_name);
		if (words_are_the_same(RUNTYPE_CONTINUE, run_type) || words_are_the_same(RUNTYPE_BRANCH, run_type)) {
			char str1[NAME_STR_SIZE], str2[NAME_STR_SIZE];
			sprintf(str1, "%04d%02d%02d-%05d", start_year, start_month, start_day, start_second);
			sprintf(str2, "%04d%02d%02d-%05d", restart_start_year, restart_start_month, restart_start_day, restart_start_second);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, words_are_the_same(str1, str2), "Error happens when importing the restart data from the file \"%s\": the current start time (%s) of the simulation is different from the start time (%s) imported from the restart data when it is a \"continue\" or \"branch\" run. Please verify.", file_name, str1, str2);
			if (leap_year_on)
				strcpy(str1, "on");
			else strcpy(str1, "off");
			if (restart_leap_year_on)
				strcpy(str2, "on");
			else strcpy(str2, "off");
			EXECUTION_REPORT(REPORT_ERROR, comp_id, leap_year_on == restart_leap_year_on, "Error happens when importing the restart data from the file \"%s\": the current setting of leap year (\"%s\") is different from the setting (\"%s\") imported from the restart data when it is a \"continue\" or \"branch\" run. Please verify. ", file_name, str1, str2);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, time_step_in_second != -1, "Error happens when importing the restart data from the file \"%s\": the time step of the component model has not been set before reading the restart data file for a \"continue\" or \"branch\" run. Please verify. ", file_name, time_step_in_second, restart_time_step_in_second);
			EXECUTION_REPORT(REPORT_ERROR, comp_id, time_step_in_second == restart_time_step_in_second, "Error happens when importing the restart data from the file \"%s\": the current setting of time step (%d) is different from the setting (%d) imported from the restart data when it is a \"continue\" or \"branch\" run. Please verify. ", file_name, time_step_in_second, restart_time_step_in_second);
		}
	}

	current_second = restart_current_second;
	current_day = restart_current_day;
	current_month = restart_current_month;
	current_year = restart_current_year;
	previous_second = restart_previous_second;
	previous_day = restart_previous_day;
	previous_month = restart_previous_month;
	previous_year = restart_previous_year;
	if (time_step_in_second == -1) {
		time_step_in_second = restart_time_step_in_second;
		annotation_mgr->add_annotation(comp_id, "setting time step", "C-Coupler read from restart file");
	}
	current_num_elapsed_day = calculate_elapsed_day(current_year, current_month, current_day);
	current_step_id = ((current_num_elapsed_day - start_num_elapsed_day) * SECONDS_PER_DAY + current_second - start_second) / time_step_in_second;
	if (words_are_the_same(RUNTYPE_CONTINUE, run_type) || words_are_the_same(RUNTYPE_BRANCH, run_type)) {
		restart_second = current_second;
		restart_num_elapsed_day = current_num_elapsed_day;
		restart_full_time = restart_num_elapsed_day * ((long)100000) + restart_second;
		EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Set the restart time to %ld", restart_full_time);
		restarted_step_id = current_step_id;
	}

	if ((words_are_the_same(RUNTYPE_CONTINUE, run_type) || words_are_the_same(RUNTYPE_BRANCH, run_type)) && !words_are_the_same(stop_option, "date")) {
		calculate_stop_time(current_year, current_month, current_day, current_second);
		stop_num_elapsed_day = calculate_elapsed_day(stop_year, stop_month, stop_day);
		current_num_elapsed_day = calculate_elapsed_day(start_year, start_month, start_day);
		EXECUTION_REPORT(REPORT_ERROR, comp_id, set_time_step_in_second(time_step_in_second, "in Time_mgt::import_restart_data", false), "Error happens when importing the restart data from the file \"%s\": the time step does not match the setting of stop time", file_name);
		current_num_elapsed_day = calculate_elapsed_day(current_year, current_month, current_day);
	}
}


void Time_mgt::reset_current_time_to_start_time(const char *annotation)
{
	Inout_interface *executed_interface = inout_interface_mgr->search_an_inout_interface_executed_with_timer(comp_id);
	if (executed_interface != NULL)
		EXECUTION_REPORT(REPORT_ERROR, comp_id, false, "Error happens when calling the API CCPL_reset_current_time_to_start_time: the current time cannot be reset because a coupling interface \"%s\" of the current component model has been executed without its timer bypassed. Please check the model code with the annotation \"%s\"", executed_interface->get_interface_name(), annotation);
	initialize_to_start_time();
}


bool Time_mgt::is_restart_timer_on()
{
	if (restart_timer == NULL)
		return false;

	return restart_timer->is_timer_on();
}



long Time_mgt::determine_continue_run_restart_time()
{
	DIR *cur_dir = opendir(comp_comm_group_mgt_mgr->get_restart_common_dir());
	std::vector<std::pair<long, long> > comps_continue_run_candidate_restart_time;
	struct dirent *ent = NULL;
	struct stat st;
	long restart_time_in_rpointer, restart_time_in_prev_rpointer;
	char rpointer_file_name[NAME_STR_SIZE * 2], prev_rpointer_file_name[NAME_STR_SIZE * 2];
	int i;


	EXECUTION_REPORT(REPORT_ERROR, -1, cur_dir != NULL, "Comp_comm_group_mgt_mgr::is_comp_type_coupled");
	while ((ent = readdir(cur_dir)) != NULL) {
		stat(ent->d_name, &st);
		if (strlen(ent->d_name) > strlen("rpointer.") && strncmp(ent->d_name, "rpointer.", strlen("rpointer.")) == 0) {
			sprintf(rpointer_file_name, "%s/%s", comp_comm_group_mgt_mgr->get_restart_common_dir(), ent->d_name);
			restart_time_in_rpointer = get_restart_time_in_rpointer_file(rpointer_file_name);
			restart_time_in_prev_rpointer = -1;
			sprintf(prev_rpointer_file_name, "%s/prev.%s", comp_comm_group_mgt_mgr->get_restart_common_dir(), ent->d_name);
			if (does_file_exist(prev_rpointer_file_name))
				restart_time_in_prev_rpointer = get_restart_time_in_rpointer_file(prev_rpointer_file_name);
			comps_continue_run_candidate_restart_time.push_back(std::make_pair(restart_time_in_rpointer, restart_time_in_prev_rpointer));
		}
	}

	if (comps_continue_run_candidate_restart_time.size() == 0)
		return -1;

	for (i = 1; i < comps_continue_run_candidate_restart_time.size(); i ++)
		if (comps_continue_run_candidate_restart_time[i].first != comps_continue_run_candidate_restart_time[0].first && comps_continue_run_candidate_restart_time[i].second != comps_continue_run_candidate_restart_time[0].first)
			break;
	if (i == comps_continue_run_candidate_restart_time.size())
		return comps_continue_run_candidate_restart_time[0].first;

	for (i = 1; i < comps_continue_run_candidate_restart_time.size(); i ++)
		if (comps_continue_run_candidate_restart_time[i].first != comps_continue_run_candidate_restart_time[0].second && comps_continue_run_candidate_restart_time[i].second != comps_continue_run_candidate_restart_time[0].second)
			break;
	if (i == comps_continue_run_candidate_restart_time.size())
		return comps_continue_run_candidate_restart_time[0].second;

	return -1;
}


Components_time_mgt::~Components_time_mgt()
{
	for (int i = 0; i < components_time_mgrs.size(); i ++)
		delete components_time_mgrs[i];
}


Time_mgt *Components_time_mgt::get_time_mgr(int comp_id)
{
	if (!comp_comm_group_mgt_mgr->is_legal_local_comp_id(comp_id, false))
		return NULL;

	if (comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, "C-Coupler native code get time manager")->get_current_proc_local_id() == -1)
		return NULL;

	for (int i = 0; i < components_time_mgrs.size(); i++)
		if (components_time_mgrs[i]->get_comp_id() == comp_id)
			return components_time_mgrs[i];

	return NULL;
}


void Components_time_mgt::define_root_comp_time_mgr(int comp_id, const char *xml_file_name)
{
	components_time_mgrs.push_back(new Time_mgt(comp_id, xml_file_name, true));
	components_time_mgrs[components_time_mgrs.size() - 1]->build_restart_timer();
}



void Components_time_mgt::set_component_time_step(int comp_id, int time_step, const char *annotation)
{
	Time_mgt *time_mgr = get_time_mgr(comp_id);
	if (time_mgr->get_time_step_in_second() != -1 && time_mgr->get_time_step_in_second() != time_step)
		EXECUTION_REPORT(REPORT_ERROR, comp_id, false, "Error happens when clalling API \"CCPL_set_normal_time_step\": the time step of the component model \"%s\" has already been set before (the corresponding model code annotation is \"%s\"). It cannot be set again at the model code with the annotation \"%s\"",
		                 comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, false, annotation)->get_comp_name(), annotation_mgr->get_annotation(comp_id, "setting time step"), annotation);
	annotation_mgr->add_annotation(comp_id, "setting time step", annotation);
	if (comp_comm_group_mgt_mgr->get_current_proc_id_in_comp(comp_id, "get the local id of the current component in Components_time_mgt::set_component_time_step") == 0)
		EXECUTION_REPORT(REPORT_ERROR, comp_id, time_step > 0, "The value of time step is wrong. It must be a positive value. Please check the model code with the annotation \"%s\"", annotation);
	get_time_mgr(comp_id)->set_time_step_in_second(time_step, annotation, true);
}


void Components_time_mgt::clone_parent_comp_time_mgr(int comp_id, int parent_comp_id, const char *annotation)
{
	Time_mgt *parent_time_mgr = get_time_mgr(parent_comp_id);
	Time_mgt *new_time_mgr;


	EXECUTION_REPORT(REPORT_ERROR, -1, comp_comm_group_mgt_mgr->is_legal_local_comp_id(comp_id, false), "Software error in Components_time_mgt::clone_parent_comp_time_mgr: wrong comp_id");
	EXECUTION_REPORT(REPORT_ERROR, comp_id, parent_time_mgr != NULL, "Software error in Components_time_mgt::clone_parent_comp_time_mgr: parent time manager is NULL");
	new_time_mgr = parent_time_mgr->clone_time_mgr(NULL, comp_id);
	components_time_mgrs.push_back(new_time_mgr);
	new_time_mgr->build_restart_timer();
}


void Components_time_mgt::advance_component_time(int comp_id, const char *annotation)
{
	Time_mgt *time_mgr = get_time_mgr(comp_id);
	time_mgr->advance_model_time(annotation, true);
	comp_comm_group_mgt_mgr->set_current_proc_current_time(comp_id, time_mgr->get_current_num_elapsed_day(), time_mgr->get_current_second());
	EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "The current time is %08d-%05d, and the current number of the time step is %d", time_mgr->get_current_date(), time_mgr->get_current_second(), time_mgr->get_current_step_id());
	comp_comm_group_mgt_mgr->search_global_node(comp_id)->get_restart_mgr()->write_restart_mgt_into_file();
}


void Components_time_mgt::check_component_current_time(int comp_id, int date, int second, const char *annotation)
{
	Time_mgt *comp_time_mgr = get_time_mgr(comp_id);
	EXECUTION_REPORT(REPORT_ERROR, -1, comp_time_mgr != NULL, "The parameter of component id for checking the current time is wrong. Please check the model code with the annotation of \"%s\"", annotation);
	comp_time_mgr->check_consistency_of_current_time(date, second, annotation);
}


bool Components_time_mgt::is_model_run_ended(int comp_id, const char *annotation)
{
	Time_mgt *comp_time_mgr = get_time_mgr(comp_id);
	EXECUTION_REPORT(REPORT_ERROR, -1, comp_time_mgr != NULL, "The parameter of component id for checking the current time is wrong. Please check the model code with the annotation of \"%s\"", annotation);
	return comp_time_mgr->check_is_model_run_finished();
}

