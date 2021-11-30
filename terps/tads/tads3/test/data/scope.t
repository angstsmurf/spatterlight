/*
 *   anonymous functions
 */

#include "tads.h"
#include "t3.h"

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime error: <<exceptionMessage>>"
    errno_ = 0
    exceptionMessage = ''
;

_say_embed(str) { tadsSay(str); }

_main(args)
{
    try
    {
        t3SetSay(&_say_embed);
        main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

main(args)
{
    local i;

    for (i = 1 ; i < 10 ; ++i)
    {
        {
            local j;

            j = i * 10;
            "i = <<i>>, j = <<j>>... ";
        }

        local k;

        if (i == 1)
            k = 123;

        "k = <<k>>\n";
    }
}

