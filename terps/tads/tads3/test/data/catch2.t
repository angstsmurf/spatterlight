/*
 *   test of catching a run-time exception 
 */

#include "tads.h"
#include "t3.h"

function main(args)
{
    local ret;
    
    test_catch_1();
    test_catch_2();

    ret = test_catch_3(nil);
    "back in _main: test_catch_3(nil) = <<ret>>\n";

    ret = test_catch_3(true);
    "back in _main: test_catch_3(true) = <<ret>>\n";
}

class Exception1: Exception
    construct(msg) { msg_ = msg; }
    msg_ = ''
    display = "This is an Exception1 with message \"<<msg_>>\""
;

modify RuntimeError
    display { tadsSay(exceptionMessage); "\n"; }
;

function test_catch_1()
{
    "test catch 1 - catch our own throw\n";
    try
    {
        "about to throw our error...\n";
        throw new Exception1('hello!');

        "??? can't be here\n";
    }
    catch (Exception1 ex)
    {
        "test catch 1 - caught Exception1: << ex.display >>\n";
    }
    catch (Exception th)
    {
        "test catch 1 - caught Exception: << th.display >>\n";
    }
    finally
    {
        "test catch 1 - in finally\n";
    }
}

function test_catch_2()
{
    "test catch 2 - catching a VM error\n";

    try
    {
        local i;
        
        "about to generate an error...\n";
        i = 1;
        i = i + 'x';

        "??? shouldn't get here!";
    }
    catch (Exception th)
    {
        "test catch 2 - caught Exception: << th.display >>\n";
    }
    finally
    {
        "test catch 2 - in finally\n";
    }
}

function test_catch_3(do_err)
{
    "test catch 3 - catching an error while computing a return value\n";

    try
    {
        "calculating our return value...\n";
        return maybe_gen_err(do_err);
    }
    catch (Exception th)
    {
        "test catch 3 - caught Exception: << th.display >>\n";
        "returning 'error'\n";
        return 'error';
    }
}

function maybe_gen_err(do_err)
{
    "... in maybe_gen_err\n";
    if (do_err)
    {
        local i;

        "... causing a run-time error\n";
        i = 0;
        i = 1/i;
    }

    "... returning 'okay'\n";
    return 'okay';
}

