/*
 * report.c
 *
 * Copyright (C) 2020 Red Hat, Inc., Kairui Song <kasong@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "memstrack.h"
#include "tracing.h"
#include "report.h"
#include "proc.h"

int m_throttle = 100;

static void report_module_summary(void) {
	struct Module **modules;
	modules = collect_modules_sorted(0);

	for (int i = 0; i < module_map.size; ++i) {
		log_info(
				"Module %s using %.1lfMB (%ld pages), peak allocation %.1lfMB (%ld pages)\n",
				modules[i]->name,
				modules[i]->tracenode.record->pages_alloc * ((double)page_size) / 1024 / 1024,
				modules[i]->tracenode.record->pages_alloc,
				modules[i]->tracenode.record->pages_alloc_peak * ((double)page_size) / 1024 / 1024,
				modules[i]->tracenode.record->pages_alloc_peak
			);
	}

	free(modules);
}

static void report_module_top(void) {
	struct Module **modules;
	modules = collect_modules_sorted(0);

	for (int i = 0; i < module_map.size; ++i) {
		log_info("Top stack usage of module %s:\n", modules[i]->name);
		print_tracenode(&modules[i]->tracenode, 2, 1, 0);
	}

	free(modules);
}

static void report_task_summary (void) {
	int task_num;
	long nr_pages_limit;
	struct Task **tasks;

	nr_pages_limit = page_alloc_counter - page_free_counter;
	nr_pages_limit = (nr_pages_limit * m_throttle + 99) / 100;

	tasks = collect_tasks_sorted(0, &task_num);
	for (int i = 0; i < task_num && nr_pages_limit > 0; ++i) {
		log_info(
				"Task %s (%ld) using %ld pages, peak usage %ld pages\n",
				tasks[i]->task_name, tasks[i]->pid,
				tasks[i]->tracenode.record->pages_alloc,
				tasks[i]->tracenode.record->pages_alloc_peak);
		nr_pages_limit -= tasks[i]->tracenode.record->pages_alloc;
	}

	free(tasks);
};

static void report_task_top (void) {
	int task_num;
	long nr_pages_limit;
	struct Task **tasks;

	nr_pages_limit = page_alloc_counter - page_free_counter;
	nr_pages_limit = (nr_pages_limit * m_throttle + 99) / 100;
	tasks = collect_tasks_sorted(0, &task_num);

	for (int i = 0; i < task_num && nr_pages_limit > 0; i++) {
		print_task(tasks[i]);
		nr_pages_limit -= tasks[i]->tracenode.record->pages_alloc;
	}

	free(tasks);
};

static void report_task_top_json(void) {
	int task_num;
	long nr_pages_limit;
	struct Task **tasks;

	nr_pages_limit = page_alloc_counter - page_free_counter;
	nr_pages_limit = (nr_pages_limit * m_throttle + 99) / 100;
	tasks = collect_tasks_sorted(0, &task_num);

	log_info("[\n");
	for (int i = 0; i < task_num && nr_pages_limit > 0; i++) {
		print_task_json(tasks[i]);
		nr_pages_limit -= tasks[i]->tracenode.record->pages_alloc_peak;

		if (i + 1 < task_num && nr_pages_limit)
			log_info(",\n");
		else
			log_info("\n");
	}
	log_info("]\n");
	free(tasks);
};

static void report_proc_slab_static(void) {
	print_slab_usage();
}

struct reporter_table_t  reporter_table[] = {
	{"module_summary", report_module_summary},
	{"module_top", report_module_top},
	{"task_summary", report_task_summary},
	{"task_top", report_task_top},
	{"task_top_json", report_task_top_json},
	{"proc_slab_static", report_proc_slab_static},
};

int report_table_size = sizeof(reporter_table) / sizeof(struct reporter_table_t);

void final_report(char *type, int task_limit) {
	load_kallsyms();

	char *report_type;
	report_type = strtok(type, ",");

	do {
		for (int i = 0; i < report_table_size; ++i) {
			if (!strcmp(reporter_table[i].name, report_type)) {
				log_info("\n======== Report format %s: ========\n", report_type);
				reporter_table[i].report();
				log_info("======== Report format %s END ========\n", report_type);
			}
		}
		report_type = strtok(NULL, ",");
	} while (report_type);
}
