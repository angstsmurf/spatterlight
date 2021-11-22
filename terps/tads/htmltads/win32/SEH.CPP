#include <Windows.h>
#include <imagehlp.h>
#include <stdio.h>

static LPTOP_LEVEL_EXCEPTION_FILTER G_old_filter;

static LONG WINAPI my_filter(EXCEPTION_POINTERS *info)
{
    CONTEXT *ctx = info->ContextRecord;
    DWORD *ebp;
    int i;
    HANDLE hsnap;
    DWORD baseaddr;

    /* show where we are now */
    printf("CS:EIP = %lx:%lx\n", ctx->SegCs, ctx->Eip);

    /* trace back the stack */
    ebp = (DWORD *)ctx->Ebp;
    for (i = 0 ; i < 50 ; ++i)
    {
        DWORD retaddr;

        /* 
         *   if the enclosing stack frame doesn't look valid, don't
         *   proceed -- each enclosing stack frame should have a higher
         *   stack address than inner ones, because the stack grows
         *   downwards 
         */
        if ((DWORD *)*ebp <= ebp)
            break;

        /* display the return address */
        retaddr = *(ebp + 1);
        printf("         %lx\n", retaddr);

        /* move on to the enclosing frame */
        ebp = (DWORD *)*ebp;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void f5(int x, int *p)
{
    *p = x;
}

static void f4(int x)
{
    int *p;

    p = 0;
    f5(x, p);
}

static void f3(int x)
{
    f4(x + 3);
}

static void f2(int x)
{
    f3(x + 2);
}

static void f1(int x)
{
    f2(x + 1);
}

void main()
{
    int x;
    
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)my_filter);

    x = 1;
    f1(x);
}

