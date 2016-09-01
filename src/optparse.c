#include <stdio.h>
#include "optparse.h"

#define opterror(options, format, ...) \
    snprintf(options->errmsg, sizeof(options->errmsg), format, __VA_ARGS__);

#define options_argv(i) \
    ((i) < options->argc ? options->argv[i] : NULL)

void optparse_init(struct optparse *options, int argc, char **argv)
{
    options->argv = argv;
    options->argc = argc;
    options->permute = 1;
    options->optind = 1;
    options->subopt = 0;
    options->optarg = NULL;
    options->errmsg[0] = '\0';
}

static inline int
is_dashdash(const char *arg)
{
    return arg != NULL && arg[0] == '-' && arg[1] == '-' && arg[2] == '\0';
}

static inline int
is_shortopt(const char *arg)
{
    return arg != NULL && arg[0] == '-' && arg[1] != '-' && arg[1] != '\0';
}

static inline int
is_longopt(const char *arg)
{
    return arg != NULL && arg[0] == '-' && arg[1] == '-' && arg[2] != '\0';
}

static void
permute(struct optparse *options, int index)
{
    char *nonoption = options->argv[index];
    for (int i = index; i < options->optind - 1; i++)
        options->argv[i] = options->argv[i + 1];
    options->argv[options->optind - 1] = nonoption;
}

static enum optparse_argtype
argtype(const char *optstring, char c)
{
    if (c == ':')
        return -1;
    for (; *optstring && c != *optstring; optstring++);
    if (!*optstring)
        return -1;
    enum optparse_argtype count = OPTPARSE_NONE;
    if (optstring[1] == ':')
        count += optstring[2] == ':' ? 2 : 1;
    return count;
}

int optparse(struct optparse *options, const char *optstring)
{
    options->errmsg[0] = '\0';
    options->optopt = 0;
    options->optarg = NULL;
    char *option = options_argv(options->optind);
    if (option == NULL) {
        return -1;
    } else if (is_dashdash(option)) {
        options->optind++; // consume "--"
        return -1;
    } else if (!is_shortopt(option)) {
        if (options->permute) {
            int index = options->optind;
            options->optind++;
            int r = optparse(options, optstring);
            permute(options, index);
            options->optind--;
            return r;
        } else {
            return -1;
        }
    }
    option += options->subopt + 1;
    options->optopt = option[0];
    int type = argtype(optstring, option[0]);
    char *next = options_argv(options->optind + 1);
    switch (type) {
    case -1:
        opterror(options, "invalid option -- '%c'", option[0]);
        options->optind++;
        return '?';
    case OPTPARSE_NONE:
        if (option[1]) {
            options->subopt++;
        } else {
            options->subopt = 0;
            options->optind++;
        }
        return option[0];
    case OPTPARSE_REQUIRED:
        options->subopt = 0;
        options->optind++;
        if (option[1]) {
            options->optarg = option + 1;
        } else if (next != NULL) {
            options->optarg = next;
            options->optind++;
        } else {
            opterror(options, "option requires an argument -- '%c'", option[0]);
            options->optarg = NULL;
            return '?';
        }
        return option[0];
    case OPTPARSE_OPTIONAL:
        options->subopt = 0;
        options->optind++;
        if (option[1])
            options->optarg = option + 1;
        else
            options->optarg = NULL;
        return option[0];
    }
    return 0;
}

char *optparse_arg(struct optparse *options)
{
    options->subopt = 0;
    char *option = options->argv[options->optind];
    if (option != NULL)
        options->optind++;
    return option;
}

static inline int
longopts_end(const struct optparse_long *longopts, int i)
{
    return !longopts[i].longname && !longopts[i].shortname;
}

static size_t
optstring_length(const struct optparse_long *longopts)
{
    int length = 0;
    for (int i = 0; !longopts_end(longopts, i); i++, length++)
        length += longopts[i].argtype;
    return length + 1;
}

static void
optstring_from_long(const struct optparse_long *longopts, char *optstring)
{
    char *p = optstring;
    for (int i = 0; !longopts_end(longopts, i); i++) {
        if (longopts[i].shortname) {
            *p++ = longopts[i].shortname;
            for (unsigned int a = 0; a < longopts[i].argtype; a++)
                *p++ = ':';
        }
    }
    *p = '\0';
}

/* Unlike strcmp(), handles options containing "=". */
static int
longopts_match(const char *longname, const char *option)
{
    if (longname == NULL)
        return 0;
    const char *a = option, *n = longname;
    for (; *a && *n && *a != '='; a++, n++)
        if (*a != *n)
            return 0;
    return *n == '\0' && (*a == '\0' || *a == '=');
}

/* Return the part after "=", or NULL. */
static const char *
longopts_arg(const char *option)
{
    for (; *option && *option != '='; option++);
    if (*option == '=')
        return option + 1;
    else
        return NULL;
}

static int
long_fallback(struct optparse *options,
              const struct optparse_long *longopts,
              int *longindex)
{
    char optstring[optstring_length(longopts)];
    optstring_from_long(longopts, optstring);
    int result = optparse(options, optstring);
    if (longindex != NULL) {
        *longindex = -1;
        if (result != -1)
            for (int i = 0; !longopts_end(longopts, i); i++)
                if (longopts[i].shortname == options->optopt)
                    *longindex = i;
    }
    return result;
}

int
optparse_long(struct optparse *options,
              const struct optparse_long *longopts,
              int *longindex)
{
//    printf("%i < %i\n",options->optind,options->argc);
    char *option = options_argv(options->optind);
    if (option == NULL) {
        return -1;
    } else if (is_dashdash(option)) {
        options->optind++; // consume "--"
        return -1;
    } else if (is_shortopt(option)) {
        return long_fallback(options, longopts, longindex);
    } else if (!is_longopt(option)) {
        if (options->permute) {
            int index = options->optind;
            options->optind++;
            int r = optparse_long(options, longopts, longindex);
            permute(options, index);
            options->optind--;
            return r;
        } else {
            return -1;
        }
    }

    /* Parse as long option. */
    options->errmsg[0] = '\0';
    options->optopt = 0;
    options->optarg = NULL;
    option += 2; // skip "--"
    options->optind++;
    for (int i = 0; !longopts_end(longopts, i); i++) {
        const char *name = longopts[i].longname;
        if (longopts_match(name, option)) {
            if (longindex)
                *longindex = i;
            options->optopt = longopts[i].shortname;
            const char *arg = longopts_arg(option);
            if (longopts[i].argtype == OPTPARSE_NONE && arg != NULL) {
                opterror(options, "option takes no arguments -- '%s'", name);
                return '?';
            } if (arg != NULL) {
                options->optarg = arg;
            } else if (longopts[i].argtype == OPTPARSE_REQUIRED) {
                options->optarg = options_argv(options->optind);
                options->optind++;
                if (options->optarg == NULL) {
                    opterror(options, "option requires argument -- '%s'", name);
                    return '?';
                }
            }
            return options->optopt;
        }
    }
    opterror(options, "invalid option -- '%s'", option);
    return '?';
}
