/*
 *  (C) 2004-2009  Dominik Brodowski <linux@dominikbrodowski.de>
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 */


#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include <getopt.h>

#include "cpufreq.h"

#ifdef NLS
#include <libintl.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop(String)
#else
#define gettext_noop(String) String
#define _(String) gettext_noop (String)
#define gettext(String) gettext_noop (String)
#define N_(String) gettext_noop (String)
#define textdomain(String)
#endif

#define NORM_FREQ_LEN 32

static void print_header(void)
{
        printf(PACKAGE " " VERSION ": cpufreq-set (C) Dominik Brodowski 2004-2009\n");
	printf(gettext("Report errors and bugs to %s, please.\n"), PACKAGE_BUGREPORT);
}

static void print_help(void)
{
	printf(gettext("Usage: cpufreq-set [options]\n"));
	printf(gettext("Options:\n"));
	printf(gettext("  -c CPU, --cpu CPU        number of CPU where cpufreq settings shall be modified\n"));
	printf(gettext("  -d FREQ, --min FREQ      new minimum CPU frequency the governor may select\n"));
	printf(gettext("  -u FREQ, --max FREQ      new maximum CPU frequency the governor may select\n"));
	printf(gettext("  -g GOV, --governor GOV   new cpufreq governor\n"));
	printf(gettext("  -f FREQ, --freq FREQ     specific frequency to be set. Requires userspace\n"
	       "                           governor to be available and loaded\n"));
	printf(gettext("  -r, --related            Switches all hardware-related CPUs\n"));
	printf(gettext("  -h, --help               Prints out this screen\n"));
	printf("\n");
	printf(gettext("Notes:\n"
	       "1. Omitting the -c or --cpu argument is equivalent to setting it to zero\n"
	       "2. The -f FREQ, --freq FREQ parameter cannot be combined with any other parameter\n"
	       "   except the -c CPU, --cpu CPU parameter\n"
	       "3. FREQuencies can be passed in Hz, kHz (default), MHz, GHz, or THz\n"
	       "   by postfixing the value with the wanted unit name, without any space\n"
	       "   (FREQuency in kHz =^ Hz * 0.001 =^ MHz * 1000 =^ GHz * 1000000).\n"));

}

static struct option set_opts[] = {
	{ .name="cpu",		.has_arg=required_argument,	.flag=NULL,	.val='c'},
	{ .name="min",		.has_arg=required_argument,	.flag=NULL,	.val='d'},
	{ .name="max",		.has_arg=required_argument,	.flag=NULL,	.val='u'},
	{ .name="governor",	.has_arg=required_argument,	.flag=NULL,	.val='g'},
	{ .name="freq",		.has_arg=required_argument,	.flag=NULL,	.val='f'},
	{ .name="help",		.has_arg=no_argument,		.flag=NULL,	.val='h'},
	{ .name="related",	.has_arg=no_argument,		.flag=NULL,	.val='r'},
};

static void print_error(void)
{
	printf(gettext("Error setting new values. Common errors:\n"
			"- Do you have proper administration rights? (super-user?)\n"
			"- Is the governor you requested available and modprobed?\n"
			"- Trying to set an invalid policy?\n"
			"- Trying to set a specific frequency, but userspace governor is not available,\n"
			"   for example because of hardware which cannot be set to a specific frequency\n"
			"   or because the userspace governor isn't loaded?\n"));
};

struct freq_units {
	char*		str_unit;
	int		power_of_ten;
};

const struct freq_units def_units[] = {
	{"hz", -3},
	{"khz", 0}, /* default */
	{"mhz", 3},
	{"ghz", 6},
	{"thz", 9},
	{NULL, 0}
};

static void print_unknown_arg(void)
{
	print_header();
	printf(gettext("invalid or unknown argument\n"));
	print_help();
}

static unsigned long string_to_frequency(const char *str)
{
	char normalized[NORM_FREQ_LEN];
	const struct freq_units *unit;
	const char *scan;
	char *end;
	unsigned long freq;
	int power = 0, match_count = 0, i, cp, pad;

	while (*str == '0')
		str++;

	for (scan = str; isdigit(*scan) || *scan == '.'; scan++) {
		if (*scan == '.' && match_count == 0)
			match_count = 1;
		else if (*scan == '.' && match_count == 1)
			return 0;
	}

	if (*scan) {
		match_count = 0;
		for (unit = def_units; unit->str_unit; unit++) {
			for (i = 0;
			     scan[i] && tolower(scan[i]) == unit->str_unit[i];
			     ++i)
				continue;
			if (scan[i])
				continue;
			match_count++;
			power = unit->power_of_ten;
		}
		if (match_count != 1)
			return 0;
	}

	/* count the number of digits to be copied */
	for (cp = 0; isdigit(str[cp]); cp++)
		continue;

	if (str[cp] == '.') {
		while (power > -1 && isdigit(str[cp+1]))
			cp++, power--;
	}
	if (power >= -1)	/* not enough => pad */
		pad = power + 1;
	else			/* to much => strip */
		pad = 0, cp += power + 1;
	/* check bounds */
	if (cp <= 0 || cp + pad > NORM_FREQ_LEN - 1)
		return 0;

	/* copy digits */
	for (i = 0; i < cp; i++, str++) {
		if (*str == '.')
			str++;
		normalized[i] = *str;
	}
	/* and pad */
	for (; i < cp + pad; i++)
		normalized[i] = '0';

	/* round up, down ? */
	match_count = (normalized[i-1] >= '5');
	/* and drop the decimal part */
	normalized[i-1] = 0; /* cp > 0 && pad >= 0 ==> i > 0 */

	/* final conversion (and applying rounding) */
	errno = 0;
	freq = strtoul(normalized, &end, 10);
	if (errno)
		return 0;
	else {
		if (match_count && freq != ULONG_MAX)
			freq++;
		return freq;
	}
}

static int do_new_policy(unsigned int cpu, struct cpufreq_policy *new_pol)
{
	struct cpufreq_policy *cur_pol = cpufreq_get_policy(cpu);
	int ret;

	if (!cur_pol) {
		printf(gettext("wrong, unknown or unhandled CPU?\n"));
		return -EINVAL;
	}

	if (!new_pol->min)
		new_pol->min = cur_pol->min;

	if (!new_pol->max)
		new_pol->max = cur_pol->max;

	if (!new_pol->governor)
		new_pol->governor = cur_pol->governor;

	ret = cpufreq_set_policy(cpu, new_pol);

	cpufreq_put_policy(cur_pol);

	return ret;
}

	
static int do_one_cpu(unsigned int cpu, struct cpufreq_policy *new_pol,
		unsigned long freq, unsigned int pc)
{
	switch (pc) {
	case 0:
		return cpufreq_set_frequency(cpu, freq);

	case 1:
		/* if only one value of a policy is to be changed, we can
		 * use a "fast path".
		 */
		if (new_pol->min)
			return cpufreq_modify_policy_min(cpu, new_pol->min);
		else if (new_pol->max)
			return cpufreq_modify_policy_max(cpu, new_pol->max);
		else if (new_pol->governor)
			return cpufreq_modify_policy_governor(cpu, new_pol->governor);

	default:
		/* slow path */
		return do_new_policy(cpu, new_pol);
	}
}


int main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind, opterr, optopt;
	int ret = 0, cont = 1;
	unsigned long freq = 0;
	char gov[20];
	int double_parm = 0;
	int related = 0;
	int policychange = 0;
	struct cpufreq_policy new_pol = {
		.min = 0,
		.max = 0,
		.governor = NULL,
	};
	struct cpufreq_affected_cpus single_cpu = {
		.cpu = 0,
		.next = NULL,
		.first = &single_cpu,
	};
	struct cpufreq_affected_cpus *cpus = NULL;

	setlocale(LC_ALL, "");
	textdomain (PACKAGE);

	/* parameter parsing */
	do {
		ret = getopt_long(argc, argv, "c:d:u:g:f:hr", set_opts, NULL);
		switch (ret) {
		case '?':
			print_unknown_arg();
			return -EINVAL;
		case 'h':
			print_header();
			print_help();
			return 0;
		case -1:
			cont = 0;
			break;
		case 'r':
			if (related)
				double_parm++;
			related++;
			break;
		case 'c':
			if (cpus)
				double_parm++;
			cpus = &single_cpu;
			if ((sscanf(optarg, "%d ", &single_cpu.cpu)) != 1) {
				print_unknown_arg();
				return -EINVAL;
                        }
			break;
		case 'd':
			if (new_pol.min)
				double_parm++;
			policychange++;
			new_pol.min = string_to_frequency(optarg);
			if (new_pol.min == 0) {
				print_unknown_arg();
				return -EINVAL;
			}
			break;
		case 'u':
			if (new_pol.max)
				double_parm++;
			policychange++;
			new_pol.max = string_to_frequency(optarg);
			if (new_pol.max == 0) {
				print_unknown_arg();
				return -EINVAL;
			}
			break;
		case 'f':
			if (freq)
				double_parm++;
			freq = string_to_frequency(optarg);
			if (freq == 0) {
				print_unknown_arg();
				return -EINVAL;
			}
			break;
		case 'g':
			if (new_pol.governor)
				double_parm++;
			policychange++;
			if ((strlen(optarg) < 3) || (strlen(optarg) > 18)) {
				print_unknown_arg();
				return -EINVAL;
                        }
			if ((sscanf(optarg, "%s", gov)) != 1) {
				print_unknown_arg();
				return -EINVAL;
                        }
			new_pol.governor = gov;
			break;
		}
	} while(cont);

	/* parameter checking */
	if (double_parm) {
		print_header();
		printf("the same parameter was passed more than once\n");
		return -EINVAL;
	}

	if (freq && policychange) {
		printf(gettext("the -f/--freq parameter cannot be combined with -d/--min, -u/--max or\n"
				"-g/--governor parameters\n"));
		return -EINVAL;
	}

	if (!freq && !policychange) {
		printf(gettext("At least one parameter out of -f/--freq, -d/--min, -u/--max, and\n"
				"-g/--governor must be passed\n"));
		return -EINVAL;
	}


	/* which CPUs shall we modify? */
	if (!cpus)
		cpus = &single_cpu;

	if (related)
		cpus = cpufreq_get_related_cpus(cpus->cpu);

	/* loop over CPUs */
	while (1) {
		ret = do_one_cpu(cpus->cpu, &new_pol, freq, policychange);
		if (ret)
			break;

		if (!cpus->next)
			break;

		cpus = cpus->next;
	}

	/* cleanup */
	if (cpus->first != &single_cpu)
		cpufreq_put_related_cpus(cpus->first);

	if (ret)
		print_error();

	return ret;
}
