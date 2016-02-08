/*
 * drivers/cpufreq/cpufreq_governor.h
 *
 * Header file for CPUFreq governors common code
 *
 * Copyright	(C) 2001 Russell King
 *		(C) 2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *		(C) 2003 Jun Nakajima <jun.nakajima@intel.com>
 *		(C) 2009 Alexander Clouter <alex@digriz.org.uk>
 *		(c) 2012 Viresh Kumar <viresh.kumar@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _CPUFREQ_GOVERNOR_H
#define _CPUFREQ_GOVERNOR_H

#include <linux/atomic.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/mutex.h>

/* Ondemand Sampling types */
enum {OD_NORMAL_SAMPLE, OD_SUB_SAMPLE};

/*
 * Macro for creating governors sysfs routines
 *
 * - gov_sys: One governor instance per whole system
 * - gov_pol: One governor instance per policy
 */

/* Create attributes */
#define gov_sys_attr_ro(_name)						\
static struct kobj_attribute _name##_gov_sys =				\
__ATTR(_name, 0444, show_##_name##_gov_sys, NULL)

#define gov_sys_attr_rw(_name)						\
static struct kobj_attribute _name##_gov_sys =				\
__ATTR(_name, 0644, show_##_name##_gov_sys, store_##_name##_gov_sys)

#define gov_pol_attr_ro(_name)						\
static struct freq_attr _name##_gov_pol =				\
__ATTR(_name, 0444, show_##_name##_gov_pol, NULL)

#define gov_pol_attr_rw(_name)						\
static struct freq_attr _name##_gov_pol =				\
__ATTR(_name, 0644, show_##_name##_gov_pol, store_##_name##_gov_pol)

#define gov_sys_pol_attr_rw(_name)					\
	gov_sys_attr_rw(_name);						\
	gov_pol_attr_rw(_name)

#define gov_sys_pol_attr_ro(_name)					\
	gov_sys_attr_ro(_name);						\
	gov_pol_attr_ro(_name)

/* Create show/store routines */
#define show_one(_gov, file_name)					\
static ssize_t show_##file_name##_gov_sys				\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)		\
{									\
	struct _gov##_dbs_tuners *tuners = _gov##_dbs_gov.gdbs_data->tuners; \
	return sprintf(buf, "%u\n", tuners->file_name);			\
}									\
									\
static ssize_t show_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	struct policy_dbs_info *policy_dbs = policy->governor_data;	\
	struct dbs_data *dbs_data = policy_dbs->dbs_data;		\
	struct _gov##_dbs_tuners *tuners = dbs_data->tuners;		\
	return sprintf(buf, "%u\n", tuners->file_name);			\
}

#define store_one(_gov, file_name)					\
static ssize_t store_##file_name##_gov_sys				\
(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) \
{									\
	struct dbs_data *dbs_data = _gov##_dbs_gov.gdbs_data;		\
	return store_##file_name(dbs_data, buf, count);			\
}									\
									\
static ssize_t store_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, const char *buf, size_t count)		\
{									\
	struct policy_dbs_info *policy_dbs = policy->governor_data;	\
	return store_##file_name(policy_dbs->dbs_data, buf, count);			\
}

#define show_store_one(_gov, file_name)					\
show_one(_gov, file_name);						\
store_one(_gov, file_name)

/* create helper routines */
#define define_get_cpu_dbs_routines(_dbs_info)				\
static struct cpu_dbs_info *get_cpu_cdbs(int cpu)			\
{									\
	return &per_cpu(_dbs_info, cpu).cdbs;				\
}									\
									\
static void *get_cpu_dbs_info_s(int cpu)				\
{									\
	return &per_cpu(_dbs_info, cpu);				\
}

/*
 * Abbreviations:
 * dbs: used as a shortform for demand based switching It helps to keep variable
 *	names smaller, simpler
 * cdbs: common dbs
 * od_*: On-demand governor
 * cs_*: Conservative governor
 */

/* Governor demand based switching data (per-policy or global). */
struct dbs_data {
	unsigned int min_sampling_rate;
	int usage_count;
	void *tuners;
};

/* Common to all CPUs of a policy */
struct policy_dbs_info {
	struct cpufreq_policy *policy;
	/*
	 * Per policy mutex that serializes load evaluation from limit-change
	 * and work-handler.
	 */
	struct mutex timer_mutex;

	u64 last_sample_time;
	s64 sample_delay_ns;
	atomic_t work_count;
	struct irq_work irq_work;
	struct work_struct work;
	/* dbs_data may be shared between multiple policy objects */
	struct dbs_data *dbs_data;
};

static inline void gov_update_sample_delay(struct policy_dbs_info *policy_dbs,
					   unsigned int delay_us)
{
	policy_dbs->sample_delay_ns = delay_us * NSEC_PER_USEC;
}

/* Per cpu structures */
struct cpu_dbs_info {
	u64 prev_cpu_idle;
	u64 prev_cpu_wall;
	u64 prev_cpu_nice;
	/*
	 * Used to keep track of load in the previous interval. However, when
	 * explicitly set to zero, it is used as a flag to ensure that we copy
	 * the previous load to the current interval only once, upon the first
	 * wake-up from idle.
	 */
	unsigned int prev_load;
	struct update_util_data update_util;
	struct policy_dbs_info *policy_dbs;
};

struct od_cpu_dbs_info_s {
	struct cpu_dbs_info cdbs;
	struct cpufreq_frequency_table *freq_table;
	unsigned int freq_lo;
	unsigned int freq_lo_jiffies;
	unsigned int freq_hi_jiffies;
	unsigned int rate_mult;
	unsigned int sample_type:1;
};

struct cs_cpu_dbs_info_s {
	struct cpu_dbs_info cdbs;
	unsigned int down_skip;
	unsigned int requested_freq;
};

/* Per policy Governors sysfs tunables */
struct od_dbs_tuners {
	unsigned int ignore_nice_load;
	unsigned int sampling_rate;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int powersave_bias;
	unsigned int io_is_busy;
};

struct cs_dbs_tuners {
	unsigned int ignore_nice_load;
	unsigned int sampling_rate;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int freq_step;
};

/* Common Governor data across policies */
struct dbs_governor {
	struct cpufreq_governor gov;

	#define GOV_ONDEMAND		0
	#define GOV_CONSERVATIVE	1
	int governor;
	struct attribute_group *attr_group_gov_sys; /* one governor - system */
	struct attribute_group *attr_group_gov_pol; /* one governor - policy */

	/*
	 * Common data for platforms that don't set
	 * CPUFREQ_HAVE_GOVERNOR_PER_POLICY
	 */
	struct dbs_data *gdbs_data;

	struct cpu_dbs_info *(*get_cpu_cdbs)(int cpu);
	void *(*get_cpu_dbs_info_s)(int cpu);
	unsigned int (*gov_dbs_timer)(struct cpufreq_policy *policy);
	void (*gov_check_cpu)(int cpu, unsigned int load);
	int (*init)(struct dbs_data *dbs_data, bool notify);
	void (*exit)(struct dbs_data *dbs_data, bool notify);

	/* Governor specific ops, see below */
	void *gov_ops;
};

static inline struct dbs_governor *dbs_governor_of(struct cpufreq_policy *policy)
{
	return container_of(policy->governor, struct dbs_governor, gov);
}

/* Governor specific ops, will be passed to dbs_data->gov_ops */
struct od_ops {
	void (*powersave_bias_init_cpu)(int cpu);
	unsigned int (*powersave_bias_target)(struct cpufreq_policy *policy,
			unsigned int freq_next, unsigned int relation);
	void (*freq_increase)(struct cpufreq_policy *policy, unsigned int freq);
};

static inline int delay_for_sampling_rate(unsigned int sampling_rate)
{
	int delay = usecs_to_jiffies(sampling_rate);

	/* We want all CPUs to do sampling nearly on same jiffy */
	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	return delay;
}

#define declare_show_sampling_rate_min(_gov)				\
static ssize_t show_sampling_rate_min_gov_sys				\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)		\
{									\
	struct dbs_data *dbs_data = _gov##_dbs_gov.gdbs_data;		\
	return sprintf(buf, "%u\n", dbs_data->min_sampling_rate);	\
}									\
									\
static ssize_t show_sampling_rate_min_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	struct policy_dbs_info *policy_dbs = policy->governor_data;	\
	struct dbs_data *dbs_data = policy_dbs->dbs_data;		\
	return sprintf(buf, "%u\n", dbs_data->min_sampling_rate);	\
}

extern struct mutex dbs_data_mutex;
extern struct mutex cpufreq_governor_lock;
void dbs_check_cpu(struct cpufreq_policy *policy);
int cpufreq_governor_dbs(struct cpufreq_policy *policy, unsigned int event);
void od_register_powersave_bias_handler(unsigned int (*f)
		(struct cpufreq_policy *, unsigned int, unsigned int),
		unsigned int powersave_bias);
void od_unregister_powersave_bias_handler(void);
#endif /* _CPUFREQ_GOVERNOR_H */
