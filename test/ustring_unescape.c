#include "common.h"

int ut(const UChar *string, const UChar *expected) // 0: pass, 1: failed
{
    int ret;
    UString *ustr;

    ustr = ustring_dup_string(string);
    ustring_unescape(ustr);
    ret = u_strCompare(ustr->ptr,  ustr->len, expected, -1, FALSE);
    ustring_destroy(ustr);

    return ret;
}

int main(void)
{
    int ret, r;
    size_t i;
    UChar tests[][2][100] = {
        // 123\uD835\uDE3C456
        {
            { 0x0031, 0x0032, 0x0033, 0x005C, 0x0075, 0x0044, 0x0038, 0x0033, 0x0035, 0x005C, 0x0075, 0x0044, 0x0045, 0x0033, 0x0043, 0x0034, 0x0035, 0x0036, 0 },
            { 0x0031, 0x0032, 0x0033, 0xD835, 0xDE3C, 0x0034, 0x0035, 0x0036, 0 }
        },
        // X\U0001D63DY
        {
            { 0x0058, 0x005C, 0x0055, 0x0030, 0x0030, 0x0030, 0x0031, 0x0044, 0x0036, 0x0033, 0x0044, 0x0059, 0 },
            { 0x0058, 0xD835, 0xDE3D, 0x0059, 0 }
        },
        // \u1D63D
        { { 0x005C, 0x0075, 0x0031, 0x0044, 0x0036, 0x0033, 0x0044, 0 }, { 0x1D63, 0x44, 0 } },
        // \U0000110000
        { { 0x005C, 0x0055, 0x0030, 0x0030, 0x0030, 0x0030, 0x0031, 0x0031, 0x0030, 0x0030, 0x0030, 0x0030, 0 }, { 0x1100, 0x30, 0x30, 0 } },
        // \u000
        { { 0x005C, 0x0075, 0x0030, 0x0030, 0x0030, 0 }, { 0 } },
        // \U1234567
        { { 0x005C, 0x0055, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0 }, { 0 } },
        // \uD835
        { { 0x005C, 0x0075, 0x0044, 0x0038, 0x0033, 0x0035, 0 } , { 0 } },
        // \uDE3C
        { { 0x005C, 0x0075, 0x0044, 0x0045, 0x0033, 0x0043, 0 } , { 0 } },
        // \uD835;\uDE3C
        { { 0x005C, 0x0075, 0x0044, 0x0038, 0x0033, 0x0035, 0x003B, 0x005C, 0x0075, 0x0044, 0x0045, 0x0033, 0x0043, 0 } , { 0x3B, 0 } },
        // \uDE3C\uD835
        { { 0x005C, 0x0075, 0x0044, 0x0045, 0x0033, 0x0043, 0x005C, 0x0075, 0x0044, 0x0038, 0x0033, 0x0035, 0 } , { 0 } },
        // \U0000D835
        { { 0x005C, 0x0055, 0x0030, 0x0030, 0x0030, 0x0030, 0x0044, 0x0038, 0x0033, 0x0035, 0 } , { 0 } },
        // \U0000DE3C
        { { 0x005C, 0x0055, 0x0030, 0x0030, 0x0030, 0x0030, 0x0044, 0x0045, 0x0033, 0x0043, 0 } , { 0 } }
    };

    ret = 0;
    env_init(EXIT_FAILURE);
    env_apply();
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        r = ut(tests[i][0], tests[i][1]);
        printf("Test %d: %s\n", i + 1, r ? RED("KO") : GREEN("OK"));
        ret |= r;
    }

    return (0 == ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
