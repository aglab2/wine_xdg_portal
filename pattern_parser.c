#include "pattern_parser_sp.h"

#include <stdbool.h>
#include <stddef.h>

#include "log.h"

struct PatternParser
{
    bool isWide;
    union
    {
        const char* pattern;
        const wchar_t* patternW;
    };
    char buf[64];
};

struct PatternParser* pattern_parser_init_a(const char* pattern)
{
    struct PatternParser* parser = malloc(sizeof(struct PatternParser));
    parser->isWide = false;
    parser->pattern = pattern;
    return parser;
}

struct PatternParser* pattern_parser_init_w(const wchar_t* pattern)
{
    struct PatternParser* parser = malloc(sizeof(struct PatternParser));
    parser->isWide = true;
    parser->patternW = pattern;
    return parser;
}

static bool pattern_parser_is_done(struct PatternParser* parser)
{
    if (parser->isWide)
    {
        return *parser->patternW == L'\0';
    }
    else
    {
        return *parser->pattern == '\0';
    }
}

static inline const char pattern_parser_next_sym(struct PatternParser* parser)
{
    if (parser->isWide)
    {
        // TODO: Handle wide characters properly. For now, just return the lower byte.
        return (const char)*parser->patternW++;
    }
    else
    {
        return *parser->pattern++;
    }
}

const char* pattern_parser_next(struct PatternParser* parser)
{
    int i = 0;
    if (pattern_parser_is_done(parser))
    {
        return NULL;
    }

    while (!pattern_parser_is_done(parser))
    {
        char c = pattern_parser_next_sym(parser);
        if (c == ';')
        {
            parser->buf[i] = '\0';
            return parser->buf;
        }
        else
        {
            if (i > sizeof(parser->buf) - 2)
            {
                log("Pattern parser buffer overflow\n");
                return NULL;
            }

            parser->buf[i++] = c;
        }
    }

    if (i)
    {
        parser->buf[i] = '\0';
        return parser->buf;
    }

    return NULL;
}
