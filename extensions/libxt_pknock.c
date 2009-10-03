/*
 * Shared library add-on to iptables to add Port Knocking and SPA matching
 * support.
 *
 * (C) 2006-2009 J. Federico Hernandez <fede.hernandez@gmail.com>
 * (C) 2006 Luis Floreani <luis.floreani@gmail.com>
 *
 * $Id$
 *
 * This program is released under the terms of GNU GPL version 2.
 */
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <xtables.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_tables.h>
//#include <linux/netfilter_ipv4/ipt_pknock.h>
#include "xt_pknock.h"

static const struct option pknock_opts[] = {
	/* .name, .has_arg, .flag, .val */
	{ "knockports",	1,	0, 'k' },
	{ "time",		1,	0, 't' },
	{ "name",		1,	0, 'n' },
	{ "opensecret",	1,	0, 'a' },
	{ "closesecret",1,	0, 'z' },
	{ "strict",		0,	0, 'x' },
	{ "checkip",	0,	0, 'c' },
	{ "chkip",		0,	0, 'c' },
	{ .name = NULL }
};

/* Function which prints out usage message. */
static void pknock_help(void)
{
	printf("pknock match options:\n"
		" --knockports port[,port,port,...]	"
			"Matches destination port(s).\n"
		" --time seconds\n"
			"Time between port match.\n"
		" --secure				"
			"hmac must be in the packets.\n"
		" --strict				"
			"Knocks sequence must be exact.\n"
		" --name rule_name			"
			"Rule name.\n"
		" --checkip				"
			"Matches if the source ip is in the list.\n"
		" --chkip\n");
}

static unsigned int
parse_ports(const char *portstring, uint16_t *ports, const char *proto)
{
	char *buffer, *cp, *next;
	unsigned int i;

	buffer = strdup(portstring);
	if (buffer == NULL)
		xtables_error(OTHER_PROBLEM, "strdup failed");

	for (cp = buffer, i = 0; cp != NULL && i < XT_PKNOCK_MAX_PORTS; cp = next, ++i)
	{
		next=strchr(cp, ',');
		if (next != NULL)
			*next++ = '\0';
		ports[i] = xtables_parse_port(cp, proto);
	}

	if (cp != NULL)
		xtables_error(PARAMETER_PROBLEM, "too many ports specified");

	free(buffer);
	return i;
}

static char *
proto_to_name(uint8_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_UDP:
		return "udp";
	default:
		return NULL;
	}
}

static const char *
check_proto(uint16_t pnum, uint8_t invflags)
{
	char *proto;

	if (invflags & XT_INV_PROTO)
		xtables_error(PARAMETER_PROBLEM, PKNOCK "only works with TCP and UDP.");

	if ((proto = proto_to_name(pnum)) != NULL)
		return proto;
	else if (pnum == 0)
		xtables_error(PARAMETER_PROBLEM, PKNOCK "needs `-p tcp' or `-p udp'");
	else
		xtables_error(PARAMETER_PROBLEM, PKNOCK "only works with TCP and UDP.");
}

/* Function which parses command options; returns true if it ate an option */
static int
__pknock_parse(int c, char **argv, int invert, unsigned int *flags,
		struct xt_entry_match **match, uint16_t pnum,
		uint16_t invflags)
{
	const char *proto;
	struct xt_pknock_mtinfo *info = (void *)(*match)->data;

	switch (c) {
	case 'k': /* --knockports */
		if (*flags & XT_PKNOCK_KNOCKPORT)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --knockports twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);

		info->ports_count = parse_ports(optarg, info->port, proto);
		info->option |= XT_PKNOCK_KNOCKPORT;
		*flags |= XT_PKNOCK_KNOCKPORT;
#if DEBUG
		printf("ports_count: %d\n", info->ports_count);
#endif
		break;

	case 't': /* --time */
		if (*flags & XT_PKNOCK_TIME)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --time twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		info->max_time = atoi(optarg);
		info->option |= XT_PKNOCK_TIME;
		*flags |= XT_PKNOCK_TIME;
		break;

	case 'n': /* --name */
		if (*flags & XT_PKNOCK_NAME)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --name twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		memset(info->rule_name, 0, XT_PKNOCK_MAX_BUF_LEN + 1);
		strncpy(info->rule_name, optarg, XT_PKNOCK_MAX_BUF_LEN);

		info->rule_name_len = strlen(info->rule_name);
		info->option |= XT_PKNOCK_NAME;
		*flags |= XT_PKNOCK_NAME;
#if DEBUG
		printf("info->rule_name: %s\n", info->rule_name);
#endif
		break;

	case 'a': /* --opensecret */
		if (*flags & XT_PKNOCK_OPENSECRET)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --opensecret twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		memset(info->open_secret, 0, XT_PKNOCK_MAX_PASSWD_LEN + 1);
		strncpy(info->open_secret, optarg, XT_PKNOCK_MAX_PASSWD_LEN);

		info->open_secret_len = strlen(info->open_secret);
		info->option |= XT_PKNOCK_OPENSECRET;
		*flags |= XT_PKNOCK_OPENSECRET;
		break;

	case 'z': /* --closesecret */
		if (*flags & XT_PKNOCK_CLOSESECRET)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --closesecret twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		memset(info->close_secret, 0, XT_PKNOCK_MAX_PASSWD_LEN + 1);
		strncpy(info->close_secret, optarg, XT_PKNOCK_MAX_PASSWD_LEN);

		info->close_secret_len = strlen(info->close_secret);
		info->option |= XT_PKNOCK_CLOSESECRET;
		*flags |= XT_PKNOCK_CLOSESECRET;
		break;

	case 'c': /* --checkip */
		if (*flags & XT_PKNOCK_CHECKIP)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --checkip twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		info->option |= XT_PKNOCK_CHECKIP;
		*flags |= XT_PKNOCK_CHECKIP;
		break;

	case 'x': /* --strict */
		if (*flags & XT_PKNOCK_STRICT)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot use --strict twice.\n");

		xtables_check_inverse(argv[optind-1], &invert, &optind, 0);

		info->option |= XT_PKNOCK_STRICT;
		*flags |= XT_PKNOCK_STRICT;
		break;

	default:
		return 0;
	}

	if (invert)
		xtables_error(PARAMETER_PROBLEM, PKNOCK "does not support invert.");

	return 1;
}

static int pknock_parse(int c, char **argv, int invert, unsigned int *flags,
                		const void *e, struct xt_entry_match **match)
{
	const struct ipt_entry *entry = e;
	return __pknock_parse(c, argv, invert, flags, match,
			entry->ip.proto, entry->ip.invflags);
}

/* Final check. */
static void pknock_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM, PKNOCK "expection an option.\n");

	if (!(flags & XT_PKNOCK_NAME))
		xtables_error(PARAMETER_PROBLEM, PKNOCK
			"--name option is required.\n");

	if (flags & XT_PKNOCK_KNOCKPORT) {
		if (flags & XT_PKNOCK_CHECKIP)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot specify --knockports with --checkip.\n");
		if ((flags & XT_PKNOCK_OPENSECRET)
			&& !(flags & XT_PKNOCK_CLOSESECRET))
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"--opensecret must go with --closesecret.\n");
		if ((flags & XT_PKNOCK_CLOSESECRET)
			&& !(flags & XT_PKNOCK_OPENSECRET))
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"--closesecret must go with --opensecret.\n");
	}

	if (flags & XT_PKNOCK_CHECKIP) {
		if (flags & XT_PKNOCK_KNOCKPORT)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot specify --checkip with --knockports.\n");
		if ((flags & XT_PKNOCK_OPENSECRET)
			|| (flags & XT_PKNOCK_CLOSESECRET))
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot specify --opensecret and"
				" --closesecret with --checkip.\n");
		if (flags & XT_PKNOCK_TIME)
			xtables_error(PARAMETER_PROBLEM, PKNOCK
				"cannot specify --time with --checkip.\n");
	}
}

/* Prints out the matchinfo. */
static void pknock_print(const void *ip,
						const struct xt_entry_match *match, int numeric)
{
	const struct xt_pknock_mtinfo *info = (void *)match->data;
	int i;

	printf("pknock ");
	if (info->option & XT_PKNOCK_KNOCKPORT) {
		printf("knockports ");
		for (i = 0; i < info->ports_count; ++i)
			printf("%s%d", i ? "," : "", info->port[i]);
		printf(" ");
	}
	if (info->option & XT_PKNOCK_TIME)
		printf("time %ld ", (long)info->max_time);
	if (info->option & XT_PKNOCK_NAME)
		printf("name %s ", info->rule_name);
	if (info->option & XT_PKNOCK_OPENSECRET)
		printf("opensecret ");
	if (info->option & XT_PKNOCK_CLOSESECRET)
		printf("closesecret ");
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void pknock_save(const void *ip, const struct xt_entry_match *match)
{
	int i;
	const struct xt_pknock_mtinfo *info = (void *)match->data;

	if (info->option & XT_PKNOCK_KNOCKPORT) {
		printf("--knockports ");
		for (i = 0; i < info->ports_count; ++i)
			printf("%s%d", i ? "," : "", info->port[i]);
		printf(" ");
	}
	if (info->option & XT_PKNOCK_TIME)
		printf("--time %ld ", (long)info->max_time);
	if (info->option & XT_PKNOCK_NAME)
		printf("--name %s ", info->rule_name);
	if (info->option & XT_PKNOCK_OPENSECRET)
		printf("--opensecret ");
	if (info->option & XT_PKNOCK_CLOSESECRET)
		printf("--closesecret ");
	if (info->option & XT_PKNOCK_STRICT)
		printf("--strict ");
	if (info->option & XT_PKNOCK_CHECKIP)
		printf("--checkip ");
}

static struct xtables_match pknock_match = {
	.name		= "pknock",
	.version	= XTABLES_VERSION,
	.revision      = 1,
	.family		= AF_INET,
	.size          = XT_ALIGN(sizeof(struct xt_pknock_mtinfo)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_pknock_mtinfo)),
	.help		= pknock_help,
	.parse		= pknock_parse,
	.final_check	= pknock_check,
	.print		= pknock_print,
	.save		= pknock_save,
	.extra_opts	= pknock_opts
};

void _init(void)
{
	xtables_register_match(&pknock_match);
}
