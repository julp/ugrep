#include "common.h"

#define ERROR_MAX_LEN 596

static const char *icu_error_desc(UErrorCode);

#ifdef DEBUG
const char *ubasename(const char *filename)
{
    const char *c;

    if (NULL == (c = strrchr(filename, DIRECTORY_SEPARATOR))) {
        return filename;
    } else {
        return c + 1;
    }
}
#endif /* DEBUG */

#ifdef _MSC_VER
# include <windows.h>

error_t *error_win32_vnew(int type, const char *format, va_list args) /* WARN_UNUSED_RESULT */
{
    LPWSTR *buf = NULL;
    error_t *error = NULL;
    int32_t length, buf_len;
    UChar buffer[ERROR_MAX_LEN + 1];

    FormatMessageW(
       FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR) &buf, 0, NULL
    );
    if (NULL != buf) {
        buf_len = u_strlen((UChar *) buf);
        error = mem_new(*error);
        length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
        // TODO: assume -1 != length ?
        error->type = type;
        error->message = mem_new_n(*error->message, length + buf_len + 1);
//         u_strcpy(error->message, buffer);
//         u_strcpy(error->message + length, buf, buf_len);
        u_memcpy(error->message, (UChar *) buffer, length);
        u_memcpy(error->message + length, (UChar *) buf, buf_len);
        error->message[length + buf_len] = 0;
        LocalFree(buf);
    }

    return error;
}

error_t *error_win32_new(int type, const char *format, ...) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    va_list args;

    va_start(args, format);
    error = error_win32_vnew(type, format, args);
    va_end(args);

    return error;
}

# ifdef DEBUG
void _error_win32_set(error_t **error, int type, const char *format, ...)
# else
void error_win32_set(error_t **error, int type, const char *format, ...)
# endif /* DEBUG */
{
    va_list args;
    error_t *tmp;

    if (NULL != error) {
        va_start(args, format);
        tmp = error_win32_vnew(type, format, args);
        va_end(args);
        if (NULL == *error) {
            *error = tmp;
        } else {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*error)->message, tmp->message);
        }
    }
}
#endif /* _MSC_VER */

error_t *error_vnew(UGREP_FILE_LINE_FUNC_DC int type, const char *format, va_list args) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    UChar buffer[ERROR_MAX_LEN + 1];
    int32_t total_length, part_length;

    total_length = 0;
    error = mem_new(*error);
    error->type = type;
    error->message = NULL;
#ifdef DEBUG
    // NOTE: *s*printf does not include the terminating null character
    part_length = u_snprintf(buffer, ERROR_MAX_LEN, "%s:%d: ", ubasename(__ugrep_file), __ugrep_line);
    assert(-1 != part_length);
    error->message = mem_new_n(*error->message, /*total_length + */part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message/* + total_length*/, buffer, part_length);
    total_length += part_length;
#endif
    part_length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
    assert(-1 != part_length);
    error->message = mem_renew(error->message, *error->message, total_length + part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message + total_length, buffer, part_length);
    total_length += part_length;
#ifdef DEBUG
    part_length = u_snprintf(buffer, ERROR_MAX_LEN, GRAY(" in %s()"), __ugrep_func);
    assert(-1 != part_length);
    error->message = mem_renew(error->message, *error->message, total_length + part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message + total_length, buffer, part_length);
    total_length += part_length;
#endif
    /*u_memcpy(error->message + total_length, EOL, EOL_LEN);
    total_length += EOL_LEN;*/
    error->message[total_length] = 0;

    return error;
}

error_t *error_new(UGREP_FILE_LINE_FUNC_DC int type, const char *format, ...) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    va_list args;

    va_start(args, format);
    error = error_vnew(UGREP_FILE_LINE_FUNC_RELAY_CC type, format, args);
    va_end(args);

    return error;
}

void _error_set(UGREP_FILE_LINE_FUNC_DC error_t **error, int type, const char *format, ...)
{
    va_list args;
    error_t *tmp;

    if (NULL != error) {
        va_start(args, format);
        tmp = error_vnew(UGREP_FILE_LINE_FUNC_RELAY_CC type, format, args);
        va_end(args);
        if (NULL == *error) {
            *error = tmp;
        } else {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*error)->message, tmp->message);
        }
    }
}

//
// actual:
// re.c:59:ICU Error "U_REGEX_MISSING_CLOSE_BRACKET" from uregex_open() in engine_re_compile(): missing closing bracket on a bracket expression
//
// wanted:
// re.c:59: missing closing bracket on a bracket expression (U_REGEX_MISSING_CLOSE_BRACKET) *from uregex_open() in engine_re_compile()*: X
//
// "%s:%d: %s (%s) from %s in %s: " format,
// __FILE__, __LINE__, icu_error_desc(status), u_errorName(status), function, __func__, ...
//
// "%s (%s): " format,
// icu_error_desc(status), u_errorName(status), ...
//
error_t *error_icu_vnew(UGREP_FILE_LINE_FUNC_DC int type, UErrorCode code, UParseError *pe, const UChar *pattern, const char *function, const char *format, va_list args) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    const char *desc;
    UChar buffer[ERROR_MAX_LEN + 1];
    int32_t total_length, part_length;

    total_length = 0;
    error = mem_new(*error);
    error->type = type;
    error->message = NULL;
#ifdef DEBUG
    // NOTE: *s*printf does not include the terminating null character
    part_length = u_snprintf(buffer, ERROR_MAX_LEN, "%s:%d: ", ubasename(__ugrep_file), __ugrep_line);
    assert(-1 != part_length);
    error->message = mem_new_n(*error->message, /*total_length + */part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message/* + total_length*/, buffer, part_length);
    total_length += part_length;
#endif
    desc = icu_error_desc(code);
    part_length = u_snprintf(buffer, ERROR_MAX_LEN, "%s (%s): ", desc, u_errorName(code));
    assert(-1 != part_length);
    error->message = mem_renew(error->message, *error->message, total_length + part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message + total_length, buffer, part_length);
    total_length += part_length;
    if (NULL != format) {
        part_length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
        assert(-1 != part_length);
        error->message = mem_renew(error->message, *error->message, total_length + part_length/* + STR_LEN(": ")*//* + EOL_LEN*/ + 1);
        u_memcpy(error->message + total_length, buffer, part_length);
        total_length += part_length;
//         error->message[total_length++] = ':';
//         error->message[total_length++] = ' ';
    }
//     error->message = mem_renew(error->message, *error->message, total_length + part_length/* + EOL_LEN*/ + 1);
//     u_charsToUChars(desc, error->message + total_length, part_length/* + EOL_LEN*/ + 1);
//     total_length += part_length;
//     if (NULL != format) {
//         u_charsToUChars(": ", error->message + desc_length, STR_SIZE(": "));
//         desc_length += STR_LEN(": ");
//         u_strcpy(error->message + desc_length, buffer);
//         u_memcpy(error->message + total_length, buffer, desc_length);
//         error->message = mem_renew(error->message, *error->message, total_length + STR_LEN(": ")/* + EOL_LEN*/ + 1);
//         error->message[total_length++] = ':';
//         error->message[total_length++] = ' ';
//     }
    if (NULL != pe && -1 != pe->line && NULL != pattern) {
        // "Invalid pattern: error at offset %d\n\t%S\n\t%*c\n", pe->offset, pattern, pe->offset, '^'
        part_length = u_snprintf(buffer, ERROR_MAX_LEN, "invalid pattern, error at offset %d\n\t%S\n\t%*c\n", pe->offset, pattern, pe->offset, '^');
        assert(-1 != part_length);
        error->message = mem_renew(error->message, *error->message, total_length + part_length/* + STR_LEN(": ")*//* + EOL_LEN*/ + 1);
        u_memcpy(error->message + total_length, buffer, part_length);
        total_length += part_length;
    }
#ifdef DEBUG
    part_length = u_snprintf(buffer, ERROR_MAX_LEN, GRAY(" from %s in %s()"), function, __ugrep_func);
    assert(-1 != part_length);
    error->message = mem_renew(error->message, *error->message, total_length + part_length/* + EOL_LEN*/ + 1);
    u_memcpy(error->message + total_length, buffer, part_length);
    total_length += part_length;
#endif
    /*u_memcpy(error->message + total_length, EOL, EOL_LEN);
    total_length += EOL_LEN;*/
    error->message[total_length] = 0;

    return error;
}

error_t *error_icu_new(UGREP_FILE_LINE_FUNC_DC int type, UErrorCode code, UParseError *pe, const UChar *pattern, const char *function, const char *format, ...) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    va_list args;

    va_start(args, format);
    error = error_icu_vnew(UGREP_FILE_LINE_FUNC_RELAY_CC type, code, pe, pattern, function, format, args);
    va_end(args);

    return error;
}

void _error_icu_set(UGREP_FILE_LINE_FUNC_DC error_t **error, int type, UErrorCode code, UParseError *pe, const UChar *pattern, const char *function, const char *format, ...)
{
    va_list args;
    error_t *tmp;

    if (NULL != error) {
        va_start(args, format);
        tmp = error_icu_vnew(UGREP_FILE_LINE_FUNC_RELAY_CC type, code, pe, pattern, function, format, args);
        va_end(args);
        if (NULL == *error) {
            *error = tmp;
        } else {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*error)->message, tmp->message);
        }
    }
}

void error_destroy(error_t *error)
{
    if (NULL != error) {
        if (error->message) {
            free(error->message);
        }
        free(error);
        error = NULL;
    }
}

void error_propagate(error_t **dst, error_t *src)
{
    if (NULL == dst) {
        if (NULL != src) {
            error_destroy(src);
        }
    } else {
        if (NULL != *dst) {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*dst)->message, src->message);
        } else {
            *dst = src;
        }
    }
}

static const char *icu_error_desc(UErrorCode code)
{
    switch (code) {
        // warnings
        case U_USING_FALLBACK_WARNING:
            return "a resource bundle lookup returned a fallback result";
        case U_USING_DEFAULT_WARNING:
            return "a resource bundle lookup returned a result from the root locale";
        case U_SAFECLONE_ALLOCATED_WARNING:
            return "a safeclone operation required allocating memory";
        case U_STATE_OLD_WARNING:
            return "ICU has to use compatibility layer to construct the service. Expect performance/memory usage degradation. Consider upgrading";
        case U_STRING_NOT_TERMINATED_WARNING:
            return "an output string could not be NUL-terminated because output length==destCapacity";
        case U_SORT_KEY_TOO_SHORT_WARNING:
            return "number of levels requested in getBound is higher than the number of levels in the sort key";
        case U_AMBIGUOUS_ALIAS_WARNING:
            return "this converter alias can go to different converter implementations";
        case U_DIFFERENT_UCA_VERSION:
            return "ucol_open encountered a mismatch between UCA version and collator image version, so the collator was constructed from rules. No impact to further function";
        case U_PLUGIN_CHANGED_LEVEL_WARNING:
            return "a plugin caused a level change. May not be an error, but later plugins may not load";
        // standard errors
        case U_ZERO_ERROR:
            return "no error, no warning";
        case U_ILLEGAL_ARGUMENT_ERROR:
            return "illegal or inappropriate argument";
        case U_MISSING_RESOURCE_ERROR:
            return "the requested resource cannot be found";
        case U_INVALID_FORMAT_ERROR:
            return "data format is not what is expected";
        case U_FILE_ACCESS_ERROR:
            return "the requested file cannot be found";
        case U_INTERNAL_PROGRAM_ERROR:
            return "bug in ICU's code";
        case U_MESSAGE_PARSE_ERROR:
            return "unable to parse a message (message format)";
        case U_MEMORY_ALLOCATION_ERROR:
            return "memory allocation error";
        case U_INDEX_OUTOFBOUNDS_ERROR:
            return "trying to access the index that is out of bounds";
        case U_PARSE_ERROR:
            return "unexpected error while parsing";
        case U_INVALID_CHAR_FOUND:
            return "unmappable input sequence or invalid character";
        case U_TRUNCATED_CHAR_FOUND:
            return "incomplete input sequence";
        case U_ILLEGAL_CHAR_FOUND:
            return "illegal input sequence/combination of input units";
        case U_INVALID_TABLE_FORMAT:
            return "conversion table file found, but corrupted";
        case U_INVALID_TABLE_FILE:
            return "conversion table file not found";
        case U_BUFFER_OVERFLOW_ERROR:
            return "a result would not fit in the supplied buffer";
        case U_UNSUPPORTED_ERROR:
            return "requested operation not supported in current context";
        case U_RESOURCE_TYPE_MISMATCH:
            return "an operation is requested over a resource that does not support it";
        case U_ILLEGAL_ESCAPE_SEQUENCE:
            return "ISO-2022 illlegal escape sequence";
        case U_UNSUPPORTED_ESCAPE_SEQUENCE:
            return "ISO-2022 unsupported escape sequence";
        case U_NO_SPACE_AVAILABLE:
            return "no space available for in-buffer expansion for Arabic shaping";
        case U_CE_NOT_FOUND_ERROR:
            return "currently used only while setting variable top, but can be used generally";
        case U_PRIMARY_TOO_LONG_ERROR:
            return "user tried to set variable top to a primary that is longer than two bytes";
        case U_STATE_TOO_OLD_ERROR:
            return "ICU cannot construct a service from this state, as it is no longer supported";
        case U_TOO_MANY_ALIASES_ERROR:
            return "there are too many aliases in the path to the requested resource. It is very possible that a circular alias definition has occured";
        case U_ENUM_OUT_OF_SYNC_ERROR:
            return "UEnumeration out of sync with underlying collection";
        case U_INVARIANT_CONVERSION_ERROR:
            return "unable to convert a UChar* string to char* with the invariant converter";
        case U_INVALID_STATE_ERROR:
            return "requested operation can not be completed with ICU in its current state";
        case U_COLLATOR_VERSION_MISMATCH:
            return "collator version is not compatible with the base version";
        case U_USELESS_COLLATOR_ERROR:
            return "collator is options only and no base is specified";
        case U_NO_WRITE_PERMISSION:
            return "attempt to modify read-only or constant data";
        // Transliterator
        case U_BAD_VARIABLE_DEFINITION:
            return "missing '$' or duplicate variable name";
        case U_MALFORMED_RULE:
            return "elements of a rule are misplaced";
        case U_MALFORMED_SET:
            return "a UnicodeSet pattern is invalid";
        case U_MALFORMED_SYMBOL_REFERENCE:
            return "UNUSED as of ICU 2.4";
        case U_MALFORMED_UNICODE_ESCAPE:
            return "a Unicode escape pattern is invalid";
        case U_MALFORMED_VARIABLE_DEFINITION:
            return "a variable definition is invalid";
        case U_MALFORMED_VARIABLE_REFERENCE:
            return "a variable reference is invalid";
        case U_MISMATCHED_SEGMENT_DELIMITERS:
            return "UNUSED as of ICU 2.4";
        case U_MISPLACED_ANCHOR_START:
            return "a start anchor appears at an illegal position";
        case U_MISPLACED_CURSOR_OFFSET:
            return "a cursor offset occurs at an illegal position";
        case U_MISPLACED_QUANTIFIER:
            return "a quantifier appears after a segment close delimiter";
        case U_MISSING_OPERATOR:
            return "a rule contains no operator";
        case U_MISSING_SEGMENT_CLOSE:
            return "UNUSED as of ICU 2.4";
        case U_MULTIPLE_ANTE_CONTEXTS:
            return "more than one ante context";
        case U_MULTIPLE_CURSORS:
            return "more than one cursor";
        case U_MULTIPLE_POST_CONTEXTS:
            return "more than one post context";
        case U_TRAILING_BACKSLASH:
            return "a dangling backslash";
        case U_UNDEFINED_SEGMENT_REFERENCE:
            return "a segment reference does not correspond to a defined segment";
        case U_UNDEFINED_VARIABLE:
            return "a variable reference does not correspond to a defined variable";
        case U_UNQUOTED_SPECIAL:
            return "a special character was not quoted or escaped";
        case U_UNTERMINATED_QUOTE:
            return "a closing single quote is missing";
        case U_RULE_MASK_ERROR:
            return "a rule is hidden by an earlier more general rule";
        case U_MISPLACED_COMPOUND_FILTER:
            return "a compound filter is in an invalid location";
        case U_MULTIPLE_COMPOUND_FILTERS:
            return "more than one compound filter";
        case U_INVALID_RBT_SYNTAX:
            return "a \"::id\" rule was passed to the RuleBasedTransliterator parser";
        case U_INVALID_PROPERTY_PATTERN:
            return "UNUSED as of ICU 2.4";
        case U_MALFORMED_PRAGMA:
            return "a 'use' pragma is invlalid";
        case U_UNCLOSED_SEGMENT:
            return "a closing ')' is missing";
        case U_ILLEGAL_CHAR_IN_SEGMENT:
            return "UNUSED as of ICU 2.4";
        case U_VARIABLE_RANGE_EXHAUSTED:
            return "too many stand-ins generated for the given variable range";
        case U_VARIABLE_RANGE_OVERLAP:
            return "the variable range overlaps characters used in rules";
        case U_ILLEGAL_CHARACTER:
            return "a special character is outside its allowed context";
        case U_INTERNAL_TRANSLITERATOR_ERROR:
            return "internal transliterator system error";
        case U_INVALID_ID:
            return "a \"::id\" rule specifies an unknown transliterator";
        case U_INVALID_FUNCTION:
            return "a \"&fn()\" rule specifies an unknown transliterator";
        // parsing error
        case U_UNEXPECTED_TOKEN:
            return "syntax error in format pattern";
        case U_MULTIPLE_DECIMAL_SEPARATORS: // U_MULTIPLE_DECIMAL_SEPERATORS
            return "more than one decimal separator in number pattern";
        case U_MULTIPLE_EXPONENTIAL_SYMBOLS:
            return "more than one exponent symbol in number pattern";
        case U_MALFORMED_EXPONENTIAL_PATTERN:
            return "grouping symbol in exponent pattern";
        case U_MULTIPLE_PERCENT_SYMBOLS:
            return "more than one percent symbol in number pattern";
        case U_MULTIPLE_PERMILL_SYMBOLS:
            return "more than one permill symbol in number pattern";
        case U_MULTIPLE_PAD_SPECIFIERS:
            return "more than one pad symbol in number pattern";
        case U_PATTERN_SYNTAX_ERROR:
            return "syntax error in format pattern";
        case U_ILLEGAL_PAD_POSITION:
            return "pad symbol misplaced in number pattern";
        case U_UNMATCHED_BRACES:
            return "braces do not match in message pattern";
        case U_UNSUPPORTED_PROPERTY:
            return "UNUSED as of ICU 2.4";
        case U_UNSUPPORTED_ATTRIBUTE:
            return "UNUSED as of ICU 2.4";
        case U_ARGUMENT_TYPE_MISMATCH:
            return "argument name and argument index mismatch in MessageFormat functions";
        case U_DUPLICATE_KEYWORD:
            return "duplicate keyword in PluralFormat";
        case U_UNDEFINED_KEYWORD:
            return "undefined Plural keyword";
        case U_DEFAULT_KEYWORD_MISSING:
            return "missing DEFAULT rule in plural rules";
        case U_DECIMAL_NUMBER_SYNTAX_ERROR:
            return "decimal number syntax error";
        // UBreakIterator
        case U_BRK_INTERNAL_ERROR:
            return "an internal error (bug) was detected";
        case U_BRK_HEX_DIGITS_EXPECTED:
            return "hex digits expected as part of a escaped char in a rule";
        case U_BRK_SEMICOLON_EXPECTED:
            return "missing ';' at the end of a RBBI rule";
        case U_BRK_RULE_SYNTAX:
            return "syntax error in RBBI rule";
        case U_BRK_UNCLOSED_SET:
            return "UnicodeSet witing an RBBI rule missing a closing ']'";
        case U_BRK_ASSIGN_ERROR:
            return "syntax error in RBBI rule assignment statement";
        case U_BRK_VARIABLE_REDFINITION:
            return "RBBI rule $Variable redefined";
        case U_BRK_MISMATCHED_PAREN:
            return "mis-matched parentheses in an RBBI rule";
        case U_BRK_NEW_LINE_IN_QUOTED_STRING:
            return "missing closing quote in an RBBI rule";
        case U_BRK_UNDEFINED_VARIABLE:
            return "use of an undefined $Variable in an RBBI rule";
        case U_BRK_INIT_ERROR:
            return "initialization failure.  Probable missing ICU Data";
        case U_BRK_RULE_EMPTY_SET:
            return "rule contains an empty Unicode Set";
        case U_BRK_UNRECOGNIZED_OPTION:
            return "!!option in RBBI rules not recognized";
        case U_BRK_MALFORMED_RULE_TAG:
            return "the {nnn} tag on a rule is mal formed";
        // URegularExpression
        case U_REGEX_INTERNAL_ERROR:
            return "an internal error (bug) was detected";
        case U_REGEX_RULE_SYNTAX:
            return "syntax error in regexp pattern";
        case U_REGEX_INVALID_STATE:
            return "regexMatcher in invalid state for requested operation";
        case U_REGEX_BAD_ESCAPE_SEQUENCE:
            return "unrecognized backslash escape sequence in pattern";
        case U_REGEX_PROPERTY_SYNTAX:
            return "incorrect Unicode property";
        case U_REGEX_UNIMPLEMENTED:
            return "use of regexp feature that is not yet implemented";
        case U_REGEX_MISMATCHED_PAREN:
            return "incorrectly nested parentheses in regexp pattern";
        case U_REGEX_NUMBER_TOO_BIG:
            return "decimal number is too large";
        case U_REGEX_BAD_INTERVAL:
            return "error in {min,max} interval";
        case U_REGEX_MAX_LT_MIN:
            return "in {min,max}, max is less than min";
        case U_REGEX_INVALID_BACK_REF:
            return "back-reference to a non-existent capture group";
        case U_REGEX_INVALID_FLAG:
            return "invalid value for match mode flags";
        case U_REGEX_LOOK_BEHIND_LIMIT:
            return "look-Behind pattern matches must have a bounded maximum length";
        case U_REGEX_SET_CONTAINS_STRING:
            return "regexps cannot have UnicodeSets containing strings";
        case U_REGEX_OCTAL_TOO_BIG:
            return "octal character constants must be <= 0377";
        case U_REGEX_MISSING_CLOSE_BRACKET:
            return "missing closing bracket on a bracket expression";
        case U_REGEX_INVALID_RANGE:
            return "in a character range [x-y], x is greater than y";
        case U_REGEX_STACK_OVERFLOW:
            return "regular expression backtrack stack overflow";
        case U_REGEX_TIME_OUT:
            return "maximum allowed match time exceeded";
        case U_REGEX_STOPPED_BY_CALLER:
            return "matching operation aborted by user callback fn";
        // IDNA
        case U_IDNA_PROHIBITED_ERROR: // U_STRINGPREP_PROHIBITED_ERROR
            return "";
        case U_IDNA_UNASSIGNED_ERROR: // U_STRINGPREP_UNASSIGNED_ERROR
            return "";
        case U_IDNA_CHECK_BIDI_ERROR: // U_STRINGPREP_CHECK_BIDI_ERROR
            return "";
        case U_IDNA_STD3_ASCII_RULES_ERROR:
            return "";
        case U_IDNA_ACE_PREFIX_ERROR:
            return "";
        case U_IDNA_VERIFICATION_ERROR:
            return "";
        case U_IDNA_LABEL_TOO_LONG_ERROR:
            return "";
        case U_IDNA_ZERO_LENGTH_LABEL_ERROR:
            return "";
        case U_IDNA_DOMAIN_NAME_TOO_LONG_ERROR:
            return "";
        // Plugins
        case U_PLUGIN_TOO_HIGH:
            return "the plugin's level is too high to be loaded right now";
        case U_PLUGIN_DIDNT_SET_LEVEL:
            return "the plugin didn't call uplug_setPlugLevel in response to a QUERY";
        default:
            return "bogus error code";
    }
}
